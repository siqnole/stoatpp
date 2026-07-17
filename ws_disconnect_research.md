# Bronx-Stoat WebSocket 1006 Disconnect Loop — Research

> **Date:** 2026-07-17  
> **Component:** stoat++ gateway / bronx-stoat bot  
> **Status:** Active — bot is in a constant reconnect loop  

---

## Symptom

The bronx-stoat bot enters an infinite WebSocket reconnect loop with close code **1006 (Abnormal Closure)**. The cycle is:

```
Connect → Authenticate → Authenticated → Ready → ~10s passes → 1006 Abnormal Closure → Reconnect → repeat
```

The loop is extremely consistent, with disconnects occurring **exactly ~10 seconds** after authentication succeeds, every single cycle.

### Live Log Evidence

```
[11:49:16.184] WebSocket closed. Code: 1006, Reason: Abnormal closure
[11:49:16.184] WebSocket connection closed.
[11:49:16.791] WebSocket connection established.
[11:49:16.791] Sending Authenticate event...
[11:49:26.961] WebSocket closed. Code: 1006, Reason: Abnormal closure   ← exactly 10s later
[11:49:26.961] WebSocket connection closed.
[11:49:27.551] WebSocket connection established.
[11:49:37.712] WebSocket closed. Code: 1006, Reason: Abnormal closure   ← exactly 10s later
```

Key observations:
- The bot **successfully authenticates** every cycle
- The bot **receives the full Ready payload** (233 servers, 2703 channels, 196 users)
- The bot **never sends a single heartbeat** before the connection drops
- No `WebSocket Error` events are logged — only the clean `Close` event with code 1006
- The ixwebsocket auto-reconnect fires immediately after each close

---

## Root Cause Analysis

### Primary Cause: Server-Side Idle Timeout (Stoat/Revolt Gateway)

The Stoat gateway (based on Revolt's `bonfire` WebSocket service) enforces a server-side idle timeout. When a client doesn't send any traffic (specifically an application-level `Ping` event) within this window after the connection is established, the server kills the connection.

**The timeline proves this:**

| Event | Timestamp | Delta |
|---|---|---|
| Authenticated | `11:49:16.791` | — |
| Ready received | immediately after | — |
| **Connection closed** | `11:49:26.961` | **~10.17s** |
| First ping would fire | (never reached) | would have been at `11:49:26.791` (10s interval) |

The bot's configured heartbeat interval (`ws_ping_interval_ms = 10000`) means the first `Ping` event is scheduled to fire **10 seconds after** `schedule_ping()` is called. But the server drops the connection at approximately the same time or just before, creating a race condition where the ping never actually gets sent.

### Why This Worked Before

This bot previously worked fine. The most likely explanation is:

1. **The Stoat server tightened its idle timeout** — this could have changed during a server update/restart. If the timeout was previously 15-30 seconds and was changed to 10 seconds, the 10-second heartbeat interval would now be insufficient.

2. **The Ready payload processing time contributes** — with 233 servers and 2703 channels, the Ready payload is massive. The `on_ready` handler iterates through all 196 users to detect bots, prints them all out, and detaches a thread to fix DB flags. While this processing happens synchronously on the event worker thread, it doesn't block the WebSocket's ability to send pings — BUT the `schedule_ping()` call only fires after the `Authenticated` event is processed, and the first actual ping send only happens after waiting the full `ws_ping_interval_ms`.

3. **ixwebsocket's default behavior** — ixwebsocket has its own WebSocket-level ping feature (`setPingInterval`/`setPingTimeout`) which is **disabled by default** (`setPingInterval(-1)`). The stoat++ gateway code does **not** configure ixwebsocket's built-in ping mechanism and instead relies entirely on application-level `Ping` events. This means there are **zero WebSocket-level keepalive frames** being sent.

### Contributing Factors

| Factor | Status | Impact |
|---|---|---|
| `ws_ping_interval_ms = 10000` | Too slow | First ping fires at exactly the timeout boundary |
| No immediate ping after auth | Missing | Gateway never sends an immediate ping right after authenticating |
| ixwebsocket setPingInterval not used | Not configured | No WebSocket-level keepalive frames |
| No exponential backoff on reconnect | Uses ixwebsocket default | Reconnects too fast, may appear like abuse |
| Ready payload is massive | 233 servers, 2703 channels | Adds processing delay before first potential ping |
| Server-side timeout | ~10 seconds (estimated) | Likely recently tightened |

---

## Code Flow Analysis

### gateway.cpp — The Heartbeat Lifecycle

```
on_connected()
  └─ if (!ws_auth_in_url) → authenticate(token)     // sends Authenticate
  
handle_event("Authenticated")
  └─ authenticated = true
  └─ schedule_ping()                                  // starts heartbeat thread

schedule_ping()
  └─ thread: while(running) {
       wait_for(ws_ping_interval_ms)                  // WAITS 10 seconds first
       if (connected && authenticated) ping(now_ms)   // only THEN sends first ping
     }
```

**The critical gap:** `schedule_ping()` uses `wait_for(interval)` as its first operation. This means the first heartbeat is delayed by the full interval duration. The server has already closed the connection by the time the first ping would fire.

### gateway.cpp — The Reconnect Flow

```
on_disconnected()
  └─ authenticated = false
  └─ stops ping thread (joins or detaches)
  └─ dispatcher_.dispatch_logout({})
  
ixwebsocket auto-reconnect (enabled on line 77)
  └─ reconnects with exponential backoff (capped at 10s by default)
  └─ triggers on_connected() → authenticate → Authenticated → schedule_ping → cycle repeats
```

---

## Confirming It's Server-Side, Not Client-Side

Evidence that the **server** is dropping the connection, not the client:

1. **Close code 1006** — this means the connection was terminated without a proper WebSocket close handshake. The server just dropped the TCP connection.
2. **No `WebSocket Error` events** — if the client was crashing or timing out internally, ixwebsocket would emit an Error event before the Close.
3. **The REST API works fine** — `GET /users/@me` succeeds, `GET /` returns 200 with server info (Revolt 0.14.0). The token is valid and the API is healthy.
4. **Authentication succeeds** — the server responds with `Authenticated` and `Ready` every time, proving the token is valid.
5. **Consistent 10-second timing** — this is a server-enforced timeout, not random network failure.

---

## Recommended Fixes

### Fix 1 (Immediate): Send a Ping Right After Authentication

The simplest fix: send an immediate `Ping` event as soon as `Authenticated` is received, before waiting for the heartbeat timer.

**In `gateway.cpp`, modify `handle_event` for the `Authenticated` case:**

```cpp
if (type == "Authenticated") {
    utils::logger::log(LogLevel::INFO, "Handshake completed. Authenticated successfully.", config_);
    pimpl_->authenticated = true;
    
    // Send an immediate ping to prevent server-side idle timeout
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    ping(now_ms);
    
    schedule_ping();
    return;
}
```

### Fix 2 (Recommended): Reduce Heartbeat Interval

The `ws_ping_interval_ms` of 10000 (10s) is too close to the server's idle timeout. Reduce it to a safer value.

**In `bronx/main.cpp`, change line 228:**

```cpp
cfg.ws_ping_interval_ms = 7000;  // was 10000, now well under the ~10s server timeout
```

The Revolt/Stoat docs recommend 10-30 seconds, but if the server timeout is 10s, you need to ping MORE frequently than that.

### Fix 3 (Belt-and-Suspenders): Enable ixwebsocket Built-in Ping

Configure ixwebsocket's own WebSocket-level ping/pong mechanism as a backup keepalive.

**In `gateway.cpp` constructor, after line 77:**

```cpp
pimpl_->ws.enableAutomaticReconnection();
pimpl_->ws.setPingInterval(5);    // send WS-level ping every 5 seconds
```

This sends WebSocket protocol-level ping frames (not Stoat application-level pings), which some servers will count as "traffic" and reset their idle timers.

### Fix 4 (Defense-in-Depth): Modify schedule_ping to Send First Ping Immediately

Change the heartbeat thread to send a ping **immediately** upon starting, then wait for the interval between subsequent pings.

**In `gateway.cpp`, modify `schedule_ping()`:**

```cpp
void gateway::schedule_ping() {
    if (pimpl_->ping_thread_running) return;
    
    pimpl_->ping_thread_running = true;
    pimpl_->ping_thread = std::thread([this]() {
        utils::logger::log(LogLevel::DEBUG, "Starting Gateway heartbeat thread.", config_);
        
        // Send first ping immediately
        if (is_connected() && pimpl_->authenticated) {
            auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            ping(now_ms);
        }
        
        while (pimpl_->ping_thread_running) {
            std::unique_lock<std::mutex> lock(pimpl_->ping_mutex);
            if (pimpl_->ping_cv.wait_for(lock, std::chrono::milliseconds(config_.ws_ping_interval_ms), [this]() {
                return !pimpl_->ping_thread_running;
            })) {
                break;
            }
            
            if (is_connected() && pimpl_->authenticated) {
                auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                ping(now_ms);
            }
        }
        utils::logger::log(LogLevel::DEBUG, "Gateway heartbeat thread exiting.", config_);
    });
}
```

### Fix 5 (Diagnostic): Test with Verbose Logging

Run the bot with `--verbose` to enable raw event logging to confirm what the server sends right before disconnect:

```bash
./bronx --verbose
```

This will enable the `on_raw_event` handler that logs all events, which could reveal if the server sends a `Ping` event that isn't being responded to in time.

---

## Is This a Stoat Server-Side Issue?

**Possibly yes.** There are two scenarios:

### Scenario A: Server Changed Timeout (Server Issue)
If the Stoat server recently reduced its idle timeout from ~15-30s to ~10s, then bots that were previously working with 10-15s heartbeat intervals would suddenly start failing. This would be a server-side change that affected all clients.

### Scenario B: Bot Was Always Borderline (Client Issue)
The 10-second heartbeat interval was always marginal. If the server timeout was always 10s, the bot was in a race condition where sometimes the first ping made it in time and sometimes it didn't. Recent factors (increased Ready payload size from joining more servers, network latency, server load) could have tipped it over.

**Evidence suggests Scenario A is more likely**, because:
- The logs show the bot worked fine as recently as 2026-06-29 (last clean session in logs)
- The disconnect timing is incredibly consistent (always ~10s, never ~9s or ~11s)
- The bot successfully worked for a long period before this started

### Recommendation
1. Apply Fixes 1 + 2 + 4 first (immediate ping after auth, reduce interval to 7s, immediate first ping in heartbeat thread)
2. If that doesn't resolve it, apply Fix 3 (ixwebsocket-level pings)
3. Check with the Stoat server admin whether the gateway timeout was recently changed
4. Monitor the `/` endpoint for server version changes that might indicate updates

---

## Infrastructure Notes

| Component | Status |
|---|---|
| `api.stoat.chat` | ✅ Responding (200, Revolt 0.14.0) |
| `events.stoat.chat` | ⚠️ Connects but drops after ~10s |
| Bot token | ✅ Valid (authenticates successfully) |
| `bronx-stoat` container | ⚠️ Running but in reconnect loop |
| `stoatchat-main_redis_1` | ❌ Exited |
| `stoatchat-main_database_1` | ❌ Exited |
| `stoatchat-main_rabbit_1` | ❌ Exited |
| `bpp-db` (bronx database) | ✅ Up 12 min |

> **Note:** The Stoat infrastructure containers (redis, database, rabbit) being down are for a **local self-hosted Stoat instance** and are not related to the `api.stoat.chat` / `events.stoat.chat` production servers that bronx connects to.

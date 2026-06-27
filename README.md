# stoat++

a simple c++17 library for [stoat.chat](https://stoat.chat).

## requirements

- c++17 compiler
- cmake (3.16+)
- git
- openssl

## installation

add it as a subdirectory in your cmake project:

```bash
git clone https://codeberg.org/siq/stoatplusplus.git stoatpp
```

then add to your `CMakeLists.txt`:
```cmake
add_subdirectory(stoatpp)
target_link_libraries(your_target PRIVATE stoatpp)
```

---

## setup with msys2 (windows)

open the **ucrt64** or **mingw64** shell (do not use the msys shell) and run:

```bash
# for ucrt64
pacman -S git mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-openssl

# for mingw64
pacman -S git mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-openssl
```

build locally:
```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
```

---

## setup with linux (debian/ubuntu)

```bash
sudo apt update
sudo apt install build-essential cmake git libssl-dev

mkdir build && cd build
cmake ..
cmake --build .
```

---

## simple example

```cpp
#include <stoatpp/stoatpp.h>
#include <iostream>

int main() {
    stoatpp::ClientConfig cfg;
    cfg.log_level = stoatpp::LogLevel::INFO;

    stoatpp::cluster bot("YOUR_BOT_TOKEN_HERE", cfg);

    bot.on_ready([](const stoatpp::events::Ready& event) {
        std::cout << "logged in as " << event.user.username << "\n";
    });

    bot.on_message([&bot](const stoatpp::events::Message& event) {
        if (event.author.bot) return;

        if (event.content == "!ping") {
            bot.send_message(event.channel_id, "pong!");
        }
    });

    bot.start();
    return 0;
}
```

see [docs.md](file:///home/siq/stoat++/docs.md) for the full api.

## for updates
join [the server](https://stt.gg/mXREEgsz).
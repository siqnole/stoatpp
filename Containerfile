# Multi-stage Containerfile for stoat++ / bronx bot

# --- Build Stage ---
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    cmake \
    make \
    g++ \
    libmariadb-dev \
    libtesseract-dev \
    libleptonica-dev \
    libcairo2-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    libgif-dev \
    libjpeg-dev \
    libpng-dev \
    libtiff-dev \
    libwebp-dev \
    libpulse-dev \
    pkg-config \
    git \
    curl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src/stoatpp

# Copy workspace
COPY . .

# Build the bot (rm -rf build avoids using host CMakeCache.txt)
WORKDIR /usr/src/stoatpp/bronx
RUN rm -rf build CMakeCache.txt CMakeFiles/ && mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release -DSTOATPP_BUILD_CLIENT=OFF -DSTOATPP_BUILD_EXAMPLES=OFF .. && \
    make -j$(nproc)

# --- Runtime Stage ---
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    libmariadb3 \
    libtesseract5 \
    liblept5 \
    libcairo2 \
    libssl3 \
    libcurl4 \
    libgif7 \
    libjpeg-turbo8 \
    libpng16-16 \
    libtiff6 \
    libwebp7 \
    libpulse0 \
    ca-certificates \
    curl \
    ffmpeg \
    python3 \
    && curl -L https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp -o /usr/local/bin/yt-dlp \
    && chmod a+rx /usr/local/bin/yt-dlp \
    && mkdir -p /var/lib/mysql /run/mariadb \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy LiveKit shared libraries and executable
COPY --from=builder /usr/src/stoatpp/bronx/build/_deps/livekit-sdk/livekit-sdk-ubuntu-24.04-x64-1.13.1/lib/ /usr/local/lib/
RUN ldconfig
COPY --from=builder /usr/src/stoatpp/bronx/build/bronx /app/bronx
COPY bronx/.env /app/.env

# Default command to run the bot
CMD ["./bronx"]

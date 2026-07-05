# Multi-stage Containerfile for stoat++ / bronx bot

# --- Build Stage ---
FROM fedora:44 AS builder

# Install build dependencies
RUN dnf install -y \
    cmake \
    make \
    gcc-c++ \
    mariadb-connector-c-devel \
    tesseract-devel \
    leptonica-devel \
    cairo-devel \
    openssl-devel \
    libcurl-devel \
    giflib-devel \
    libjpeg-turbo-devel \
    libpng-devel \
    libtiff-devel \
    libwebp-devel \
    pkgconfig \
    git \
    && dnf clean all

WORKDIR /usr/src/stoatpp

# Copy workspace
COPY . .

# Build the bot (rm -rf build avoids using host CMakeCache.txt)
WORKDIR /usr/src/stoatpp/bronx
RUN rm -rf build && mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j$(nproc)

# --- Runtime Stage ---
FROM fedora:44

# Install runtime dependencies
RUN dnf install -y \
    mariadb-connector-c \
    tesseract \
    leptonica \
    cairo \
    openssl \
    libcurl \
    giflib \
    libjpeg-turbo \
    libpng \
    libtiff \
    libwebp \
    && dnf clean all

WORKDIR /app

# Copy executable and configuration
COPY --from=builder /usr/src/stoatpp/bronx/build/bronx /app/bronx
COPY bronx/.env /app/.env

# Default command to run the bot
CMD ["./bronx"]

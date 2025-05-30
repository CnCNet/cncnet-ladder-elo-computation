# -------- Build Stage --------
FROM ubuntu:22.04 AS build

# Install build dependencies
RUN apt-get update && apt-get install -y \
    ca-certificates \
    git \
    cmake \
    ninja-build \
    libmysqlcppconn-dev \
    g++ \
    build-essential

COPY . /app

WORKDIR /app/build

# Build your application
RUN cmake -G Ninja /app && ninja -j $(( $(nproc) / 2 ))

# -------- Runtime Stage --------
FROM debian:bookworm-slim

# Install only runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libmysqlcppconn-dev && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# Set workdir and copy the built binary
WORKDIR /app
COPY --from=build /app/build/elogen /app/elogen

# Run it
ENTRYPOINT ["/app/elogen"]

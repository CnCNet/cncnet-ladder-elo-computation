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

# Copy your source code
WORKDIR /app
COPY . .

RUN mkdir build && cd build

# Build your application (adjust this to your project)
RUN cmake -G Ninja /app && ninja -j $(( $(nproc) / 2 ))

# -------- Runtime Stage --------
FROM alpine:latest

# Install runtime dependencies
COPY --from=build /usr/lib/x86_64-linux-gnu/libmysqlcppconn.so.7 /usr/lib/libmysqlcppconn.so.7

# Copy the built binary from the build stage
COPY --from=build /app/build/elogen /usr/local/bin/elogen

RUN chmod +x /usr/local/bin/elogen

RUN ln -s /usr/lib/libmysqlcppconn.so.7 /usr/lib/libmysqlcppconn.so

# Run it
ENTRYPOINT ["/usr/local/bin/elogen"]

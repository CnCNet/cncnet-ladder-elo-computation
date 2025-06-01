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
FROM debian:bookworm-slim AS app

# Install only runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libmysqlcppconn-dev && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# Set workdir and copy the built binary
WORKDIR /app
COPY --from=build /app/build/elogen /app/elogen

# Run it
ENTRYPOINT ["/app/elogen"]


# -------- CRON Stage --------
FROM app AS cron

# Create non-root user and group
ARG UID=1001
ARG GID=1001
ARG USER=cncnet
RUN groupadd -g $GID $USER && \
    useradd -u $UID -g $GID -m -s /bin/bash $USER

# Install cron
RUN apt-get update && apt-get install -y cron && rm -rf /var/lib/apt/lists/*

# Switch back to root to run cron
USER root

# Start cron in the foreground
CMD ["cron", "-f"]
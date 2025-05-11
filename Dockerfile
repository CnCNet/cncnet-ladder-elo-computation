FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    ca-certificates \
    build-essential \
    git \
    cmake \
    ninja-build \
    libmysqlcppconn-dev \
    && rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/CnCNet/cncnet-ladder-elo-computation.git && \
    mkdir build

WORKDIR /build

ENTRYPOINT ["/bin/bash", "-c", "cd /cncnet-ladder-elo-computation && git checkout test && git pull && cd /build && cmake -G Ninja /cncnet-ladder-elo-computation && ninja -j $(( $(nproc) / 2 )) && ./elogen"]

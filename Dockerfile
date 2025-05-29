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

RUN git clone https://github.com/CnCNet/cncnet-ladder-elo-computation.git /repos/cncnet-ladder-elo-computation && \
    mkdir build && \
    cd build && \
    cmake -G Ninja /repos/cncnet-ladder-elo-computation && \
    ninja -j $(( $(nproc) / 2 ))

WORKDIR /build

FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    software-properties-common \
    && add-apt-repository ppa:ubuntu-toolchain-r/test -y \
    && apt-get update && apt-get install -y \
    build-essential \
    gcc-14 \
    g++-14 \
    cmake \
    git \
    wget \
    curl \
    pkg-config \
    libssl-dev \
    libasio-dev \
    libsasl2-dev \
    libzstd-dev \
    ca-certificates \
    python3 \
    && rm -rf /var/lib/apt/lists/*

ENV CC=gcc-14
ENV CXX=g++-14

WORKDIR /opt

RUN wget https://github.com/mongodb/mongo-c-driver/archive/refs/tags/1.29.0.tar.gz -O mongo-c-driver.tar.gz && \
    tar -xzf mongo-c-driver.tar.gz && \
    cd mongo-c-driver-1.29.0 && \
    cmake -S . -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF && \
    cmake --build build -j$(nproc) && \
    cmake --install build && \
    cd /opt && rm -rf mongo-c-driver*

RUN wget https://github.com/mongodb/mongo-cxx-driver/archive/refs/tags/r4.1.1.tar.gz -O mongo-cxx-driver.tar.gz && \
    tar -xzf mongo-cxx-driver.tar.gz && \
    cd mongo-cxx-driver-r4.1.1 && \
    cmake -S . -B build \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_STANDARD=23 \
      -DCMAKE_CXX_STANDARD_REQUIRED=ON \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DBUILD_VERSION=4.1.1 \
      -Dmongo-c-driver_ROOT=/usr/local && \
    cmake --build build -j$(nproc) && \
    cmake --install build && \
    cd /opt && rm -rf mongo-cxx-driver*



RUN git clone https://github.com/CrowCpp/Crow.git /opt/crow && \
    cmake -S /opt/crow -B /opt/crow/build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=23 \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DCROW_BUILD_EXAMPLES=OFF \
        -DCROW_BUILD_TESTS=OFF && \
    cmake --install /opt/crow/build

WORKDIR /app
COPY . .

RUN cmake -S . -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_STANDARD=23 \
        -DCMAKE_CXX_STANDARD_REQUIRED=ON \
        -DCMAKE_PREFIX_PATH=/usr/local && \
    cmake --build build -j$(nproc)

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    libssl3 \
    libsasl2-2 \
    libzstd1 \
    ca-certificates \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /usr/local /usr/local
COPY --from=builder /app/build/src/server/findthebug-server /usr/local/bin/findthebug-server

RUN ldconfig

RUN useradd -m appuser
USER appuser
WORKDIR /home/appuser

EXPOSE 8080

CMD ["/usr/local/bin/findthebug-server"]

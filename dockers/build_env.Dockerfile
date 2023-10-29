FROM ubuntu:18.04 as build

RUN apt-get update && apt-get install -y \
  autoconf \
  automake \
  build-essential \
  cmake \
  curl \
  g++ \
  git \
  libtool \
  make \
  pkg-config \
  unzip \
  && apt-get clean

ENV GRPC_RELEASE_TAG v1.58.0
RUN mkdir /grpc/
RUN cd /grpc/ && git clone --recurse-submodules -b v1.58.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc
RUN mkdir /grpcbin/
RUN apt install -y wget
RUN wget https://cmake.org/files/v3.22/cmake-3.22.0-linux-aarch64.tar.gz
RUN tar xvf cmake-3.22.0-linux-aarch64.tar.gz
ENV PATH="/cmake-3.22.0-linux-aarch64/bin:/grpcbin/bin:${PATH}"
RUN echo "-- installing grpc" && \
    cd /grpc/grpc && \
    mkdir -p cmake/build && \
    cd cmake/build && \
    cmake -DgRPC_INSTALL=ON \
          -DgRPC_BUILD_TESTS=OFF \
          -DCMAKE_INSTALL_PREFIX=/grpcbin/ \
          ../.. && \
    make -j$(nproc) && make install && make clean && ldconfig

RUN apt update && apt install -y libsystemd-dev
WORKDIR /app





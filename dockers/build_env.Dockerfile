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
RUN export ARCH=$(uname -m) && wget https://cmake.org/files/v3.22/cmake-3.22.0-linux-${ARCH}.tar.gz && \
    tar xvf cmake-3.22.0-linux-${ARCH}.tar.gz && \
    mv cmake-3.22.0-linux-${ARCH} cmake && \
    rm cmake-3.22.0-linux-${ARCH}.tar.gz
ENV PATH="/cmake/bin:/grpcbin/bin:${PATH}"
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





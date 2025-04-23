FROM hub.byted.org/compile/milvus-base:ubuntu20.04

WORKDIR /workspace

RUN apt-get update && \
    apt-get install -y curl ca-certificates libaio-dev libgomp1 libopenblas-dev && \
    apt-get install bison

COPY . .

# RUN conan install ./internal/core --build=missing
RUN bash -x ./scripts/3rdparty_build.sh
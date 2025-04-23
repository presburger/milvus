#FROM hub.byted.org/compile/milvus-base:v6.3.3.2 as builder

#FROM hub.byted.org/compile/milvus-base:ubuntu20.04 as builder
FROM hub.byted.org/compile/milvus-base:ubuntu20.04-conan as builder

WORKDIR /workspace

COPY . .

RUN mv /cmake_build /workspace/cmake_build

RUN apt-get update && \
    apt-get install -y --no-install-recommends curl ca-certificates libaio-dev libgomp1 libopenblas-dev
ENV SKIP_3RDPARTY=1

RUN git config --global http.sslVerify false

RUN echo "check old proxy" \
    echo "http_proxy=$http_proxy"   # 查看HTTP代理 \
    echo "https_proxy=$https_proxy"  # 查看HTTPS代理 \
    echo "$no_proxy=$no_proxy "    # 查看排除代理的地址列表:ml-citation{ref="8" data="citationList"}

ENV http_proxy="http://sys-proxy-rd-relay.byted.org:8118" https_proxy="http://sys-proxy-rd-relay.byted.org:8118"

RUN echo "check new proxy" \
    echo "http_proxy=$http_proxy"   # 查看HTTP代理 \
    echo "https_proxy=$https_proxy"  # 查看HTTPS代理 \
    echo "$no_proxy=$no_proxy "    # 查看排除代理的地址列表:ml-citation{ref="8" data="citationList"}

RUN echo "debug info" \
    echo "GO version $(go version)" \
    echo "GOPATH: ${GOPATH}" \
    echo "GOROOT: ${GOROOT}"

#export GOPATH=$GOPATH
#export PATH=$GOPATH/bin:$PATH
#export GOROOT=$GOROOT
#export PATH=$GOROOT/bin:$PATH
ENV GOPROXY="https://goproxy.byted.org|https://goproxy.cn|direct"
ENV GOPRIVATE="*.byted.org,*.everphoto.cn,git.smartisan.com"
ENV GOSUMDB="sum.golang.google.cn"
ENV PATH=$PATH:$GOPATH/bin


RUN make milvus


FROM hub.byted.org/ubuntu:focal-20240530

ARG TARGETARCH
ARG MILVUS_ASAN_LIB

RUN apt-get update && \
    apt-get install -y --no-install-recommends curl ca-certificates libaio-dev libgomp1 libopenblas-dev && \
    apt-get remove --purge -y && \
    rm -rf /var/lib/apt/lists/*

# Add Tini
#RUN curl -L -o /tini https://github.com/krallin/tini/releases/download/v0.19.0/tini-$TARGETARCH && \
#    chmod +x /tini

RUN mkdir -p /milvus/bin


COPY --from=builder --chown=root:root --chmod=774 ./bin/milvus /milvus/bin/milvus

COPY --chown=root:root --chmod=774 ./configs/ /milvus/configs/

COPY --from=builder --chown=root:root --chmod=774 ./lib/ /milvus/lib/

ENV PATH=/milvus/bin:$PATH
ENV LD_LIBRARY_PATH=/milvus/lib:$LD_LIBRARY_PATH:/usr/lib
ENV LD_PRELOAD=${MILVUS_ASAN_LIB}:/milvus/lib/libjemalloc.so
ENV MALLOC_CONF=background_thread:true

#ENTRYPOINT ["/tini", "--"]

WORKDIR /milvus/

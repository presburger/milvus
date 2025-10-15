# Copyright (C) 2019-2022 Zilliz. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance
# with the License. You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under the License
# is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
# or implied. See the License for the specific language governing permissions and limitations under the License.

ARG REGION

FROM ubuntu:jammy-20240530 as us-east

FROM ubuntu:jammy-20240530 as us-ttp

FROM ubuntu:jammy-20240530 as us-east-red

FROM ubuntu:jammy-20240530 as aliyun_va

FROM ubuntu:jammy-20240530 as aliyun_sg

FROM hub.byted.org/ubuntu:jammy-20240530 as china-north-lf

FROM ${REGION}

# REGION 要想在FROM之后使用，必须再次声明ARG
ARG REGION
ARG TARGETARCH
ARG MILVUS_ASAN_LIB

RUN apt-get update && \
    apt-get install -y --no-install-recommends systemd tzdata wget curl ca-certificates libaio-dev libgomp1 libopenblas-dev && \
    apt-get remove --purge -y && \
    rm -rf /var/lib/apt/lists/*

RUN if [ "$REGION" = "china-north-lf" ]; then \
      TZ=Asia/Shanghai && \
      ln -fs /usr/share/zoneinfo/$TZ /etc/localtime && \
      echo $TZ > /etc/timezone; \
    fi

# Add Tini
RUN curl -L -o /tini https://github.com/krallin/tini/releases/download/v0.19.0/tini-$TARGETARCH && \
    chmod +x /tini

RUN mkdir -p /milvus/

ENV PATH=/milvus/bin:$PATH
ENV LD_LIBRARY_PATH=/milvus/lib:$LD_LIBRARY_PATH:/usr/lib
ENV LD_PRELOAD=${MILVUS_ASAN_LIB}:/milvus/lib/libjemalloc.so
ENV MALLOC_CONF=background_thread:true

# 创建用户 tiger 及相关目录
RUN useradd -m tiger && \
    mkdir -p /opt/tiger /var/log/tiger /opt/log/tiger && \
    chown -R tiger:tiger /opt/tiger /var/log/tiger /opt/log/tiger

ENTRYPOINT ["/tini", "--"]

WORKDIR /milvus/

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
FROM hub.byted.org/ubuntu:focal-20240530

ARG TARGETARCH
ARG MILVUS_ASAN_LIB

ENV TZ=Asia/Shanghai
RUN apt-get update && \
    apt-get install -y tzdata && \
    ln -fs /usr/share/zoneinfo/$TZ /etc/localtime && \
    echo $TZ > /etc/timezone


RUN apt-get install -y --no-install-recommends curl wget ca-certificates libaio-dev libgomp1 libopenblas-dev systemd liblapacke-dev && \
    apt-get remove --purge -y && \
    rm -rf /var/lib/apt/lists/*


# 创建用户 tiger
RUN useradd -m tiger

# 设置用户 tiger 的密码（可选，为了安全起见，建议使用更安全的密码设置方法）
# 可以通过 docker-compose.yml 中的 secrets 或者 Docker 的 secrets 功能来管理密码
# RUN echo 'tiger:tigerpassword' | chpasswd

# 创建目录 /opt/tiger
RUN mkdir -p /opt/tiger
RUN chown -R tiger:tiger /opt/tiger

# 创建目录 /var/log/tiger
RUN mkdir -p /var/log/tiger
RUN chown -R tiger:tiger /var/log/tiger

# 创建目录 /opt/log/tiger
RUN mkdir -p /opt/log/tiger
RUN chown -R tiger:tiger /opt/log/tiger

# Add Tini
RUN curl -L -o /tini https://github.com/krallin/tini/releases/download/v0.19.0/tini-$TARGETARCH && \
    chmod +x /tini

RUN mkdir -p /milvus/

ENV PATH=/milvus/bin:$PATH
ENV LD_LIBRARY_PATH=/milvus/lib:$LD_LIBRARY_PATH:/usr/lib
ENV LD_PRELOAD=${MILVUS_ASAN_LIB}:/milvus/lib/libjemalloc.so
ENV MALLOC_CONF=background_thread:true


RUN echo "run version:ubuntu20.04-conan" > version.txt

ENTRYPOINT ["/tini", "--"]

WORKDIR /milvus/

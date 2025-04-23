#!/bin/bash

# install dep
#bash -x ./scripts/install_deps_scm.sh
#
##
#echo "install conan 1.64.1"
#pip3 install conan==1.64.1  --break-system-packages

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
echo "SCRIPT_DIR: {SCRIPT_DIR}"


#if [ ! -d "./cmake_build" ]; then
#  mv /cmake_build ./cmake_build
#fi
#export SKIP_3RDPARTY=1

export ENABLE_GCP_NATIVE=ON

#apt-get update
#apt-get install -y --no-install-recommends curl ca-certificates libaio-dev libgomp1 libopenblas-dev
#update-ca-certificates

export GOROOT=/usr/local/go
export GOPATH=/go
export PATH=$GOPATH/bin:$PATH
export GOPROXY="https://goproxy.byted.org|https://goproxy.cn|direct"
export GOPRIVATE="*.byted.org,*.everphoto.cn,git.smartisan.com"
export GOSUMDB="sum.golang.google.cn"
echo "debug info"
echo "GO version $(go version)"
echo "GOPATH: ${GOPATH}"
echo "GOROOT: ${GOROOT}"

DEFAULT_RUN_LOCAL_CANAN="false"
RUN_LOCAL_CANAN_FLAG=${RUN_LOCAL_CANAN_FLAG:-$DEFAULT_RUN_LOCAL_CANAN}
echo "RUN_LOCAL_CANAN_FLAG: ${RUN_LOCAL_CANAN_FLAG}"

# check
if [ -n "$RUN_LOCAL_CANAN_FLAG" ] && [ "$RUN_LOCAL_CANAN_FLAG" = "true" ]; then
    bash -x $SCRIPT_DIR/scm_builder/run_local_canan.sh
    echo "run local canan"
fi

## 下载并安装 Conan 官方维护的 CA 证书
conan config install https://github.com/conan-io/conanclientcert.git
export CONAN_VERIFY_SSL=False
conan config set general.verify_ssl=False

echo "print build environment infos"
echo "cmake version: $(cmake --version)"

export MAKEFLAGS="-j2"
export CMAKE_BUILD_PARALLEL_LEVEL=2
echo "build milvus"
make -j2 build-cpp
# print-build-info build-go
make print-build-info
make build-go

mkdir -p ./output/milvus
cp -rf ./configs/ ./output/milvus/configs/
cp -rf ./internal/core/output/* ./output/milvus/
mkdir -p ./output/milvus/bin
cp ./bin/milvus ./output/milvus/bin/milvus

echo "build ok"

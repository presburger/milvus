#!/bin/bash

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
echo "SCRIPT_DIR: {SCRIPT_DIR}"
echo "Build with user: $(whoami)"
echo "Build with Env: $(env)"

export ENABLE_GCP_NATIVE=${CUSTOM_ENABLE_GCP_NATIVE:-"ON"}
export CONAN_USER_HOME="/home/milvus"

# 批量处理CUSTOM_GO前缀的环境变量
for var in $(compgen -e | grep '^CUSTOM_GO'); do
    export GO${var#CUSTOM_GO}="${!var}"
done
export PATH=$GOPATH/bin:$PATH

for var in $(compgen -e | grep '^CUSTOM_SET_'); do
    export ${var#CUSTOM_SET_}="${!var}"
done

# 批量处理CUSTOM_UNSET_前缀的环境变量
for var in $(compgen -e | grep '^CUSTOM_UNSET_'); do
    unset ${var#CUSTOM_UNSET_}
done

export CCACHE_COMPILERCHECK="content"
export CCACHE_COMPRESS=1
export CCACHE_COMPRESSLEVEL=5
export CCACHE_MAXSIZE="2G"
export CCACHE_DIR="/ccache"
echo "GO version $(go version)"
echo "GO Env: $(go env)"
echo "cmake version: $(cmake --version)"
echo "conan version: $(conan --version)"
echo "cargo version: $(cargo --version)"

# Run local conan server
DEFAULT_RUN_LOCAL_CANAN="false"
RUN_LOCAL_CANAN_FLAG=${RUN_LOCAL_CANAN_FLAG:-$DEFAULT_RUN_LOCAL_CANAN}
echo "RUN_LOCAL_CANAN_FLAG: ${RUN_LOCAL_CANAN_FLAG}"
if [ -n "$RUN_LOCAL_CANAN_FLAG" ] && [ "$RUN_LOCAL_CANAN_FLAG" = "true" ]; then
    bash -x $SCRIPT_DIR/scm_builder/run_local_canan.sh
    echo "run local conan server ok"
fi

## 下载并安装 Conan 官方维护的 CA 证书
conan config install https://github.com/conan-io/conanclientcert.git
export CONAN_VERIFY_SSL=False
conan config set general.verify_ssl=False

## Building...

DEFAULT_PARALLEL_LEVEL=8
CUSTOM_PARALLEL_LEVEL=${CUSTOM_PARALLEL_LEVEL:-$DEFAULT_PARALLEL_LEVEL}
echo "CUSTOM_PARALLEL_LEVEL: ${CUSTOM_PARALLEL_LEVEL}"
#export CMAKE_BUILD_PARALLEL_LEVEL=${CUSTOM_PARALLEL_LEVEL}
#export MAKEFLAGS="-j${CUSTOM_PARALLEL_LEVEL}"
# for core_build.sh
export jobs=${CUSTOM_PARALLEL_LEVEL}
#export CMAKE_EXTRA_ARGS="-j${CUSTOM_PARALLEL_LEVEL}"
#export CONAN_CPU_COUNT=${CUSTOM_PARALLEL_LEVEL}
#conan profile new default --detect --force
#conan profile update conf.tools.build:jobs=${CUSTOM_PARALLEL_LEVEL} default

package_final_output() {
  mkdir -p ./output/milvus/configs
  cp -rf ./configs/* ./output/milvus/configs/

  mkdir -p ./output/milvus/lib
  cp ./internal/core/output/lib/*.dylib* ./output/milvus/lib/ 2>/dev/null||true
  cp ./internal/core/output/lib/*.so* ./output/milvus/lib/ ||true
  cp ./internal/core/output/lib64/*.so* ./output/milvus/lib/ 2>/dev/null||true
  for LIB_PATH in $(ldd ./bin/milvus | grep -E '(asan|atomic)' | awk '{print $3}'); do
      cp"$LIB_PATH"./output/milvus/lib/ 2>/dev/null
  done

  mkdir -p ./output/milvus/bin
  cp ./bin/milvus ./output/milvus/bin/milvus
  cp -rf ./lib/* ./output/milvus/lib/ ||true
}

build_milvus() {
  make build-cpp
  make print-build-info
  make build-go
  make build-go-plugin

  package_final_output
}

build_milvus_gpu() {
  make milvus-gpu
  make build-go-plugin
  
  package_final_output
}

restore_cache() {
  if [ -n "$CUSTOM_CACHE_ARTIFACT" ]; then
    rm -rf /ccache/
    rm -rf /home/milvus/.conan/
    if [ "$CUSTOM_CACHE_ARTIFACT" = "clear" ]; then
      echo "Clear cache"
      return 0
    fi
    echo "Restore cache from ${CUSTOM_CACHE_ARTIFACT}"
    artifact download ${CUSTOM_CACHE_ARTIFACT} ./${CUSTOM_CACHE_ARTIFACT}
    tar -xzf ./${CUSTOM_CACHE_ARTIFACT} -C /
    echo "Restore cache ok: $(ls -lh /ccache/)"
    echo "Restore cache ok: $(ls -lh /home/milvus/)"
  else
    echo "CACHE_ARTIFACT is empty, skip restore cache"
  fi
}

build_cpp() {
  echo "make $1"
  make $1

  mkdir -p ./output/home/milvus
  cp -rf /home/milvus/.conan ./output/home/milvus/
  cp -rf /ccache/ ./output/
}

build_cpp_gpu() {
  make build-cpp-gpu

  mkdir -p ./output/home/milvus
  cp -rf /home/milvus/.conan ./output/home/milvus/
  cp -rf /ccache/ ./output/
}

CUSTOM_BUILD_ACTION=${CUSTOM_BUILD_ACTION:-"milvus"}
echo "CUSTOM_BUILD_ACTION: ${CUSTOM_BUILD_ACTION}"
if [ -n "$CUSTOM_BUILD_ACTION" ]; then
    if [ "$CUSTOM_BUILD_ACTION" = "milvus" ]; then
        echo "build milvus"
        restore_cache
        build_milvus
        echo "build milvus cpu ok"
    elif [ "$CUSTOM_BUILD_ACTION" = "milvus-gpu" ]; then
        echo "build milvus gpu"
        restore_cache
        build_milvus_gpu
        echo "build milvus gpu ok"
    elif [ "$CUSTOM_BUILD_ACTION" = "build-cpp" ]; then
        echo "build cpp"
        build_cpp $CUSTOM_BUILD_ACTION
        echo "build cpp ok"
    elif [ "$CUSTOM_BUILD_ACTION" = "build-cpp-gpu" ]; then
        echo "build cpp gpu"
        build_cpp_gpu
        echo "build cpp gpu ok"
    else
        echo "Unknown build action: $CUSTOM_BUILD_ACTION"
    fi
else
    echo "No build action specified."
fi
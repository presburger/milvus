#!/usr/bin/env bash

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
echo "[INFO] 脚本目录：$script_dir"

cp -rf /root/.conan/data $script_dir/
mkdir -p $script_dir/logs/

nohup conan_server --server_dir $script_dir/conan_server/server.conf -d $script_dir/data > $script_dir/logs/app.log 2>&1 &

CONAN_ARTIFACTORY_URL="${CONAN_ARTIFACTORY_URL:-http://localhost:9300}"
if [[ ! `conan remote list` == *default-conan-local* ]]; then
    echo "add remote $CONAN_ARTIFACTORY_URL"
    conan remote add default-conan-local $CONAN_ARTIFACTORY_URL false
else
    echo "update remote $CONAN_ARTIFACTORY_URL"
    conan remote update default-conan-local $CONAN_ARTIFACTORY_URL false
fi

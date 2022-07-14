// Copyright (C) 2019-2020 Zilliz. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance
// with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied. See the License for the specific language governing permissions and limitations under the License

#include <string.h>
#include "config/ConfigKnowhere.h"
#include "indexbuilder/init_c.h"
#include <stdarg.h>
#include <vector>
void
IndexBuilderInit(const char* conf_file) {
    milvus::config::KnowhereInitImpl(conf_file);
}

// return value must be freed by the caller
char*
IndexBuilderSetSimdType(const char* value) {
    auto real_type = milvus::config::KnowhereSetSimdType(value);
    char* ret = reinterpret_cast<char*>(malloc(real_type.length() + 1));
    memcpy(ret, real_type.c_str(), real_type.length());
    ret[real_type.length()] = 0;
    return ret;
}

void
IndexBuilderSetIndexSliceSize(const int64_t value) {
    milvus::config::KnowhereSetIndexSliceSize(value);
}

void
IndexBuilderInitGPUResource(int cnt, ...) {
    std::vector<int64_t> gpu_ids;
    va_list p_args;
    va_start(p_args, cnt);
    for (idx = 0; idx < cnt; ++idx) {
        gpu_ids.push_back(va_arg(p_args, int64_t));
    }
    va_end(p_args);
    milvus::config::KnowhereInitGPUResource(gpu_ids);
}

void
IndexBuilderFreeGPUResource() {
    milvus::config::KnowhereFreeGPUResource();
}

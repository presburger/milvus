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

#include <boost/filesystem/operations.hpp>
#include <chrono>
#include <arrow/array/builder_binary.h>
#include <arrow/array/builder_primitive.h>
#include <arrow/record_batch.h>
#include <arrow/type.h>
#include <arrow/type_fwd.h>
#include <gtest/gtest.h>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <fstream>
#include <vector>
#include <unistd.h>

#include "common/EasyAssert.h"
#include "common/FieldDataInterface.h"
#include "common/Slice.h"
#include "common/Common.h"
#include "common/Types.h"
#include "storage/ChunkManager.h"
#include "storage/DataCodec.h"
#include "storage/InsertData.h"
#include "storage/ThreadPool.h"
#include "storage/Types.h"
#include "storage/Util.h"
#include "storage/DiskFileManagerImpl.h"
#include "storage/LocalChunkManagerSingleton.h"
#include "index/VectorDiskIndex.h"

#include "test_utils/storage_test_utils.h"

using namespace std;
using namespace milvus;
using namespace milvus::storage;
using namespace knowhere;

class DiskAnnFileManagerTest : public testing::Test {
 public:
    DiskAnnFileManagerTest() {
    }
    ~DiskAnnFileManagerTest() {
    }

    virtual void
    SetUp() {
        cm_ = storage::CreateChunkManager(get_default_local_storage_config());
    }

 protected:
    ChunkManagerPtr cm_;
};

TEST_F(DiskAnnFileManagerTest, AddFilePositiveParallel) {
    auto lcm = LocalChunkManagerSingleton::GetInstance().GetChunkManager();
    std::string indexFilePath = "/tmp/diskann/index_files/1000/index";
    auto exist = lcm->Exist(indexFilePath);
    EXPECT_EQ(exist, false);
    uint64_t index_size = 50 << 20;
    lcm->CreateFile(indexFilePath);
    std::vector<uint8_t> data(index_size);
    lcm->Write(indexFilePath, data.data(), index_size);

    // collection_id: 1, partition_id: 2, segment_id: 3
    // field_id: 100, index_build_id: 1000, index_version: 1
    FieldDataMeta filed_data_meta = {1, 2, 3, 100};
    IndexMeta index_meta = {3, 100, 1000, 1, "index"};

    int64_t slice_size = milvus::FILE_SLICE_SIZE;
    auto diskAnnFileManager = std::make_shared<DiskFileManagerImpl>(
        storage::FileManagerContext(filed_data_meta, index_meta, cm_));
    auto ok = diskAnnFileManager->AddFile(indexFilePath);
    EXPECT_EQ(ok, true);

    auto remote_files_to_size = diskAnnFileManager->GetRemotePathsToFileSize();
    auto num_slice = index_size / slice_size;
    EXPECT_EQ(remote_files_to_size.size(),
              index_size % slice_size == 0 ? num_slice : num_slice + 1);

    std::vector<std::string> remote_files;
    for (auto& file2size : remote_files_to_size) {
        std::cout << file2size.first << std::endl;
        remote_files.emplace_back(file2size.first);
    }
    diskAnnFileManager->CacheIndexToDisk(remote_files);
    auto local_files = diskAnnFileManager->GetLocalFilePaths();
    for (auto& file : local_files) {
        auto file_size = lcm->Size(file);
        auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[file_size]);
        lcm->Read(file, buf.get(), file_size);

        auto index =
            milvus::storage::CreateFieldData(storage::DataType::INT8, false);
        index->FillFieldData(buf.get(), file_size);
        auto rows = index->get_num_rows();
        auto rawData = (uint8_t*)(index->Data());

        EXPECT_EQ(rows, index_size);
        EXPECT_EQ(rawData[0], data[0]);
        EXPECT_EQ(rawData[4], data[4]);
    }

    for (auto file : local_files) {
        cm_->Remove(file);
    }
}

int
test_worker(string s) {
    std::cout << s << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << s << std::endl;
    return 1;
}

int
compute(int a) {
    return a + 10;
}

TEST_F(DiskAnnFileManagerTest, TestThreadPoolBase) {
    auto thread_pool = std::make_shared<milvus::ThreadPool>(10, "test1");
    std::cout << "current thread num" << thread_pool->GetThreadNum()
              << std::endl;
    auto thread_num_1 = thread_pool->GetThreadNum();
    EXPECT_GT(thread_num_1, 0);

    auto fut = thread_pool->Submit(compute, 10);
    auto res = fut.get();
    EXPECT_EQ(res, 20);

    std::vector<std::future<int>> futs;
    for (int i = 0; i < 10; ++i) {
        futs.push_back(thread_pool->Submit(compute, i));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    std::cout << "current thread num" << thread_pool->GetThreadNum()
              << std::endl;

    for (int i = 0; i < 10; ++i) {
        std::cout << futs[i].get() << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    std::cout << "current thread num" << thread_pool->GetThreadNum()
              << std::endl;
    auto thread_num_2 = thread_pool->GetThreadNum();
    EXPECT_EQ(thread_num_2, thread_num_1);
}

TEST_F(DiskAnnFileManagerTest, TestThreadPool) {
    auto thread_pool = std::make_shared<milvus::ThreadPool>(10, "test");
    std::vector<std::future<int>> futures;
    auto start = chrono::system_clock::now();
    for (int i = 0; i < 10; i++) {
        futures.push_back(
            thread_pool->Submit(test_worker, "test_id" + std::to_string(i)));
    }
    for (auto& future : futures) {
        EXPECT_EQ(future.get(), 1);
    }
    auto end = chrono::system_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end - start);
    auto second = double(duration.count()) * chrono::microseconds::period::num /
                  chrono::microseconds::period::den;
    std::cout << "cost time:" << second << std::endl;
}

int
test_exception(string s) {
    if (s == "test_id60") {
        throw SegcoreError(ErrorCode::UnexpectedError, "run time error");
    }
    return 1;
}

TEST_F(DiskAnnFileManagerTest, TestThreadPoolException) {
    try {
        auto thread_pool = std::make_shared<milvus::ThreadPool>(10, "test");
        std::vector<std::future<int>> futures;
        for (int i = 0; i < 10; i++) {
            futures.push_back(thread_pool->Submit(
                test_exception, "test_id" + std::to_string(i)));
        }
        for (auto& future : futures) {
            future.get();
        }
    } catch (std::exception& e) {
        EXPECT_EQ(std::string(e.what()), "run time error");
    }
}

namespace {
const int64_t kOptFieldId = 123456;
const std::string kOptFieldName = "opt_field_name";
const int64_t kOptFieldDataRange = 1000;
const std::string kOptFieldPath = "/tmp/diskann/opt_field/";
const size_t kEntityCnt = 1000 * 10;
const FieldDataMeta kOptVecFieldDataMeta = {1, 2, 3, 100};
using OffsetT = uint32_t;

auto
CreateFileManager(const ChunkManagerPtr& cm)
    -> std::shared_ptr<DiskFileManagerImpl> {
    // collection_id: 1, partition_id: 2, segment_id: 3
    // field_id: 100, index_build_id: 1000, index_version: 1
    IndexMeta index_meta = {
        3, 100, 1000, 1, "opt_fields", "field_name", DataType::VECTOR_FLOAT, 1};
    int64_t slice_size = milvus::FILE_SLICE_SIZE;
    return std::make_shared<DiskFileManagerImpl>(
        storage::FileManagerContext(kOptVecFieldDataMeta, index_meta, cm));
}

template <typename T>
auto
PrepareRawFieldData(const int64_t opt_field_data_range) -> std::vector<T> {
    if (opt_field_data_range > std::numeric_limits<T>::max()) {
        throw std::runtime_error("field data range is too large: " +
                                 std::to_string(opt_field_data_range));
    }
    std::vector<T> data(kEntityCnt);
    T field_val = 0;
    for (size_t i = 0; i < kEntityCnt; ++i) {
        data[i] = field_val++;
        if (field_val >= opt_field_data_range) {
            field_val = 0;
        }
    }
    return data;
}

template <>
auto
PrepareRawFieldData<std::string>(const int64_t opt_field_data_range)
    -> std::vector<std::string> {
    if (opt_field_data_range > std::numeric_limits<char>::max()) {
        throw std::runtime_error("field data range is too large: " +
                                 std::to_string(opt_field_data_range));
    }
    std::vector<std::string> data(kEntityCnt);
    char field_val = 0;
    for (size_t i = 0; i < kEntityCnt; ++i) {
        data[i] = std::to_string(field_val);
        field_val++;
        if (field_val >= opt_field_data_range) {
            field_val = 0;
        }
    }
    return data;
}

template <DataType DT, typename NativeType>
auto
PrepareInsertData(const int64_t opt_field_data_range) -> std::string {
    std::vector<NativeType> data =
        PrepareRawFieldData<NativeType>(opt_field_data_range);
    auto field_data = storage::CreateFieldData(DT, false, 1, kEntityCnt);
    field_data->FillFieldData(data.data(), kEntityCnt);
    storage::InsertData insert_data(field_data);
    insert_data.SetFieldDataMeta(kOptVecFieldDataMeta);
    insert_data.SetTimestamps(0, 100);
    auto serialized_data = insert_data.Serialize(storage::StorageType::Remote);

    auto chunk_manager =
        storage::CreateChunkManager(get_default_local_storage_config());

    std::string path = kOptFieldPath + std::to_string(kOptFieldId);
    boost::filesystem::remove_all(path);
    chunk_manager->Write(path, serialized_data.data(), serialized_data.size());
    return path;
}

template <DataType DT>
auto
PrepareOptionalField(const std::shared_ptr<DiskFileManagerImpl>& file_manager,
                     const std::string& insert_file_path) -> OptFieldT {
    OptFieldT opt_field;
    std::vector<std::string> insert_files;
    insert_files.emplace_back(insert_file_path);
    opt_field[kOptFieldId] = {kOptFieldName, DT, insert_files};
    return opt_field;
}

void
CheckOptFieldCorrectness(
    const std::string& local_file_path,
    const int64_t opt_field_data_range = kOptFieldDataRange) {
    std::ifstream ifs(local_file_path);
    if (!ifs.is_open()) {
        FAIL() << "open file failed: " << local_file_path << std::endl;
        return;
    }
    uint8_t meta_version;
    uint32_t meta_num_of_fields, num_of_unique_field_data;
    int64_t field_id;
    ifs.read(reinterpret_cast<char*>(&meta_version), sizeof(meta_version));
    EXPECT_EQ(meta_version, 0);
    ifs.read(reinterpret_cast<char*>(&meta_num_of_fields),
             sizeof(meta_num_of_fields));
    EXPECT_EQ(meta_num_of_fields, 1);
    ifs.read(reinterpret_cast<char*>(&field_id), sizeof(field_id));
    EXPECT_EQ(field_id, kOptFieldId);
    ifs.read(reinterpret_cast<char*>(&num_of_unique_field_data),
             sizeof(num_of_unique_field_data));
    EXPECT_EQ(num_of_unique_field_data, opt_field_data_range);

    uint32_t expected_single_category_offset_cnt =
        kEntityCnt / opt_field_data_range;
    uint32_t read_single_category_offset_cnt;
    std::vector<OffsetT> single_category_offsets(
        expected_single_category_offset_cnt);
    for (uint32_t i = 0; i < num_of_unique_field_data; ++i) {
        ifs.read(reinterpret_cast<char*>(&read_single_category_offset_cnt),
                 sizeof(read_single_category_offset_cnt));
        ASSERT_EQ(read_single_category_offset_cnt,
                  expected_single_category_offset_cnt);
        ifs.read(reinterpret_cast<char*>(single_category_offsets.data()),
                 read_single_category_offset_cnt * sizeof(OffsetT));

        OffsetT first_offset = 0;
        if (read_single_category_offset_cnt > 0) {
            first_offset = single_category_offsets[0];
        }
        for (size_t j = 1; j < read_single_category_offset_cnt; ++j) {
            ASSERT_EQ(single_category_offsets[j] % opt_field_data_range,
                      first_offset % opt_field_data_range);
        }
    }
}
}  // namespace

TEST_F(DiskAnnFileManagerTest, CacheOptFieldToDiskOptFieldMoreThanOne) {
    auto file_manager = CreateFileManager(cm_);
    const auto insert_file_path =
        PrepareInsertData<DataType::INT64, int64_t>(kOptFieldDataRange);
    OptFieldT opt_fields =
        PrepareOptionalField<DataType::INT64>(file_manager, insert_file_path);
    opt_fields[kOptFieldId + 1] = {
        kOptFieldName + "second", DataType::INT64, {insert_file_path}};
    EXPECT_THROW(file_manager->CacheOptFieldToDisk(opt_fields), SegcoreError);
}

TEST_F(DiskAnnFileManagerTest, CacheOptFieldToDiskSpaceCorrect) {
    auto file_manager = CreateFileManager(cm_);
    const auto insert_file_path =
        PrepareInsertData<DataType::INT64, int64_t>(kOptFieldDataRange);
    auto opt_fileds =
        PrepareOptionalField<DataType::INT64>(file_manager, insert_file_path);
    auto res = file_manager->CacheOptFieldToDisk(opt_fileds);
    ASSERT_FALSE(res.empty());
    CheckOptFieldCorrectness(res);
}

#define TEST_TYPE(NAME, TYPE, NATIVE_TYPE, RANGE)                            \
    TEST_F(DiskAnnFileManagerTest, CacheOptFieldToDiskCorrect##NAME) {       \
        auto file_manager = CreateFileManager(cm_);                          \
        auto insert_file_path = PrepareInsertData<TYPE, NATIVE_TYPE>(RANGE); \
        auto opt_fields =                                                    \
            PrepareOptionalField<TYPE>(file_manager, insert_file_path);      \
        auto res = file_manager->CacheOptFieldToDisk(opt_fields);            \
        ASSERT_FALSE(res.empty());                                           \
        CheckOptFieldCorrectness(res, RANGE);                                \
    };

TEST_TYPE(INT8, DataType::INT8, int8_t, 100);
TEST_TYPE(INT16, DataType::INT16, int16_t, kOptFieldDataRange);
TEST_TYPE(INT32, DataType::INT32, int32_t, kOptFieldDataRange);
TEST_TYPE(INT64, DataType::INT64, int64_t, kOptFieldDataRange);
TEST_TYPE(FLOAT, DataType::FLOAT, float, kOptFieldDataRange);
TEST_TYPE(DOUBLE, DataType::DOUBLE, double, kOptFieldDataRange);
TEST_TYPE(STRING, DataType::STRING, std::string, 100);
TEST_TYPE(VARCHAR, DataType::VARCHAR, std::string, 100);

#undef TEST_TYPE

TEST_F(DiskAnnFileManagerTest, CacheOptFieldToDiskOnlyOneCategory) {
    auto file_manager = CreateFileManager(cm_);
    {
        const auto insert_file_path =
            PrepareInsertData<DataType::INT64, int64_t>(1);
        auto opt_fileds = PrepareOptionalField<DataType::INT64>(
            file_manager, insert_file_path);
        auto res = file_manager->CacheOptFieldToDisk(opt_fileds);
        ASSERT_TRUE(res.empty());
    }
}


TEST_F(DiskAnnFileManagerTest, TestDiskannLegacyFlags) {
    // Arrange
    auto file_manager = CreateFileManager(cm_);
    
    // Act & Assert - Test default values
    EXPECT_TRUE(file_manager->IsLegacyDiskann());
    EXPECT_FALSE(file_manager->IsDiskannMemoryEnabled());
    EXPECT_EQ(file_manager->GetDiskannLoadMemorySize(), 0);
    
    // Act & Assert - Test setting diskann legacy flag
    file_manager->SetIsLegacyDiskann(false);
    EXPECT_FALSE(file_manager->IsLegacyDiskann());
    
    file_manager->SetIsLegacyDiskann(true);
    EXPECT_TRUE(file_manager->IsLegacyDiskann());
    
    // Act & Assert - Test setting diskann memory flag
    file_manager->SetUseDiskannMemory(true);
    EXPECT_TRUE(file_manager->IsDiskannMemoryEnabled());
    
    file_manager->SetUseDiskannMemory(false);
    EXPECT_FALSE(file_manager->IsDiskannMemoryEnabled());
}

TEST_F(DiskAnnFileManagerTest, TestAccumulateDiskannLoadMemorySize) {
    // Arrange
    auto file_manager = CreateFileManager(cm_);
    file_manager->SetIsLegacyDiskann(false);
    
    struct TestCase {
        std::string file_name;
        size_t file_size;
        bool use_diskann_memory;
        bool should_include;
        std::string description;
    };
    
    std::vector<TestCase> test_cases = {
        // Sampling files - should be excluded
        {"_sample_data.bin", 1000, false, false, "sample data file"},
        {"_sample_ids.bin", 2000, true, false, "sample ids file"},
        
        // Quantization files - should be included
        {"_rabitq_data.bin", 3000, false, true, "rabitq file"},
        {"_pq_compressed.bin", 4000, false, true, "pq compressed file"},
        {"_pq_pivots.bin", 5000, false, true, "pq pivots file"},
        {"_rotation_matrix.bin", 6000, false, true, "rotation matrix file"},
        {"max_base_norm.bin", 7000, false, true, "max base norm file"},
        
        // Memory index files - included when use_diskann_memory=true
        {"_mem.index", 8000, true, true, "memory index file with memory mode"},
        {"_mem.index.data", 9000, true, true, "memory index data file with memory mode"},
        {"_mem.index", 8000, false, false, "memory index file with disk mode"},
        {"_mem.index.data", 9000, false, false, "memory index data file with disk mode"},
        
        // Disk index files - included when use_diskann_memory=false
        {"_disk.index", 10000, false, true, "disk index file with disk mode"},
        {"_disk.index", 10000, true, false, "disk index file with memory mode"},
        
        // Other files - should be excluded
        {"other_file.bin", 11000, false, false, "other file"},
        {"unknown.data", 12000, true, false, "unknown file"},
    };
    
    for (const auto& test_case : test_cases) {
        // Arrange
        auto test_file_manager = CreateFileManager(cm_);
        test_file_manager->SetIsLegacyDiskann(false);
        test_file_manager->SetUseDiskannMemory(test_case.use_diskann_memory);
        
        size_t initial_size = test_file_manager->GetDiskannLoadMemorySize();
        
        // Act - Create a temporary file and add it
        auto lcm = LocalChunkManagerSingleton::GetInstance().GetChunkManager();
        std::string temp_file_path = "/tmp/diskann/test_files/" + test_case.file_name;
        lcm->CreateFile(temp_file_path);
        std::vector<uint8_t> data(test_case.file_size, 0x42);
        lcm->Write(temp_file_path, data.data(), test_case.file_size);
        
        test_file_manager->AddFile(temp_file_path);
        
        // Assert
        size_t expected_size = initial_size + (test_case.should_include ? test_case.file_size : 0);
        EXPECT_EQ(test_file_manager->GetDiskannLoadMemorySize(), expected_size) 
            << "Failed for test case: " << test_case.description 
            << " (file: " << test_case.file_name << ", use_memory: " << test_case.use_diskann_memory << ")";
        
        // Cleanup
        lcm->Remove(temp_file_path);
    }
}

TEST_F(DiskAnnFileManagerTest, TestAccumulateWithZeroFileSize) {
    // Arrange
    auto file_manager = CreateFileManager(cm_);
    file_manager->SetIsLegacyDiskann(false);
    file_manager->SetUseDiskannMemory(false);
    
    auto lcm = LocalChunkManagerSingleton::GetInstance().GetChunkManager();
    std::string temp_file_path = "/tmp/diskann/test_files/_disk.index";
    
    // Act - Create empty file
    lcm->CreateFile(temp_file_path);
    // Don't write any data, so file size is 0
    
    size_t initial_size = file_manager->GetDiskannLoadMemorySize();
    file_manager->AddFile(temp_file_path);
    
    // Assert - Zero size files should not be accumulated
    EXPECT_EQ(file_manager->GetDiskannLoadMemorySize(), initial_size);
    
    // Cleanup
    lcm->Remove(temp_file_path);
}

TEST_F(DiskAnnFileManagerTest, TestNonLegacyMemorySizeCalculation) {
    // Arrange
    auto file_manager = CreateFileManager(cm_);
    auto lcm = LocalChunkManagerSingleton::GetInstance().GetChunkManager();
    
    // Create test files with different sizes
    std::vector<std::pair<std::string, size_t>> test_files = {
        {"_disk.index", 50000},
        {"rabitq_compressed.bin", 30000},
        {"pq_pivots.bin", 20000},
        {"_sample_data.bin", 10000}  // This should be excluded
    };
    
    size_t expected_total = 0;
    for (const auto& [file_name, file_size] : test_files) {
        std::string temp_file_path = "/tmp/diskann/test_files/" + file_name;
        lcm->CreateFile(temp_file_path);
        std::vector<uint8_t> data(file_size, 0x42);
        lcm->Write(temp_file_path, data.data(), file_size);
        
        // Only include non-sample files in expected total
        if (file_name != "_sample_data.bin") {
            expected_total += file_size;
        }
    }
    
    // Test with legacy diskann disabled
    file_manager->SetIsLegacyDiskann(false);
    file_manager->SetUseDiskannMemory(false);  // Use disk mode
    
    // Act - Add all files
    for (const auto& [file_name, file_size] : test_files) {
        std::string temp_file_path = "/tmp/diskann/test_files/" + file_name;
        file_manager->AddFile(temp_file_path);
    }
    
    // Assert
    EXPECT_EQ(file_manager->GetDiskannLoadMemorySize(), expected_total);
    EXPECT_GT(file_manager->GetAddedTotalFileSize(), file_manager->GetDiskannLoadMemorySize());
    
    // Cleanup
    for (const auto& [file_name, file_size] : test_files) {
        std::string temp_file_path = "/tmp/diskann/test_files/" + file_name;
        lcm->Remove(temp_file_path);
    }
}

TEST_F(DiskAnnFileManagerTest, TestStoreStrategyParsing) {
    // This test would ideally test the VectorDiskIndex Build method,
    // but since we're focusing on DiskFileManagerImpl, we test the flag setting directly
    
    // Arrange
    auto file_manager = CreateFileManager(cm_);
    
    // Test case 1: Memory store strategy
    file_manager->SetUseDiskannMemory(true);
    EXPECT_TRUE(file_manager->IsDiskannMemoryEnabled());
    
    // Test case 2: Non-memory store strategy
    file_manager->SetUseDiskannMemory(false);
    EXPECT_FALSE(file_manager->IsDiskannMemoryEnabled());
}

TEST_F(DiskAnnFileManagerTest, TestNonLegacyEndToEnd) {
    auto file_manager = CreateFileManager(cm_);
    auto lcm = LocalChunkManagerSingleton::GetInstance().GetChunkManager();
    
    // Step 1: Configure for legacy diskann
    file_manager->SetIsLegacyDiskann(false);
    file_manager->SetUseDiskannMemory(true);  // Use memory mode
    
    // Step 2: Add various types of files
    std::vector<std::pair<std::string, size_t>> files = {
        {"_mem.index", 100000},           // Should be included (memory mode)
        {"_mem.index.data", 50000},       // Should be included (memory mode)
        {"rabitq_compressed.bin", 30000}, // Should be included (quantization)
        {"pq_pivots.bin", 20000},         // Should be included (quantization)
        {"_sample_data.bin", 10000},      // Should be excluded (sampling)
        {"_disk.index", 80000},           // Should be excluded (disk mode file in memory mode)
    };
    
    size_t expected_non_legacy_size = 100000 + 50000 + 30000 + 20000; // Exclude sample and disk files
    size_t expected_total_size = 0;
    
    for (const auto& [file_name, file_size] : files) {
        expected_total_size += file_size;
        
        std::string temp_file_path = "/tmp/diskann/test_files/" + file_name;
        lcm->CreateFile(temp_file_path);
        std::vector<uint8_t> data(file_size, 0x42);
        lcm->Write(temp_file_path, data.data(), file_size);
        
        // Act
        file_manager->AddFile(temp_file_path);
    }
    
    // Assert
    EXPECT_EQ(file_manager->GetDiskannLoadMemorySize(), expected_non_legacy_size);
    EXPECT_EQ(file_manager->GetAddedTotalFileSize(), expected_total_size);
    EXPECT_TRUE(file_manager->IsLegacyDiskann());
    EXPECT_TRUE(file_manager->IsDiskannMemoryEnabled());
    
    EXPECT_NE(file_manager->GetDiskannLoadMemorySize(), file_manager->GetAddedTotalFileSize());
    
    // Cleanup
    for (const auto& [file_name, file_size] : files) {
        std::string temp_file_path = "/tmp/diskann/test_files/" + file_name;
        lcm->Remove(temp_file_path);
    }
}

TEST_F(DiskAnnFileManagerTest, TestMemoryVsDiskModeFiltering) {
    // Arrange
    auto lcm = LocalChunkManagerSingleton::GetInstance().GetChunkManager();
    
    // Test files for both memory and disk modes
    std::vector<std::pair<std::string, size_t>> test_files = {
        {"_mem.index", 10000},
        {"_mem.index.data", 5000},
        {"_disk.index", 8000},
    };
    
    // Create test files
    for (const auto& [file_name, file_size] : test_files) {
        std::string temp_file_path = "/tmp/diskann/test_files/" + file_name;
        lcm->CreateFile(temp_file_path);
        std::vector<uint8_t> data(file_size, 0x42);
        lcm->Write(temp_file_path, data.data(), file_size);
    }
    
    // Test Case 1: Memory mode - should include memory files, exclude disk files
    {
        auto file_manager = CreateFileManager(cm_);
        file_manager->SetIsLegacyDiskann(false);
        file_manager->SetUseDiskannMemory(true);
        
        for (const auto& [file_name, file_size] : test_files) {
            std::string temp_file_path = "/tmp/diskann/test_files/" + file_name;
            file_manager->AddFile(temp_file_path);
        }
        
        size_t expected_memory_mode_size = 10000 + 5000; // Only memory files
        EXPECT_EQ(file_manager->GetDiskannLoadMemorySize(), expected_memory_mode_size);
    }
    
    // Test Case 2: Disk mode - should include disk files, exclude memory files
    {
        auto file_manager = CreateFileManager(cm_);
        file_manager->SetIsLegacyDiskann(false);
        file_manager->SetUseDiskannMemory(false);
        
        for (const auto& [file_name, file_size] : test_files) {
            std::string temp_file_path = "/tmp/diskann/test_files/" + file_name;
            file_manager->AddFile(temp_file_path);
        }
        
        size_t expected_disk_mode_size = 8000; // Only disk files
        EXPECT_EQ(file_manager->GetDiskannLoadMemorySize(), expected_disk_mode_size);
    }
    
    // Cleanup
    for (const auto& [file_name, file_size] : test_files) {
        std::string temp_file_path = "/tmp/diskann/test_files/" + file_name;
        lcm->Remove(temp_file_path);
    }
}

/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "velox/connectors/hive/storage_adapters/hdfs/HdfsWriteFile.h"
#ifdef VELOX_ENABLE_HDFS3
#include <hdfs/hdfs.h>
#elif VELOX_ENABLE_HDFS
#include "velox/connectors/hive/storage_adapters/hdfs/HdfsInternal.h"
#endif

namespace facebook::velox {
HdfsWriteFile::HdfsWriteFile(
    hdfsFS hdfsClient,
    std::string_view path,
    int bufferSize,
    short replication,
    int blockSize)
    : hdfsClient_(hdfsClient), filePath_(path) {
  auto pos = filePath_.rfind("/");
  auto parentDir = filePath_.substr(0, pos + 1);
  if (hdfsExists(hdfsClient_, parentDir.c_str()) == -1) {
    hdfsCreateDirectory(hdfsClient_, parentDir.c_str());
  }

  hdfsFile_ = hdfsOpenFile(
      hdfsClient_,
      filePath_.c_str(),
      O_WRONLY,
      bufferSize,
      replication,
      blockSize);
#ifdef VELOX_ENABLE_HDFS3
  VELOX_CHECK_NOT_NULL(
      hdfsFile_,
      "Failed to open hdfs file: {}, with error: {}",
      filePath_,
      std::string(hdfsGetLastError()));
#elif VELOX_ENABLE_HDFS
  VELOX_CHECK_NOT_NULL(hdfsFile_, "Failed to open hdfs file: {}", filePath_);
#endif
}

HdfsWriteFile::~HdfsWriteFile() {
  if (hdfsFile_) {
    close();
  }
}

void HdfsWriteFile::close() {
  int success = hdfsCloseFile(hdfsClient_, hdfsFile_);
#ifdef VELOX_ENABLE_HDFS3
  VELOX_CHECK_EQ(
      success,
      0,
      "Failed to close hdfs file: {}",
      std::string(hdfsGetLastError()));
#elif VELOX_ENABLE_HDFS
  VELOX_CHECK_EQ(success, 0, "Failed to close hdfs file.");
#endif
  hdfsFile_ = nullptr;
}

void HdfsWriteFile::flush() {
  VELOX_CHECK_NOT_NULL(
      hdfsFile_,
      "Cannot flush HDFS file because file handle is null, file path: {}",
      filePath_);
  int success = hdfsFlush(hdfsClient_, hdfsFile_);
#ifdef VELOX_ENABLE_HDFS3
  VELOX_CHECK_EQ(
      success, 0, "Hdfs flush error: {}", std::string(hdfsGetLastError()));
#elif VELOX_ENABLE_HDFS
  VELOX_CHECK_EQ(success, 0, "Hdfs flush error.");
#endif
}

void HdfsWriteFile::append(std::string_view data) {
  if (data.size() == 0) {
    return;
  }
  VELOX_CHECK_NOT_NULL(
      hdfsFile_,
      "Cannot append to HDFS file because file handle is null, file path: {}",
      filePath_);
  int64_t totalWrittenBytes =
      hdfsWrite(hdfsClient_, hdfsFile_, std::string(data).c_str(), data.size());
#ifdef VELOX_ENABLE_HDFS3
  VELOX_CHECK_EQ(
      totalWrittenBytes,
      data.size(),
      "Write failure in HDFSWriteFile::append {}",
      std::string(hdfsGetLastError()));
#elif VELOX_ENABLE_HDFS
  VELOX_CHECK_EQ(
      totalWrittenBytes,
      data.size(),
      "Write failure in HDFSWriteFile::append.");
#endif
}

uint64_t HdfsWriteFile::size() const {
  auto fileInfo = hdfsGetPathInfo(hdfsClient_, filePath_.c_str());
  uint64_t size = fileInfo->mSize;
  // should call hdfsFreeFileInfo to avoid memory leak
  hdfsFreeFileInfo(fileInfo, 1);
  return size;
}

} // namespace facebook::velox

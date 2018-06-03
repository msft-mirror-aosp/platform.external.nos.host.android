/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "buffer.h"
#include "proto_utils.h"

#include <keymasterV4_0/key_param_output.h>

#include <android-base/logging.h>

#include <openssl/rsa.h>

#include <map>
#include <vector>

namespace android {
namespace hardware {
namespace keymaster {

// HAL
using ::android::hardware::keymaster::V4_0::Algorithm;
using ::android::hardware::keymaster::V4_0::BlockMode;
using ::android::hardware::keymaster::V4_0::ErrorCode;
using ::android::hardware::keymaster::V4_0::PaddingMode;

// std
using std::map;
using std::pair;
using std::vector;

class Operation {
public:
    Operation(uint64_t handle, Algorithm algorithm, BlockMode blockMode) :
        _handle(handle), _algorithm(algorithm), _blockMode(blockMode),
        _buffer{} {
        switch (_algorithm) {
        case Algorithm::AES:
            _blockSize = 16;
            break;
        case Algorithm::TRIPLE_DES:
            _blockSize = 8;
            break;
        case Algorithm::HMAC:
            _blockSize = 1;
        default:
            break;
        }
    }
    Operation(uint64_t handle, Algorithm algorithm,
              PaddingMode paddingMode, size_t keyBits) :
        _handle(handle), _algorithm(algorithm),
        _paddingMode(paddingMode), _blockSize(keyBits / 8) { }

    size_t remaining() const {
        return _buffer.size();
    }

    void append(const hidl_vec<uint8_t>& input, uint32_t *consumed) {
        _buffer.insert(_buffer.end(), input.begin(), input.end());
        *consumed = input.size();
    }

    void read(hidl_vec<uint8_t> *data) {
        // Retain at least one full block; this is done so that when
        // in GCM mode, the tag will be available to be consumed by
        // final().
        if (_buffer.size() <= _blockSize) {
            *data = vector<uint8_t>();
            return;
        }
        size_t retain = (_buffer.size() % _blockSize) + _blockSize;
        const size_t count = _buffer.size() - retain;
        *data = vector<uint8_t>(_buffer.begin(), _buffer.begin() + count);
        _buffer.erase(_buffer.begin(), _buffer.begin() + count);
    }

    void final(hidl_vec<uint8_t> *data) {
        if (data != NULL) {
            *data = _buffer;
        }
        _buffer.clear();
    }

private:
    uint64_t _handle;
    Algorithm _algorithm;
    BlockMode _blockMode;
    PaddingMode _paddingMode;
    size_t _blockSize;
    vector<uint8_t> _buffer;
};

static map<uint64_t, Operation>  buffer_map;
typedef map<uint64_t, Operation>::iterator  buffer_item;

ErrorCode buffer_begin(uint64_t handle, Algorithm algorithm,
                       BlockMode blockMode, PaddingMode paddingMode,
                       size_t keyBits)
{
    if (buffer_map.find(handle) != buffer_map.end()) {
        LOG(ERROR) << "Duplicate operation handle " << handle
                   << "returned by begin()";
        // Final the existing op to potential mishandling of data.
        buffer_final(handle, NULL);
        return ErrorCode::UNKNOWN_ERROR;
    }

    if (algorithm == Algorithm::AES || algorithm == Algorithm::TRIPLE_DES ||
        algorithm == Algorithm::HMAC) {
        buffer_map.insert(
            pair<uint64_t, Operation>(
                handle, Operation(handle, algorithm, blockMode)));
    } else if (algorithm == Algorithm::EC || algorithm == Algorithm::RSA) {
        buffer_map.insert(
            pair<uint64_t, Operation>(handle, Operation(handle, algorithm,
                                                        paddingMode, keyBits)));
    }
    return ErrorCode::OK;
}

size_t buffer_remaining(uint64_t handle) {
    if (buffer_map.find(handle) == buffer_map.end()) {
        LOG(ERROR) << "Remaining requested on absent operation: " << handle;
        return 0;
    }

    const Operation &op = buffer_map.find(handle)->second;
    return op.remaining();
}

ErrorCode buffer_append(uint64_t handle,
                        const hidl_vec<uint8_t>& input,
                        uint32_t *consumed)
{
    if (buffer_map.find(handle) == buffer_map.end()) {
        LOG(ERROR) << "Append requested on absent operation: " << handle;
        return ErrorCode::UNKNOWN_ERROR;
    }

    Operation *op = &buffer_map.find(handle)->second;
    op->append(input, consumed);
    return ErrorCode::OK;
}

ErrorCode buffer_read(uint64_t handle,
                         hidl_vec<uint8_t> *data)
{
    if (buffer_map.find(handle) == buffer_map.end()) {
        LOG(ERROR) << "Read requested on absent operation: " << handle;
        return ErrorCode::UNKNOWN_ERROR;
    }

    Operation *op = &buffer_map.find(handle)->second;
    op->read(data);
    return ErrorCode::OK;
}

ErrorCode buffer_final(uint64_t handle,
                   hidl_vec<uint8_t> *data)
{
    if (buffer_map.find(handle) == buffer_map.end()) {
        LOG(ERROR) << "Final requested on absent operation: " << handle;
        return ErrorCode::UNKNOWN_ERROR;
    }
    Operation *op = &buffer_map.find(handle)->second;
    op->final(data);
    buffer_map.erase(handle);
    return ErrorCode::OK;
}

}  // namespace keymaster
}  // hardware
}  // android

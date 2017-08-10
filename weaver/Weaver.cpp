/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "Weaver.h"

#include <android-base/logging.h>

namespace android {
namespace hardware {
namespace weaver {

// libhidl
using ::android::hardware::Void;

// HAL
using ::android::hardware::weaver::V1_0::WeaverConfig;
using ::android::hardware::weaver::V1_0::WeaverReadResponse;
using ::android::hardware::weaver::V1_0::WeaverReadStatus;

// Methods from ::android::hardware::weaver::V1_0::IWeaver follow.
Return<void> Weaver::getConfig(getConfig_cb _hidl_cb) {
    LOG(VERBOSE) << "Running Weaver::getNumSlots";

    // TODO: implement
    LOG(ERROR) << "Not implemented";

    _hidl_cb(WeaverStatus::FAILED, WeaverConfig{});
    return Void();
}

Return<WeaverStatus> Weaver::write(uint32_t slotId, const hidl_vec<uint8_t>& key,
                           const hidl_vec<uint8_t>& value) {
    LOG(INFO) << "Running Weaver::write on slot " << slotId;

    // TODO: implement
    LOG(ERROR) << "Not implemented";
    (void) key;
    (void) value;

    return WeaverStatus::FAILED;
}

Return<void> Weaver::read(uint32_t slotId, const hidl_vec<uint8_t>& key, read_cb _hidl_cb) {
    LOG(VERBOSE) << "Running Weaver::read on slot " << slotId;

    // TODO: implement
    LOG(ERROR) << "Not implemented";
    (void) key;

    _hidl_cb(WeaverReadStatus::FAILED, WeaverReadResponse{});
    return Void();
}

} // namespace weaver
} // namespace hardware
} // namespace android

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

#include "OemLock.h"

#include <android-base/logging.h>

namespace android {
namespace hardware {
namespace oemlock {

// libhidl
using ::android::hardware::Void;

// Methods from ::android::hardware::oemlock::V1_0::IOemLock follow.
Return<void> OemLock::getName(getName_cb _hidl_cb) {
    LOG(VERBOSE) << "Running OemLock::getName";
    _hidl_cb(OemLockStatus::OK, {"01"});
    return Void();
}

Return<OemLockSecureStatus> OemLock::setOemUnlockAllowedByCarrier(
    bool allowed, const hidl_vec<uint8_t>& signature) {
    LOG(INFO) << "Running OemLock::setOemUnlockAllowedByCarrier: " << allowed;

    // TODO: implement
    LOG(ERROR) << "Not implemented";
    (void) signature;

    return OemLockSecureStatus::FAILED;
}

Return<void> OemLock::isOemUnlockAllowedByCarrier(isOemUnlockAllowedByCarrier_cb _hidl_cb) {
    LOG(VERBOSE) << "Running OemLock::isOemUnlockAllowedByCarrier";

    // TODO: implement
    LOG(ERROR) << "Not implemented";

    _hidl_cb(OemLockStatus::FAILED, false);
    return Void();
}

Return<OemLockStatus> OemLock::setOemUnlockAllowedByDevice(bool allowed) {
    LOG(INFO) << "Running OemLock::setOemUnlockAllowedByDevice: " << allowed;

    // TODO: implement
    LOG(ERROR) << "Not implemented";

    return OemLockStatus::FAILED;
}

Return<void> OemLock::isOemUnlockAllowedByDevice(isOemUnlockAllowedByDevice_cb _hidl_cb) {
    LOG(VERBOSE) << "Running OemLock::isOemUnlockAllowedByDevice";

    // TODO: implement
    LOG(ERROR) << "Not implemented";

    _hidl_cb(OemLockStatus::FAILED, false);
    return Void();
}

} // namespace oemlock
} // namespace hardware
} // namespace android

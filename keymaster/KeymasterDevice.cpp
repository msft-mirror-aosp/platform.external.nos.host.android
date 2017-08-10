/*
 * Copyright 2017, The Android Open Source Project
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

#include "KeymasterDevice.h"

#include <android-base/logging.h>

namespace android {
namespace hardware {
namespace keymaster {

// libhidl
using ::android::hardware::Void;

// HAL
using ::android::hardware::keymaster::V3_0::KeyCharacteristics;

// Methods from ::android::hardware::keymaster::V3_0::IKeymasterDevice follow.
Return<void> KeymasterDevice::getHardwareFeatures(getHardwareFeatures_cb _hidl_cb) {
    LOG(VERBOSE) << "Running KeymasterDevice::getHardwareFeatures";

    // TODO: implement
    LOG(ERROR) << "Not implemented";

    _hidl_cb(false, false, false, false, false, {""}, {""});
    return Void();
}

Return<ErrorCode> KeymasterDevice::addRngEntropy(const hidl_vec<uint8_t>& data) {
    LOG(VERBOSE) << "Running KeymasterDevice::addRngEntropy";

    // TODO: implement
    LOG(ERROR) << "Not implemented";
    (void) data;

    return ErrorCode::UNIMPLEMENTED;
}

Return<void> KeymasterDevice::generateKey(const hidl_vec<KeyParameter>& keyParams,
                                          generateKey_cb _hidl_cb) {
    LOG(VERBOSE) << "Running KeymasterDevice::generateKey";

    // TODO: implement
    LOG(ERROR) << "Not implemented";
    (void) keyParams;

    _hidl_cb(ErrorCode::UNIMPLEMENTED, hidl_vec<uint8_t>{}, KeyCharacteristics{});
    return Void();
}

Return<void> KeymasterDevice::getKeyCharacteristics(const hidl_vec<uint8_t>& keyBlob,
                                                    const hidl_vec<uint8_t>& clientId,
                                                    const hidl_vec<uint8_t>& appData,
                                                    getKeyCharacteristics_cb _hidl_cb) {
    LOG(VERBOSE) << "Running KeymasterDevice::getKeyCharacteristics";

    // TODO: implement
    LOG(ERROR) << "Not implemented";
    (void) keyBlob;
    (void) clientId;
    (void) appData;

    _hidl_cb(ErrorCode::UNIMPLEMENTED, KeyCharacteristics{});
    return Void();
}

Return<void> KeymasterDevice::importKey(const hidl_vec<KeyParameter>& params, KeyFormat keyFormat,
                                        const hidl_vec<uint8_t>& keyData, importKey_cb _hidl_cb) {
    LOG(VERBOSE) << "Running KeymasterDevice::importKey";

    // TODO: implement
    LOG(ERROR) << "Not implemented";
    (void) params;
    (void) keyFormat;
    (void) keyData;

    _hidl_cb(ErrorCode::UNIMPLEMENTED, hidl_vec<uint8_t>{}, KeyCharacteristics{});
    return Void();
}

Return<void> KeymasterDevice::exportKey(KeyFormat exportFormat, const hidl_vec<uint8_t>& keyBlob,
                                        const hidl_vec<uint8_t>& clientId,
                                        const hidl_vec<uint8_t>& appData, exportKey_cb _hidl_cb) {
    LOG(VERBOSE) << "Running KeymasterDevice::exportKey";

    // TODO: implement
    LOG(ERROR) << "Not implemented";
    (void) exportFormat;
    (void) keyBlob;
    (void) clientId;
    (void) appData;

    _hidl_cb(ErrorCode::UNIMPLEMENTED, hidl_vec<uint8_t>{});
    return Void();
}

Return<void> KeymasterDevice::attestKey(const hidl_vec<uint8_t>& keyToAttest,
                                        const hidl_vec<KeyParameter>& attestParams,
                                        attestKey_cb _hidl_cb) {
    LOG(VERBOSE) << "Running KeymasterDevice::attestKey";

    // TODO: implement
    LOG(ERROR) << "Not implemented";
    (void) keyToAttest;
    (void) attestParams;

    _hidl_cb(ErrorCode::UNIMPLEMENTED, hidl_vec<hidl_vec<uint8_t>>{});
    return Void();
}

Return<void> KeymasterDevice::upgradeKey(const hidl_vec<uint8_t>& keyBlobToUpgrade,
                                         const hidl_vec<KeyParameter>& upgradeParams,
                                         upgradeKey_cb _hidl_cb) {
    LOG(VERBOSE) << "Running KeymasterDevice::upgradeKey";

    // TODO: implement
    LOG(ERROR) << "Not implemented";
    (void) keyBlobToUpgrade;
    (void) upgradeParams;

    _hidl_cb(ErrorCode::UNIMPLEMENTED, hidl_vec<uint8_t>{});
    return Void();
}

Return<ErrorCode> KeymasterDevice::deleteKey(const hidl_vec<uint8_t>& keyBlob) {
    LOG(VERBOSE) << "Running KeymasterDevice::deleteKey";

    // TODO: implement
    LOG(ERROR) << "Not implemented";
    (void) keyBlob;

    return ErrorCode::UNIMPLEMENTED;
}

Return<ErrorCode> KeymasterDevice::deleteAllKeys() {
    LOG(VERBOSE) << "Running KeymasterDevice::deleteAllKeys";

    // TODO: implement
    LOG(ERROR) << "Not implemented";

    return ErrorCode::UNIMPLEMENTED;
}

Return<ErrorCode> KeymasterDevice::destroyAttestationIds() {
    LOG(VERBOSE) << "Running KeymasterDevice::destroyAttestationIds";

    // TODO: implement
    LOG(ERROR) << "Not implemented";

    return ErrorCode::UNIMPLEMENTED;
}

Return<void> KeymasterDevice::begin(KeyPurpose purpose, const hidl_vec<uint8_t>& key,
                                    const hidl_vec<KeyParameter>& inParams, begin_cb _hidl_cb) {
    LOG(VERBOSE) << "Running KeymasterDevice::begin";

    // TODO: implement
    LOG(ERROR) << "Not implemented";
    (void) purpose;
    (void) key;
    (void) inParams;

    _hidl_cb(ErrorCode::UNIMPLEMENTED, hidl_vec<KeyParameter>{}, 0);
    return Void();
}

Return<void> KeymasterDevice::update(uint64_t operationHandle,
                                     const hidl_vec<KeyParameter>& inParams,
                                     const hidl_vec<uint8_t>& input, update_cb _hidl_cb) {
    LOG(VERBOSE) << "Running KeymasterDevice::update";

    // TODO: implement
    LOG(ERROR) << "Not implemented";
    (void) operationHandle;
    (void) inParams;
    (void) input;

    _hidl_cb(ErrorCode::UNIMPLEMENTED, 0, hidl_vec<KeyParameter>{}, hidl_vec<uint8_t>{});
    return Void();
}

Return<void> KeymasterDevice::finish(uint64_t operationHandle,
                                     const hidl_vec<KeyParameter>& inParams,
                                     const hidl_vec<uint8_t>& input,
                                     const hidl_vec<uint8_t>& signature, finish_cb _hidl_cb) {
    LOG(VERBOSE) << "Running KeymasterDevice::finish";

    // TODO: implement
    LOG(ERROR) << "Not implemented";
    (void) operationHandle;
    (void) inParams;
    (void) input;
    (void) signature;

    _hidl_cb(ErrorCode::UNIMPLEMENTED, hidl_vec<KeyParameter>{}, hidl_vec<uint8_t>{});
    return Void();
}

Return<ErrorCode> KeymasterDevice::abort(uint64_t operationHandle) {
    LOG(VERBOSE) << "Running KeymasterDevice::abort";

    // TODO: implement
    LOG(ERROR) << "Not implemented";
    (void) operationHandle;

    return ErrorCode::UNIMPLEMENTED;
}

}  // namespace keymaster
}  // namespace hardware
}  // namespace android

/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "AuthSecret.h"

#include <android-base/logging.h>

#include <AuthSecret.h>

namespace android {
namespace hardware {
namespace authsecret {

// libhidl
using ::android::hardware::Void;

// Methods from ::android::hardware::authsecret::V1_0::IAuthSecret follow.
Return<void> AuthSecret::primaryUserCredential(const hidl_vec<uint8_t>& secret) {
    LOG(INFO) << "Running AuthSecret::primaryUserCredential";
    (void) secret;
    // TODO: implement
    return Void();
}

} // namespace authsecret
} // namespace hardware
} // namespace android

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

#include <android-base/logging.h>
#include <hidl/HidlTransportSupport.h>
#include <utils/StrongPointer.h>

#include <nugget/AppClient.h>
#include <nugget/CitadelClient.h>

#include <Weaver.h>

// TODO: get this from the nugget repo
#define APP_ID_WEAVER 0

using ::android::OK;
using ::android::sp;
using ::android::status_t;
using ::android::hardware::configureRpcThreadpool;
using ::android::hardware::joinRpcThreadpool;

using ::android::hardware::weaver::Weaver;

using ::nugget::CitadelClient;
using ::nugget::AppClient;

int main() {
    LOG(INFO) << "Weaver HAL service starting";

    // Connect to Citadel
    CitadelClient citadel;
    citadel.open();
    if (!citadel.isOpen()) {
        LOG(FATAL) << "Failed to open Citadel client";
    }

    // This thread will become the only thread of the daemon
    constexpr bool thisThreadWillJoinPool = true;
    configureRpcThreadpool(1, thisThreadWillJoinPool);

    // Start the HAL service
    AppClient weaverClient{citadel, APP_ID_WEAVER};
    sp<Weaver> weaver = new Weaver{weaverClient};
    const status_t status = weaver->registerAsService();
    if (status != OK) {
      LOG(FATAL) << "Failed to register Weaver as a service (status: " << status << ")";
    }

    joinRpcThreadpool();
    return -1; // Should never be reached
}

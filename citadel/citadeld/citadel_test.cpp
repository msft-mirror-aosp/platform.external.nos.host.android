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

#include <iostream>

#include <android-base/logging.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>

#include <android/hardware/citadel/ICitadeld.h>

#include <application.h>

using ::android::defaultServiceManager;
using ::android::sp;
using ::android::ProcessState;

using ::android::hardware::citadel::ICitadeld;

/**
 * This utility invokes the citadeld device checks and reports the results.
 */
int main(int argc, char** argv) {
    (void) argc;
    (void) argv;

    std::cout << "Checking citadel is present and responsive...\n";

    // Ensure this process is using the vndbinder
    ProcessState::initWithDriver("/dev/vndbinder");
    ::android::sp<::android::hardware::citadel::ICitadeld> _citadeld
            = ICitadeld::asInterface(defaultServiceManager()->getService(ICitadeld::descriptor));
    if (_citadeld == nullptr) {
        std::cerr << "Failed to connect to citadeld\n";
        return EXIT_FAILURE;
    }

    // Invoke the checks
    bool passed = false;
    if (!_citadeld->checkDevice(&passed).isOk()) {
        std::cerr << "Failed to run device checks via citadeld\n";
        return EXIT_FAILURE;
    }

    // Report the outcome
    if (passed) {
        std::cout << "\nPASS PASS PASS\n\n";
        return EXIT_SUCCESS;
    } else {
        std::cerr << "\nFAIL FAIL FAIL (see citadeld logcat for details)\n\n";
        return EXIT_FAILURE;
    }
}

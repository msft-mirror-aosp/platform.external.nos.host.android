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

#include "check.h"

#include <iomanip>
#include <sstream>

#include <android-base/logging.h>

#include <application.h>
#include <app_nugget.h>

#include <nos/debug.h>
#include <nos/transport.h>

/* TPM Register magic commands */
#define RD4_TPM_DID_VID 0x83d40f00
#define WR1_TPM_FW_VER  0x00d40f90

namespace android {
namespace citadeld {

namespace {

bool CheckDatagram(nos_device* const dev) {
    int rv;
    uint8_t buf[4];
    const uint8_t expected_tpm_did_vid[] = { 0xe0, 0x1a, 0x28, 0x00 };
    static_assert(sizeof(buf) >= sizeof(expected_tpm_did_vid),
                  "buf not large enough");

    // Clear the version string pointer
    LOG(INFO) << "Check: simple write";
    buf[0] = 0x00;
    rv = dev->ops.write(dev->ctx, WR1_TPM_FW_VER, buf, 1);
    if (rv != 0) {
        LOG(ERROR) << "  Failed with 0x" << std::hex << rv << " (" << std::dec << rv << ")";
        return false;
    }
    LOG(INFO) << "  Passed";

    // Read TPM_DID_VID
    LOG(INFO) << "Check: simple read";
    uint32_t buflen = sizeof(expected_tpm_did_vid);
    memset(buf, 0, buflen);
    rv = dev->ops.read(dev->ctx, RD4_TPM_DID_VID, buf, buflen);
    if (rv != 0) {
        LOG(ERROR) << "  Failed with 0x" << std::hex << rv << " (" << std::dec << rv << ")";
        return false;
    } else {
        std::ostringstream ss;
        ss << "got:" << std::setfill('0') << std::setw(2) << std::hex;
        for (uint32_t i = 0; i < buflen; ++i) {
            ss << " " << buf[i];
        }
        LOG(DEBUG) << ss.str();
        if (memcmp(buf, expected_tpm_did_vid, sizeof(expected_tpm_did_vid))) {
            LOG(ERROR) << "  Returned unexpected values";
            return false;
        }
    }
    LOG(INFO) << "  Passed";
    return true;
}

bool CheckTransport(nos_device* const dev) {
    LOG(INFO) << "Check: get version string";
    uint8_t buf[520];
    uint32_t replycount = sizeof(buf);
    uint32_t retval = nos_call_application(dev, APP_ID_NUGGET, NUGGET_PARAM_VERSION,
                                           buf, 0,
                                           buf, &replycount);
    if (retval != 0) {
        LOG(ERROR) << "  Failed with 0x" << std::hex << retval
                   << " (" << nos::StatusCodeString(retval) << ")";
        return false;
    }
    if (replycount < 4 || replycount > 512) {
        LOG(ERROR) << "  Returned " << replycount << " bytes, which seems wrong";
        return false;
    }
    LOG(INFO) << "  Passed";
    return true;
}

} // namespace

bool CheckDevice(nos_device* const dev)
{
    if (dev == nullptr) {
        LOG(ERROR) << "Checks require non-null device";
        return false;
    }

    bool all_passed = true;

    LOG(INFO) << "----";
    LOG(INFO) << "Datagram checks begin...";
    if (!CheckDatagram(dev)) {
        LOG(ERROR) << "Datagram checks failed";
        all_passed = false;
    } else {
        LOG(INFO) << "Datagram checks passed";
    }

    LOG(INFO) << "----";
    LOG(INFO) << "Transport checks begin...";
    if (!CheckTransport(dev)) {
        LOG(ERROR) << "Transport checks failed";
        all_passed = false;
    } else {
        LOG(INFO) << "Transport checks passed";
    }

    // TODO: add more e.g. GPIO


    LOG(INFO) << "----";
    return all_passed;
}

} // namespace citadeld
} // namespace android

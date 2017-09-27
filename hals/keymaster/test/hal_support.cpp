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

#include <hal_support.h>

using ::android::hardware::keymaster::V3_0::KeyParameter;
using ::android::hardware::keymaster::V3_0::Tag;

bool operator==(const KeyParameter& a, const KeyParameter& b) {
    if (a.tag != b.tag) {
        return false;
    }

    switch (a.tag) {

    /* Boolean tags */
    case Tag::INVALID:
    case Tag::CALLER_NONCE:
    case Tag::INCLUDE_UNIQUE_ID:
    case Tag::ECIES_SINGLE_HASH_MODE:
    case Tag::BOOTLOADER_ONLY:
    case Tag::NO_AUTH_REQUIRED:
    case Tag::ALLOW_WHILE_ON_BODY:
    case Tag::EXPORTABLE:
    case Tag::ALL_APPLICATIONS:
    case Tag::ROLLBACK_RESISTANT:
    case Tag::RESET_SINCE_ID_ROTATION:
        return true;

    /* Integer tags */
    case Tag::KEY_SIZE:
    case Tag::MIN_MAC_LENGTH:
    case Tag::MIN_SECONDS_BETWEEN_OPS:
    case Tag::MAX_USES_PER_BOOT:
    case Tag::ALL_USERS:
    case Tag::USER_ID:
    case Tag::OS_VERSION:
    case Tag::OS_PATCHLEVEL:
    case Tag::MAC_LENGTH:
    case Tag::AUTH_TIMEOUT:
        return a.f.integer == b.f.integer;

    /* Long integer tags */
    case Tag::RSA_PUBLIC_EXPONENT:
    case Tag::USER_SECURE_ID:
        return a.f.longInteger == b.f.longInteger;

    /* Date-time tags */
    case Tag::ACTIVE_DATETIME:
    case Tag::ORIGINATION_EXPIRE_DATETIME:
    case Tag::USAGE_EXPIRE_DATETIME:
    case Tag::CREATION_DATETIME:
        return a.f.dateTime == b.f.dateTime;

    /* Bytes tags */
    case Tag::APPLICATION_ID:
    case Tag::APPLICATION_DATA:
    case Tag::ROOT_OF_TRUST:
    case Tag::UNIQUE_ID:
    case Tag::ATTESTATION_CHALLENGE:
    case Tag::ATTESTATION_APPLICATION_ID:
    case Tag::ATTESTATION_ID_BRAND:
    case Tag::ATTESTATION_ID_DEVICE:
    case Tag::ATTESTATION_ID_PRODUCT:
    case Tag::ATTESTATION_ID_SERIAL:
    case Tag::ATTESTATION_ID_IMEI:
    case Tag::ATTESTATION_ID_MEID:
    case Tag::ATTESTATION_ID_MANUFACTURER:
    case Tag::ATTESTATION_ID_MODEL:
    case Tag::ASSOCIATED_DATA:
    case Tag::NONCE:
    case Tag::AUTH_TOKEN:
        return a.blob == b.blob;

    /* Enum tags */
    case Tag::PURPOSE:
        return a.f.purpose == b.f.purpose;
    case Tag::ALGORITHM:
        return a.f.algorithm == b.f.algorithm;
    case Tag::BLOCK_MODE:
        return a.f.blockMode == b.f.blockMode;
    case Tag::DIGEST:
        return a.f.digest == b.f.digest;
    case Tag::PADDING:
        return a.f.paddingMode == b.f.paddingMode;
    case Tag::EC_CURVE:
        return a.f.ecCurve == b.f.ecCurve;
    case Tag::BLOB_USAGE_REQUIREMENTS:
        return a.f.keyBlobUsageRequirements == b.f.keyBlobUsageRequirements;
    case Tag::USER_AUTH_TYPE:
        return a.f.integer == b.f.integer;
    case Tag::ORIGIN:
        return a.f.origin == b.f.origin;

    /* Unsupported tags */
    case Tag::KDF:
        return false;
    }
}

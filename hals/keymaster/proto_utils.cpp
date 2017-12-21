/*
 * Copyright 2017 The Android Open Source Project
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

#include "proto_utils.h"

#include <android-base/logging.h>
#include <android/hardware/keymaster/4.0/types.h>

namespace android {
namespace hardware {
namespace keymaster {

// HAL
using ::android::hardware::keymaster::V4_0::Algorithm;
using ::android::hardware::keymaster::V3_0::BlockMode;
using ::android::hardware::keymaster::V3_0::Digest;
using ::android::hardware::keymaster::V3_0::EcCurve;
using ::android::hardware::keymaster::V3_0::HardwareAuthenticatorType;
using ::android::hardware::keymaster::V3_0::KeyDerivationFunction;
using ::android::hardware::keymaster::V4_0::KeyOrigin;
using ::android::hardware::keymaster::V4_0::KeyPurpose;
using ::android::hardware::keymaster::V3_0::KeyBlobUsageRequirements;
using ::android::hardware::keymaster::V3_0::PaddingMode;
using ::android::hardware::keymaster::V4_0::Tag;
using ::android::hardware::keymaster::V3_0::TagType;

static TagType type_from_tag(Tag tag)
{
    return static_cast<TagType>(tag & (0xF << 28));
}

static nosapp::TagType type_from_tag(nosapp::Tag tag)
{
    return static_cast<nosapp::TagType>(tag & (0xF << 16));
}

static nosapp::Tag translate_tag(Tag tag)
{
    enum TagType tag_type = type_from_tag(tag);

    /* TODO:  This is gross.  The alternative is to have a complete table. */
    return static_cast<nosapp::Tag>(
        (((uint32_t)tag_type >> 28) << 16) | (tag & 0xffff));
}

static Tag translate_tag(nosapp::Tag tag)
{
    enum nosapp::TagType tag_type = type_from_tag(tag);

    /* TODO:  This is gross.  The alternative is to have a complete table. */
    return static_cast<Tag>(
        (((uint32_t)tag_type >> 16) << 28) | (tag & 0xffff));
}

static nosapp::KeyPurpose translate_purpose(KeyPurpose purpose)
{
    switch (purpose) {
    case KeyPurpose::ENCRYPT:
        return nosapp::KeyPurpose::ENCRYPT;
    case KeyPurpose::DECRYPT:
        return nosapp::KeyPurpose::DECRYPT;
    case KeyPurpose::SIGN:
        return nosapp::KeyPurpose::SIGN;
    case KeyPurpose::VERIFY:
        return nosapp::KeyPurpose::VERIFY;
    case KeyPurpose::WRAP_KEY:
        return nosapp::KeyPurpose::WRAP_KEY;
    default:
        return nosapp::KeyPurpose::PURPOSE_MAX;
    }
}

static ErrorCode translate_purpose(nosapp::KeyPurpose purpose, KeyPurpose *out)
{
    switch (purpose) {
    case nosapp::KeyPurpose::ENCRYPT:
        *out = KeyPurpose::ENCRYPT;
        break;
    case nosapp::KeyPurpose::DECRYPT:
        *out = KeyPurpose::DECRYPT;
        break;
    case nosapp::KeyPurpose::SIGN:
        *out = KeyPurpose::SIGN;
        break;
    case nosapp::KeyPurpose::VERIFY:
        *out = KeyPurpose::VERIFY;
        break;
    case nosapp::KeyPurpose::WRAP_KEY:
        *out = KeyPurpose::WRAP_KEY;
        break;
    default:
        return ErrorCode::UNKNOWN_ERROR;
    }

    return ErrorCode::OK;
}

static nosapp::Algorithm translate_algorithm(Algorithm algorithm)
{
    switch (algorithm) {
    case Algorithm::RSA:
        return nosapp::Algorithm::RSA;
    case Algorithm::EC:
        return nosapp::Algorithm::EC;
    case Algorithm::AES:
        return nosapp::Algorithm::AES;
    case Algorithm::DES:
        return nosapp::Algorithm::DES;
    case Algorithm::HMAC:
        return nosapp::Algorithm::HMAC;
    default:
        return nosapp::Algorithm::ALGORITHM_MAX;
    }
}

static ErrorCode translate_algorithm(nosapp::Algorithm algorithm,
                                     Algorithm *out)
{
    switch (algorithm) {
    case nosapp::Algorithm::RSA:
        *out = Algorithm::RSA;
        break;
    case nosapp::Algorithm::EC:
        *out = Algorithm::EC;
        break;
    case nosapp::Algorithm::AES:
        *out = Algorithm::AES;
        break;
    case nosapp::Algorithm::DES:
        *out = Algorithm::DES;
        break;
    case nosapp::Algorithm::HMAC:
        *out = Algorithm::HMAC;
        break;
    default:
        return ErrorCode::UNKNOWN_ERROR;
    }

    return ErrorCode::OK;
}

static nosapp::BlockMode translate_block_mode(BlockMode block_mode)
{
    switch (block_mode) {
    case BlockMode::ECB:
        return nosapp::BlockMode::ECB;
    case BlockMode::CBC:
        return nosapp::BlockMode::CBC;
    case BlockMode::CTR:
        return nosapp::BlockMode::CTR;
    case BlockMode::GCM:
        return nosapp::BlockMode::GCM;
    default:
        return nosapp::BlockMode::BLOCK_MODE_MAX;
    }
}

static ErrorCode translate_block_mode(nosapp::BlockMode block_mode,
                                      BlockMode *out)
{
    switch (block_mode) {
    case nosapp::BlockMode::ECB:
        *out = BlockMode::ECB;
        break;
    case nosapp::BlockMode::CBC:
        *out = BlockMode::CBC;
        break;
    case nosapp::BlockMode::CTR:
        *out = BlockMode::CTR;
        break;
    case nosapp::BlockMode::GCM:
        *out = BlockMode::GCM;
        break;
    default:
        return ErrorCode::UNKNOWN_ERROR;
        break;
    }

    return ErrorCode::OK;
}

static nosapp::Digest translate_digest(Digest digest)
{
    switch (digest) {
    case Digest::NONE:
        return nosapp::Digest::DIGEST_NONE;
    case Digest::MD5:
        return nosapp::Digest::DIGEST_MD5;
    case Digest::SHA1:
        return nosapp::Digest::DIGEST_SHA1;
    case Digest::SHA_2_224:
        return nosapp::Digest::DIGEST_SHA_2_224;
    case Digest::SHA_2_256:
        return nosapp::Digest::DIGEST_SHA_2_256;
    case Digest::SHA_2_384:
        return nosapp::Digest::DIGEST_SHA_2_384;
    case Digest::SHA_2_512:
        return nosapp::Digest::DIGEST_SHA_2_512;
    default:
        return nosapp::Digest::DIGEST_MAX;
    }
}

static ErrorCode translate_digest(nosapp::Digest digest,
                                  Digest *out)
{
    switch (digest) {
    case nosapp::Digest::DIGEST_NONE:
        *out = Digest::NONE;
        break;
    case nosapp::Digest::DIGEST_MD5:
        *out = Digest::MD5;
        break;
    case nosapp::Digest::DIGEST_SHA1:
        *out = Digest::SHA1;
        break;
    case nosapp::Digest::DIGEST_SHA_2_224:
        *out = Digest::SHA_2_224;
        break;
    case nosapp::Digest::DIGEST_SHA_2_256:
        *out = Digest::SHA_2_256;
        break;
    case nosapp::Digest::DIGEST_SHA_2_384:
        *out = Digest::SHA_2_384;
        break;
    case nosapp::Digest::DIGEST_SHA_2_512:
        *out = Digest::SHA_2_512;
        break;
    default:
        return ErrorCode::UNKNOWN_ERROR;
    }

    return ErrorCode::OK;
}

static nosapp::PaddingMode translate_padding_mode(PaddingMode padding_mode)
{
    switch (padding_mode) {
    case PaddingMode::NONE:
        return nosapp::PaddingMode::PADDING_NONE;
    case PaddingMode::RSA_OAEP:
        return nosapp::PaddingMode::PADDING_RSA_OAEP;
    case PaddingMode::RSA_PSS:
        return nosapp::PaddingMode::PADDING_RSA_PSS;
    case PaddingMode::RSA_PKCS1_1_5_ENCRYPT:
        return nosapp::PaddingMode::PADDING_RSA_PKCS1_1_5_ENCRYPT;
    case PaddingMode::RSA_PKCS1_1_5_SIGN:
        return nosapp::PaddingMode::PADDING_RSA_PKCS1_1_5_SIGN;
    case PaddingMode::PKCS7:
        return nosapp::PaddingMode::PADDING_PKCS7;
    default:
        return nosapp::PaddingMode::PADDING_MODE_MAX;
    }
}

static ErrorCode translate_padding_mode(nosapp::PaddingMode padding_mode,
    PaddingMode *out)
{
    switch (padding_mode) {
    case nosapp::PaddingMode::PADDING_NONE:
        *out = PaddingMode::NONE;
        break;
    case nosapp::PaddingMode::PADDING_RSA_OAEP:
        *out = PaddingMode::RSA_OAEP;
        break;
    case nosapp::PaddingMode::PADDING_RSA_PSS:
        *out = PaddingMode::RSA_PSS;
        break;
    case nosapp::PaddingMode::PADDING_RSA_PKCS1_1_5_ENCRYPT:
        *out = PaddingMode::RSA_PKCS1_1_5_ENCRYPT;
        break;
    case nosapp::PaddingMode::PADDING_RSA_PKCS1_1_5_SIGN:
        *out = PaddingMode::RSA_PKCS1_1_5_SIGN;
        break;
    case nosapp::PaddingMode::PADDING_PKCS7:
        *out = PaddingMode::PKCS7;
        break;
    default:
        return ErrorCode::UNKNOWN_ERROR;
    }

    return ErrorCode::OK;
}

static nosapp::EcCurve translate_ec_curve(EcCurve ec_curve)
{
    switch (ec_curve) {
    case EcCurve::P_224:
        return nosapp::EcCurve::P_224;
    case EcCurve::P_256:
        return nosapp::EcCurve::P_256;
    case EcCurve::P_384:
        return nosapp::EcCurve::P_384;
    case EcCurve::P_521:
        return nosapp::EcCurve::P_521;
    default:
        return nosapp::EcCurve::EC_CURVE_MAX;
    }
}

static ErrorCode translate_ec_curve(nosapp::EcCurve ec_curve, EcCurve *out)
{
    switch (ec_curve) {
    case nosapp::EcCurve::P_224:
        *out = EcCurve::P_224;
        break;
    case nosapp::EcCurve::P_256:
        *out = EcCurve::P_256;
        break;
    case nosapp::EcCurve::P_384:
        *out = EcCurve::P_384;
        break;
    case nosapp::EcCurve::P_521:
        *out = EcCurve::P_521;
        break;
    default:
        return ErrorCode::UNKNOWN_ERROR;
    }

    return ErrorCode::OK;
}

static nosapp::KeyBlobUsageRequirements translate_key_blob_usage_requirements(
    KeyBlobUsageRequirements usage)
{
    switch (usage) {
    case KeyBlobUsageRequirements::STANDALONE:
        return nosapp::KeyBlobUsageRequirements::STANDALONE;
    case KeyBlobUsageRequirements::REQUIRES_FILE_SYSTEM:
        return nosapp::KeyBlobUsageRequirements::REQUIRES_FILE_SYSTEM;
    default:
        return nosapp::KeyBlobUsageRequirements::KEY_USAGE_MAX;
        break;
    }
}

static ErrorCode translate_key_blob_usage_requirements(
    nosapp::KeyBlobUsageRequirements usage, KeyBlobUsageRequirements *out)
{
    switch (usage) {
    case nosapp::KeyBlobUsageRequirements::STANDALONE:
        *out = KeyBlobUsageRequirements::STANDALONE;
        break;
    case nosapp::KeyBlobUsageRequirements::REQUIRES_FILE_SYSTEM:
        *out = KeyBlobUsageRequirements::REQUIRES_FILE_SYSTEM;
        break;
    default:
        return ErrorCode::UNKNOWN_ERROR;
    }

    return ErrorCode::UNKNOWN_ERROR;
}

static nosapp::HardwareAuthenticatorType translate_hardware_authenticator_type(
    HardwareAuthenticatorType authenticator_type)
{
    switch (authenticator_type) {
    case HardwareAuthenticatorType::NONE:
        return nosapp::HardwareAuthenticatorType::AUTH_NONE;
    case HardwareAuthenticatorType::PASSWORD:
        return nosapp::HardwareAuthenticatorType::AUTH_PASSWORD;
    case HardwareAuthenticatorType::FINGERPRINT:
        return nosapp::HardwareAuthenticatorType::AUTH_FINGERPRINT;
    case HardwareAuthenticatorType::ANY:
        return nosapp::HardwareAuthenticatorType::AUTH_ANY;
    default:
        return nosapp::HardwareAuthenticatorType::AUTH_MAX;
    }
}

static ErrorCode translate_hardware_authenticator_type(
    nosapp::HardwareAuthenticatorType authenticator_type,
    HardwareAuthenticatorType *out)
{
    switch (authenticator_type) {
    case nosapp::HardwareAuthenticatorType::AUTH_NONE:
        *out = HardwareAuthenticatorType::NONE;
        break;
    case nosapp::HardwareAuthenticatorType::AUTH_PASSWORD:
        *out = HardwareAuthenticatorType::PASSWORD;
        break;
    case nosapp::HardwareAuthenticatorType::AUTH_FINGERPRINT:
        *out = HardwareAuthenticatorType::FINGERPRINT;
        break;
    case nosapp::HardwareAuthenticatorType::AUTH_ANY:
        *out = HardwareAuthenticatorType::ANY;
        break;
    default:
        return ErrorCode::UNKNOWN_ERROR;
    }

    return ErrorCode::OK;
}

static nosapp::KeyOrigin translate_key_origin(KeyOrigin key_origin)
{
    switch (key_origin) {
    case KeyOrigin::GENERATED:
        return nosapp::KeyOrigin::GENERATED;
    case KeyOrigin::DERIVED:
        return nosapp::KeyOrigin::DERIVED;
    case KeyOrigin::IMPORTED:
        return nosapp::KeyOrigin::IMPORTED;
    case KeyOrigin::UNKNOWN:
        return nosapp::KeyOrigin::UNKNOWN;
    default:
        return nosapp::KeyOrigin::KEY_ORIGIN_MAX;
    }
}

static ErrorCode translate_key_origin(nosapp::KeyOrigin key_origin,
                                      KeyOrigin *out)
{
    switch (key_origin) {
    case nosapp::KeyOrigin::GENERATED:
        *out = KeyOrigin::GENERATED;
        break;
    case nosapp::KeyOrigin::DERIVED:
        *out = KeyOrigin::DERIVED;
        break;
    case nosapp::KeyOrigin::IMPORTED:
        *out = KeyOrigin::IMPORTED;
        break;
    case nosapp::KeyOrigin::UNKNOWN:
        *out = KeyOrigin::UNKNOWN;
        break;
    default:
        return ErrorCode::UNKNOWN_ERROR;
    }

    return ErrorCode::OK;
}

ErrorCode key_parameter_to_pb(const KeyParameter& param,
                              nosapp::KeyParameter *pb)
{
    switch (param.tag) {
    case Tag::INVALID:
        LOG(ERROR) << "key_parameter_to_pb: invalid Tag received: "
                   << (uint32_t)param.tag;
        return ErrorCode::INVALID_ARGUMENT;
        break;
    case Tag::PURPOSE: // (TagType:ENUM_REP | 1)
        pb->set_integer((uint32_t)translate_purpose(param.f.purpose));
        break;
    case Tag::ALGORITHM: // (TagType:ENUM | 2)
        pb->set_integer((uint32_t)translate_algorithm(param.f.algorithm));
        break;
    case Tag::KEY_SIZE: // (TagType:UINT | 3)
        pb->set_integer(param.f.integer);
        break;
    case Tag::BLOCK_MODE: // (TagType:ENUM_REP | 4)
        pb->set_integer((uint32_t)translate_block_mode(param.f.blockMode));
        break;
    case Tag::DIGEST: // (TagType:ENUM_REP | 5)
        pb->set_integer((uint32_t)translate_digest(param.f.digest));
        break;
    case Tag::PADDING:; // (TagType:ENUM_REP | 6)
        pb->set_integer((uint32_t)translate_padding_mode(param.f.paddingMode));
        break;
    case Tag::CALLER_NONCE: // (TagType:BOOL | 7)
        pb->set_integer(param.f.boolValue);
        break;
    case Tag::MIN_MAC_LENGTH: // (TagType:UINT | 8)
        pb->set_integer(param.f.integer);
        break;
    case Tag::EC_CURVE: // (TagType:ENUM | 10)
        pb->set_integer((uint32_t)translate_ec_curve(param.f.ecCurve));
        break;
    case Tag::RSA_PUBLIC_EXPONENT: // (TagType:ULONG | 200)
        pb->set_integer(param.f.integer);
        break;
    case Tag::INCLUDE_UNIQUE_ID: // (TagType:BOOL | 202)
        pb->set_integer(param.f.boolValue);
        break;
    case Tag::BLOB_USAGE_REQUIREMENTS: // (TagType:ENUM | 301)
        pb->set_integer((uint32_t)translate_key_blob_usage_requirements(
                            param.f.keyBlobUsageRequirements));
        break;
    case Tag::BOOTLOADER_ONLY: // (TagType:BOOL | 302)
        pb->set_integer(param.f.boolValue);
        break;
    case Tag::ROLLBACK_RESISTANCE: // (TagType:BOOL | 303)
        pb->set_integer(param.f.boolValue);
        break;
    case Tag::HARDWARE_TYPE: // (TagType:ENUM | 304)
        break;
    case Tag::ACTIVE_DATETIME: // (TagType:DATE | 400)
        pb->set_long_integer(param.f.dateTime);
        break;
    case Tag::ORIGINATION_EXPIRE_DATETIME: // (TagType:DATE | 401)
    case Tag::USAGE_EXPIRE_DATETIME: // (TagType:DATE | 402)
        pb->set_long_integer(param.f.dateTime);
        break;
    case Tag::MIN_SECONDS_BETWEEN_OPS: // (TagType:UINT | 403)
    case Tag::MAX_USES_PER_BOOT: // (TagType:UINT | 404)
        pb->set_integer(param.f.integer);
        break;
    case Tag::USER_SECURE_ID: // (TagType:ULONG_REP | 502)
        pb->set_long_integer(param.f.longInteger);
        break;
    case Tag::NO_AUTH_REQUIRED: // (TagType:BOOL | 503)
        pb->set_integer(param.f.boolValue);
        break;
    case Tag::USER_AUTH_TYPE: // (TagType:ENUM | 504)
        pb->set_integer((uint32_t)translate_hardware_authenticator_type(
                            param.f.hardwareAuthenticatorType));
        break;
    case Tag::AUTH_TIMEOUT: // (TagType:UINT | 505)
        pb->set_integer(param.f.integer);
        break;
    case Tag::ALLOW_WHILE_ON_BODY: // (TagType:BOOL | 506)
        pb->set_integer(param.f.boolValue);
        break;
    case Tag::APPLICATION_ID: // (TagType:BYTES | 601)
        pb->set_blob(&param.blob[0], param.blob.size());
        break;
    case Tag::APPLICATION_DATA: // (TagType:BYTES | 700)
        pb->set_blob(&param.blob[0], param.blob.size());
        break;
    case Tag::CREATION_DATETIME: // (TagType:DATE | 701)
        pb->set_long_integer(param.f.dateTime);
        break;
    case Tag::ORIGIN: // (TagType:ENUM | 702)
        pb->set_integer((uint32_t)translate_key_origin(param.f.origin));
        break;
    case Tag::ROOT_OF_TRUST: // (TagType:BYTES | 704)
        pb->set_blob(&param.blob[0], param.blob.size());
        break;
    case Tag::OS_VERSION: // (TagType:UINT | 705)
    case Tag::OS_PATCHLEVEL: // (TagType:UINT | 706)
        pb->set_integer(param.f.integer);
        break;
    case Tag::UNIQUE_ID: // (TagType:BYTES | 707)
    case Tag::ATTESTATION_CHALLENGE: // (TagType:BYTES | 708)
    case Tag::ATTESTATION_APPLICATION_ID: // (TagType:BYTES | 709)
    case Tag::ATTESTATION_ID_BRAND: // (TagType:BYTES | 710)
    case Tag::ATTESTATION_ID_DEVICE: // (TagType:BYTES | 711)
    case Tag::ATTESTATION_ID_PRODUCT: // (TagType:BYTES | 712)
    case Tag::ATTESTATION_ID_SERIAL: // (TagType:BYTES | 713)
    case Tag::ATTESTATION_ID_IMEI: // (TagType:BYTES | 714)
    case Tag::ATTESTATION_ID_MEID: // (TagType:BYTES | 715)
    case Tag::ATTESTATION_ID_MANUFACTURER: // (TagType:BYTES | 716)
    case Tag::ATTESTATION_ID_MODEL: // (TagType:BYTES | 717)
    case Tag::ASSOCIATED_DATA: // (TagType:BYTES | 1000)
    case Tag::NONCE: // (TagType:BYTES | 1001)
        pb->set_blob(&param.blob[0], param.blob.size());
        break;
    case Tag::MAC_LENGTH: // (TagType:UINT | 1003)
        pb->set_integer(param.f.integer);
        break;
    case Tag::RESET_SINCE_ID_ROTATION: // (TagType:BOOL | 1004)
        pb->set_integer(param.f.boolValue);
        break;
    }

    pb->set_tag(translate_tag(param.tag));
    return ErrorCode::OK;
}

ErrorCode pb_to_key_parameter(const nosapp::KeyParameter& param,
                              KeyParameter *kp)
{
    switch (param.tag()) {
    case nosapp::Tag::TAG_INVALID:
        LOG(ERROR) << "key_parameter_to_pb: invalid Tag received: "
                   << (uint32_t)param.tag();
        return ErrorCode::UNKNOWN_ERROR;
        break;
    case nosapp::Tag::PURPOSE: // (TagType:ENUM_REP | 1)
        if (translate_purpose(static_cast<nosapp::KeyPurpose>(param.integer()),
                              &kp->f.purpose) != ErrorCode::OK) {
            return ErrorCode::UNKNOWN_ERROR;
        }
        break;
    case nosapp::Tag::ALGORITHM: // (TagType:ENUM | 2)
        if (translate_algorithm(static_cast<nosapp::Algorithm>(param.integer()),
                                &kp->f.algorithm) != ErrorCode::OK) {
            return ErrorCode::UNKNOWN_ERROR;
        }
        break;
    case nosapp::Tag::KEY_SIZE: // (TagType:UINT | 3)
        kp->f.integer = param.integer();
        break;
    case nosapp::Tag::BLOCK_MODE: // (TagType:ENUM_REP | 4)
        if (translate_block_mode(
                static_cast<nosapp::BlockMode>(param.integer()),
                &kp->f.blockMode) != ErrorCode::OK) {
            return ErrorCode::UNKNOWN_ERROR;
        }
        break;
    case nosapp::Tag::DIGEST: // (TagType:ENUM_REP | 5)
        if (translate_digest(
                static_cast<nosapp::Digest>(param.integer()),
                &kp->f.digest) != ErrorCode::OK) {
            return ErrorCode::UNKNOWN_ERROR;
        }
        break;
    case nosapp::Tag::PADDING:; // (TagType:ENUM_REP | 6)
        if (translate_padding_mode(
                static_cast<nosapp::PaddingMode>(param.integer()),
                &kp->f.paddingMode) != ErrorCode::OK) {
            return ErrorCode::UNKNOWN_ERROR;
        }
        break;
    case nosapp::Tag::CALLER_NONCE: // (TagType:BOOL | 7)
        kp->f.boolValue = (bool)param.integer();
        break;
    case nosapp::Tag::MIN_MAC_LENGTH: // (TagType:UINT | 8)
        kp->f.integer = param.integer();
        break;
    case nosapp::Tag::EC_CURVE: // (TagType:ENUM | 10)
        if (translate_ec_curve(
                static_cast<nosapp::EcCurve>(param.integer()),
                &kp->f.ecCurve) != ErrorCode::OK) {
            return ErrorCode::UNKNOWN_ERROR;
        }
        break;
    case nosapp::Tag::RSA_PUBLIC_EXPONENT: // (TagType:ULONG | 200)
        kp->f.integer = param.integer();
        break;
    case nosapp::Tag::INCLUDE_UNIQUE_ID: // (TagType:BOOL | 202)
        kp->f.boolValue = (bool)param.integer();
        break;
    case nosapp::Tag::BLOB_USAGE_REQUIREMENTS: // (TagType:ENUM | 301)
        if (translate_key_blob_usage_requirements(
                static_cast<nosapp::KeyBlobUsageRequirements>(param.integer()),
                &kp->f.keyBlobUsageRequirements) != ErrorCode::OK) {
            return ErrorCode::UNKNOWN_ERROR;
        }
        break;
    case nosapp::Tag::BOOTLOADER_ONLY: // (TagType:BOOL | 302)
    case nosapp::Tag::ROLLBACK_RESISTANCE: // (TagType:BOOL | 303)
        kp->f.boolValue = (bool)param.integer();
        break;
    case nosapp::Tag::ACTIVE_DATETIME: // (TagType:DATE | 400)
    case nosapp::Tag::ORIGINATION_EXPIRE_DATETIME: // (TagType:DATE | 401)
    case nosapp::Tag::USAGE_EXPIRE_DATETIME: // (TagType:DATE | 402)
        kp->f.dateTime = param.long_integer();
        break;
    case nosapp::Tag::MIN_SECONDS_BETWEEN_OPS: // (TagType:UINT | 403)
    case nosapp::Tag::MAX_USES_PER_BOOT: // (TagType:UINT | 404)
        kp->f.integer = param.integer();
        break;
    case nosapp::Tag::USER_SECURE_ID: // (TagType:ULONG_REP | 502)
        kp->f.longInteger = param.long_integer();
        break;
    case nosapp::Tag::NO_AUTH_REQUIRED: // (TagType:BOOL | 503)
        kp->f.boolValue = (bool)param.integer();
        break;
    case nosapp::Tag::USER_AUTH_TYPE: // (TagType:ENUM | 504)
        if (translate_hardware_authenticator_type(
                static_cast<nosapp::HardwareAuthenticatorType>(param.integer()),
                &kp->f.hardwareAuthenticatorType) != ErrorCode::OK) {
            return ErrorCode::UNKNOWN_ERROR;
        }
        break;
    case nosapp::Tag::AUTH_TIMEOUT: // (TagType:UINT | 505)
        kp->f.integer = param.integer();
        break;
    case nosapp::Tag::ALLOW_WHILE_ON_BODY: // (TagType:BOOL | 506)
        kp->f.boolValue = (bool)param.integer();
        break;
    case nosapp::Tag::APPLICATION_ID: // (TagType:BYTES | 601)
        kp->blob.setToExternal(
            reinterpret_cast<uint8_t *>(
                const_cast<char *>(param.blob().data())), param.blob().size());;
        break;
    case nosapp::Tag::APPLICATION_DATA: // (TagType:BYTES | 700)
        kp->blob.setToExternal(
            reinterpret_cast<uint8_t *>(
                const_cast<char *>(param.blob().data())), param.blob().size());;
        break;
    case nosapp::Tag::CREATION_DATETIME: // (TagType:DATE | 701)
        kp->f.dateTime = param.long_integer();
        break;
    case nosapp::Tag::ORIGIN: // (TagType:ENUM | 702)
        if (translate_key_origin(
                static_cast<nosapp::KeyOrigin>(param.integer()),
                &kp->f.origin) != ErrorCode::OK) {
            return ErrorCode::UNKNOWN_ERROR;
        }
        break;
    case nosapp::Tag::ROOT_OF_TRUST: // (TagType:BYTES | 704)
        kp->blob.setToExternal(
            reinterpret_cast<uint8_t *>(
                const_cast<char *>(param.blob().data())), param.blob().size());
        break;
    case nosapp::Tag::OS_VERSION: // (TagType:UINT | 705)
    case nosapp::Tag::OS_PATCHLEVEL: // (TagType:UINT | 706)
        kp->f.integer = param.integer();
        break;
    case nosapp::Tag::UNIQUE_ID: // (TagType:BYTES | 707)
    case nosapp::Tag::ATTESTATION_CHALLENGE: // (TagType:BYTES | 708)
    case nosapp::Tag::ATTESTATION_APPLICATION_ID: // (TagType:BYTES | 709)
    case nosapp::Tag::ATTESTATION_ID_BRAND: // (TagType:BYTES | 710)
    case nosapp::Tag::ATTESTATION_ID_DEVICE: // (TagType:BYTES | 711)
    case nosapp::Tag::ATTESTATION_ID_PRODUCT: // (TagType:BYTES | 712)
    case nosapp::Tag::ATTESTATION_ID_SERIAL: // (TagType:BYTES | 713)
    case nosapp::Tag::ATTESTATION_ID_IMEI: // (TagType:BYTES | 714)
    case nosapp::Tag::ATTESTATION_ID_MEID: // (TagType:BYTES | 715)
    case nosapp::Tag::ATTESTATION_ID_MANUFACTURER: // (TagType:BYTES | 716)
    case nosapp::Tag::ATTESTATION_ID_MODEL: // (TagType:BYTES | 717)
    case nosapp::Tag::ASSOCIATED_DATA: // (TagType:BYTES | 1000)
    case nosapp::Tag::NONCE: // (TagType:BYTES | 1001)
        kp->blob.setToExternal(
            reinterpret_cast<uint8_t *>(
                const_cast<char *>(param.blob().data())), param.blob().size());
        break;
    case nosapp::Tag::MAC_LENGTH: // (TagType:UINT | 1003)
        kp->f.integer = param.integer();
        break;
    case nosapp::Tag::RESET_SINCE_ID_ROTATION: // (TagType:BOOL | 1004)
        kp->f.boolValue = (bool)param.integer();
        break;
    default:
        return ErrorCode::UNKNOWN_ERROR;
    }

    kp->tag = translate_tag(param.tag());
    return ErrorCode::OK;
}

ErrorCode hidl_params_to_pb(const hidl_vec<KeyParameter>& params,
                       nosapp::KeyParameters *pb)
{
    for (size_t i = 0; i < params.size(); i++) {
        nosapp::KeyParameter *param = pb->add_params();
        ErrorCode error = key_parameter_to_pb(params[i], param);
        if (error != ErrorCode::OK) {
            return ErrorCode::INVALID_ARGUMENT;
        }
    }

    return ErrorCode::OK;
}

ErrorCode hidl_params_to_map(const hidl_vec<KeyParameter>& params,
                             tag_map_t *tag_map)
{
    for (size_t i = 0; i < params.size(); i++) {
        switch (type_from_tag(params[i].tag)) {
        case TagType::INVALID:
            return ErrorCode::INVALID_ARGUMENT;
            break;
        case TagType::ENUM:
        case TagType::UINT:
        case TagType::ULONG:
        case TagType::DATE:
        case TagType::BOOL:
        case TagType::BIGNUM:
        case TagType::BYTES:
            if (tag_map->find(params[i].tag) != tag_map->end()) {
                // Duplicates not allowed for these tags types.
                return ErrorCode::INVALID_ARGUMENT;
            }
            /* Fall-through! */
        case TagType::ENUM_REP:
        case TagType::UINT_REP:
        case TagType::ULONG_REP:
            if (tag_map->find(params[i].tag) == tag_map->end()) {
                vector<KeyParameter> v{params[i]};
                tag_map->insert(
                    std::pair<Tag, vector<KeyParameter> >(
                        params[i].tag, v));
            } else {
                LOG(ERROR) << "params_to_map: never reached";
                (*tag_map)[params[i].tag].push_back(params[i]);
            }
            break;
        default:
            /* Unrecognized TagType. */
            return ErrorCode::INVALID_ARGUMENT;
            break;
        }
    }

    return ErrorCode::OK;
}

ErrorCode map_params_to_pb(const tag_map_t& params,
                           nosapp::KeyParameters *pbParams)
{
    for (const auto& it : params) {
        for (const auto& pt : it.second) {
            nosapp::KeyParameter *param = pbParams->add_params();
            ErrorCode error = key_parameter_to_pb(pt, param);
            if (error != ErrorCode::OK) {
                return error;
            }
        }
    }

    return ErrorCode::OK;
}

ErrorCode pb_to_hidl_params(const nosapp::KeyParameters& pbParams,
                            hidl_vec<KeyParameter> *params)
{
    std::vector<KeyParameter> kpv;
    for (size_t i = 0; i < (size_t)pbParams.params_size(); i++) {
        KeyParameter kp;
        const nosapp::KeyParameter& param = pbParams.params(i);

        ErrorCode error = pb_to_key_parameter(param, &kp);
        if (error != ErrorCode::OK) {
            return ErrorCode::UNKNOWN_ERROR;
        }

        kpv.push_back(kp);
    }

    *params = kpv;

    return ErrorCode::OK;
}

}  // namespace keymaster
}  // hardware
}  // android

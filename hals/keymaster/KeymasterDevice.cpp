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
#include "export_key.h"
#include "import_key.h"
#include "import_wrapped_key.h"
#include "proto_utils.h"

#include <Keymaster.client.h>
#include <nos/debug.h>
#include <nos/NuggetClient.h>

#include <keymasterV4_0/key_param_output.h>

#include <android-base/logging.h>
#include <android-base/properties.h>

#include <algorithm>

namespace android {
namespace hardware {
namespace keymaster {

namespace {

constexpr char PROPERTY_OS_VERSION[] = "ro.build.version.release";
constexpr char PROPERTY_OS_PATCHLEVEL[] = "ro.build.version.security_patch";
constexpr char PROPERTY_VENDOR_PATCHLEVEL[] = "ro.vendor.build.security_patch";

std::string DigitsOnly(const std::string& code) {
	// Keep digits only.
	std::string filtered_code;
	filtered_code.reserve(code.size());
	for (const auto c : code) {
		if (isdigit(c)) {
			filtered_code.push_back(c);
		}
	}
	return filtered_code;
}

uint32_t DateCodeToUint32(const std::string& code, bool include_day) {
    // Keep digits only.
    std::string filtered_code = DigitsOnly(code);

    // Return 0 if the date string has an unexpected number of digits.
    uint32_t return_value = 0;
    if (filtered_code.size() == 8) {
        return_value = std::stoi(filtered_code);
        if (!include_day) {
            return_value /= 100;
        }
    } else if (filtered_code.size() == 6) {
        return_value = std::stoi(filtered_code);
        if (include_day) {
            return_value *= 100;
        }
    }
    return return_value;
}

}  // namespace

// std
using std::string;

// base
using ::android::base::GetProperty;

// libhidl
using ::android::hardware::Void;

// HAL
using ::android::hardware::keymaster::V4_0::Algorithm;
using ::android::hardware::keymaster::V4_0::KeyCharacteristics;
using ::android::hardware::keymaster::V4_0::KeyFormat;
using ::android::hardware::keymaster::V4_0::HardwareAuthToken;
using ::android::hardware::keymaster::V4_0::HardwareAuthenticatorType;
using ::android::hardware::keymaster::V4_0::SecurityLevel;
using ::android::hardware::keymaster::V4_0::Tag;

// nos
using nos::NuggetClient;

// Keymaster app
// KM 3.0 types
using ::nugget::app::keymaster::AddRngEntropyRequest;
using ::nugget::app::keymaster::AddRngEntropyResponse;
using ::nugget::app::keymaster::GenerateKeyRequest;
using ::nugget::app::keymaster::GenerateKeyResponse;
using ::nugget::app::keymaster::GetKeyCharacteristicsRequest;
using ::nugget::app::keymaster::GetKeyCharacteristicsResponse;
using ::nugget::app::keymaster::ImportKeyRequest;
using ::nugget::app::keymaster::ImportKeyResponse;
using ::nugget::app::keymaster::ExportKeyRequest;
using ::nugget::app::keymaster::ExportKeyResponse;
using ::nugget::app::keymaster::AttestKeyRequest;
using ::nugget::app::keymaster::AttestKeyResponse;
using ::nugget::app::keymaster::UpgradeKeyRequest;
using ::nugget::app::keymaster::UpgradeKeyResponse;
using ::nugget::app::keymaster::DeleteKeyRequest;
using ::nugget::app::keymaster::DeleteKeyResponse;
using ::nugget::app::keymaster::DeleteAllKeysRequest;
using ::nugget::app::keymaster::DeleteAllKeysResponse;
using ::nugget::app::keymaster::DestroyAttestationIdsRequest;
using ::nugget::app::keymaster::DestroyAttestationIdsResponse;
using ::nugget::app::keymaster::BeginOperationRequest;
using ::nugget::app::keymaster::BeginOperationResponse;
using ::nugget::app::keymaster::UpdateOperationRequest;
using ::nugget::app::keymaster::UpdateOperationResponse;
using ::nugget::app::keymaster::FinishOperationRequest;
using ::nugget::app::keymaster::FinishOperationResponse;
using ::nugget::app::keymaster::AbortOperationRequest;
using ::nugget::app::keymaster::AbortOperationResponse;
using ::nugget::app::keymaster::ComputeSharedHmacRequest;
using ::nugget::app::keymaster::ComputeSharedHmacResponse;
using ::nugget::app::keymaster::GetHmacSharingParametersRequest;
using ::nugget::app::keymaster::GetHmacSharingParametersResponse;

// KM 4.0 types
using ::nugget::app::keymaster::ImportWrappedKeyRequest;
namespace nosapp = ::nugget::app::keymaster;

static ErrorCode status_to_error_code(uint32_t status)
{
    switch (status) {
    case APP_SUCCESS:
        return ErrorCode::OK;
        break;
    case APP_ERROR_BOGUS_ARGS:
        return ErrorCode::INVALID_ARGUMENT;
        break;
    case APP_ERROR_INTERNAL:
        return ErrorCode::UNKNOWN_ERROR;
        break;
    case APP_ERROR_TOO_MUCH:
        return ErrorCode::INSUFFICIENT_BUFFER_SPACE;
        break;
    case APP_ERROR_RPC:
        return ErrorCode::SECURE_HW_COMMUNICATION_FAILED;
        break;
    // TODO: app specific error codes go here.
    default:
        return ErrorCode::UNKNOWN_ERROR;
        break;
    }
}

#define KM_CALL(meth) {                                                       \
    const uint32_t status = _keymaster. meth (request, &response);            \
    const ErrorCode error_code = translate_error_code(response.error_code()); \
    if (status != APP_SUCCESS) {                                              \
        LOG(ERROR) << #meth << " : request failed with status: "              \
                   << nos::StatusCodeString(status);                          \
        return status_to_error_code(status);                                  \
    }                                                                         \
    if (error_code != ErrorCode::OK) {                                        \
        LOG(ERROR) << #meth << " : device response error code: "              \
                   << error_code;                                             \
        return error_code;                                                    \
    }                                                                         \
}

#define KM_CALLV(meth, ...) {                                                 \
    const uint32_t status = _keymaster. meth (request, &response);            \
    const ErrorCode error_code = translate_error_code(response.error_code()); \
    if (status != APP_SUCCESS) {                                              \
        LOG(ERROR) << #meth << " : request failed with status: "              \
                   << nos::StatusCodeString(status);                          \
        _hidl_cb(status_to_error_code(status), __VA_ARGS__);                  \
        return Void();                                                        \
    }                                                                         \
    if (error_code != ErrorCode::OK) {                                        \
        LOG(ERROR) << #meth << " : device response error code: "              \
                   << error_code;                                             \
        _hidl_cb(error_code, __VA_ARGS__);                                    \
        return Void();                                                        \
    }                                                                         \
}

// Methods from ::android::hardware::keymaster::V3_0::IKeymasterDevice follow.

KeymasterDevice::KeymasterDevice(KeymasterClient& keymaster) :
        _keymaster{keymaster} {
    os_version = std::stoi(DigitsOnly(GetProperty(PROPERTY_OS_VERSION, "")));
    os_patchlevel = DateCodeToUint32(GetProperty(PROPERTY_OS_PATCHLEVEL, ""),
                                     false);
    vendor_patchlevel = DateCodeToUint32(
            GetProperty(PROPERTY_VENDOR_PATCHLEVEL, ""), true);
    // TODO(b/80248239) Hand these off to citadel.
}

Return<void> KeymasterDevice::getHardwareInfo(
        getHardwareInfo_cb _hidl_cb)
{
    LOG(VERBOSE) << "Running KeymasterDevice::getHardwareInfo";

    (void)_keymaster;
    _hidl_cb(SecurityLevel::STRONGBOX,
             string("CitadelKeymaster"), string("Google"));

    return Void();
}

Return<void> KeymasterDevice::getHmacSharingParameters(
    getHmacSharingParameters_cb _hidl_cb)
{
    LOG(VERBOSE) << "Running KeymasterDevice::getHmacSharingParameters";

    GetHmacSharingParametersRequest request;
    GetHmacSharingParametersResponse response;
    HmacSharingParameters result;

    KM_CALLV(GetHmacSharingParameters, result);

    ErrorCode ec = translate_error_code(response.error_code());

    if (ec != ErrorCode::OK) {
        _hidl_cb(ec, HmacSharingParameters());
    }

    const std::string & nonce = response.hmac_sharing_params().nonce();
    const std::string & seed = response.hmac_sharing_params().seed();

    if (seed.size() == 32) {
        result.seed.setToExternal(reinterpret_cast<uint8_t*>(
                const_cast<char*>(seed.data())),
                seed.size(), false);
    } else if (seed.size() != 0) {
        LOG(ERROR) << "Citadel returned unexpected seed size: "
                << seed.size();
        _hidl_cb(ErrorCode::UNKNOWN_ERROR, HmacSharingParameters());
        return Void();
    }

    if (nonce.size() == result.nonce.size()) {
        std::copy(nonce.begin(), nonce.end(), result.nonce.data());
    } else {
        LOG(ERROR) << "Citadel returned unexpected nonce size: "
                << nonce.size();
        _hidl_cb(ErrorCode::UNKNOWN_ERROR, HmacSharingParameters());
        return Void();
    }

    _hidl_cb(ec, result);

    return Void();
}

Return<void> KeymasterDevice::computeSharedHmac(
    const hidl_vec<HmacSharingParameters>& params,
    computeSharedHmac_cb _hidl_cb)
{
    LOG(VERBOSE) << "Running KeymasterDevice::computeSharedHmac";

    ComputeSharedHmacRequest request;
    ComputeSharedHmacResponse response;
    hidl_vec<uint8_t> result;

    for (const HmacSharingParameters & param : params) {
        // TODO respect max number of parameters defined in
        // keymaster_types.proto
        nosapp::HmacSharingParameters* req_param =
                request.add_hmac_sharing_params();
        req_param->set_nonce(
                reinterpret_cast<const int8_t*>(
                param.nonce.data()), param.nonce.size());
        req_param->set_seed(reinterpret_cast<const int8_t*>(param.seed.data()),
                param.seed.size());
    }

    KM_CALLV(ComputeSharedHmac, result);

    ErrorCode ec = translate_error_code(response.error_code());

    if (ec != ErrorCode::OK) {
        _hidl_cb(ec, result);
        return Void();
    }

    const std::string & share_check = response.sharing_check();

    result.setToExternal(reinterpret_cast<uint8_t*>(
            const_cast<char*>(share_check.data())), share_check.size(), false);
    _hidl_cb(ec, result);

    return Void();
}

Return<void> KeymasterDevice::verifyAuthorization(
    uint64_t operationHandle, const hidl_vec<KeyParameter>& parametersToVerify,
    const HardwareAuthToken& authToken, verifyAuthorization_cb _hidl_cb)
{
    LOG(VERBOSE) << "Running KeymasterDevice::verifyAuthorization";

    (void)operationHandle;
    (void)parametersToVerify;
    (void)authToken;

    (void)_keymaster;
    _hidl_cb(ErrorCode::UNIMPLEMENTED, VerificationToken());

    return Void();
}

Return<ErrorCode> KeymasterDevice::addRngEntropy(const hidl_vec<uint8_t>& data)
{
    LOG(VERBOSE) << "Running KeymasterDevice::addRngEntropy";

    if (!data.size()) return ErrorCode::OK;

    const size_t chunk_size = 1024;
    for (size_t i = 0; i < data.size(); i += chunk_size) {
        AddRngEntropyRequest request;
        AddRngEntropyResponse response;

        request.set_data(&data[i], std::min(chunk_size, data.size() - i));

        // Call device.
        KM_CALL(AddRngEntropy);
    }

    return ErrorCode::OK;
}

Return<void> KeymasterDevice::generateKey(
        const hidl_vec<KeyParameter>& keyParams,
        generateKey_cb _hidl_cb)
{
    LOG(VERBOSE) << "Running KeymasterDevice::generateKey";

    GenerateKeyRequest request;
    GenerateKeyResponse response;

    hidl_vec<uint8_t> blob;
    KeyCharacteristics characteristics;
    if (hidl_params_to_pb(
            keyParams, request.mutable_params()) != ErrorCode::OK) {
      _hidl_cb(ErrorCode::INVALID_ARGUMENT, blob, characteristics);
      return Void();
    }

    // Call device.
    KM_CALLV(GenerateKey, hidl_vec<uint8_t>{}, KeyCharacteristics());

    blob.setToExternal(
        reinterpret_cast<uint8_t*>(
            const_cast<char*>(response.blob().blob().data())),
        response.blob().blob().size(), false);
    pb_to_hidl_params(response.characteristics().software_enforced(),
                      &characteristics.softwareEnforced);
    pb_to_hidl_params(response.characteristics().tee_enforced(),
                      &characteristics.hardwareEnforced);

    _hidl_cb(translate_error_code(response.error_code()),
             blob, characteristics);
    return Void();
}

Return<void> KeymasterDevice::getKeyCharacteristics(
        const hidl_vec<uint8_t>& keyBlob,
        const hidl_vec<uint8_t>& clientId,
        const hidl_vec<uint8_t>& appData,
        getKeyCharacteristics_cb _hidl_cb)
{
    LOG(VERBOSE) << "Running KeymasterDevice::getKeyCharacteristics";

    GetKeyCharacteristicsRequest request;
    GetKeyCharacteristicsResponse response;

    request.mutable_blob()->set_blob(&keyBlob[0], keyBlob.size());
    request.set_client_id(&clientId[0], clientId.size());
    request.set_app_data(&appData[0], appData.size());

    // Call device.
    KM_CALLV(GetKeyCharacteristics, KeyCharacteristics());

    KeyCharacteristics characteristics;
    pb_to_hidl_params(response.characteristics().software_enforced(),
                      &characteristics.softwareEnforced);
    pb_to_hidl_params(response.characteristics().tee_enforced(),
                      &characteristics.hardwareEnforced);

    _hidl_cb(translate_error_code(response.error_code()), characteristics);
    return Void();
}

Return<void> KeymasterDevice::importKey(
        const hidl_vec<KeyParameter>& params, KeyFormat keyFormat,
        const hidl_vec<uint8_t>& keyData, importKey_cb _hidl_cb)
{
    LOG(VERBOSE) << "Running KeymasterDevice::importKey";

    ErrorCode error;
    ImportKeyRequest request;
    ImportKeyResponse response;

    error = import_key_request(params, keyFormat, keyData, &request);
    if (error != ErrorCode::OK) {
        LOG(ERROR) << "ImportKey request parsing failed with error "
                   << error;
        _hidl_cb(error, hidl_vec<uint8_t>{}, KeyCharacteristics{});
        return Void();
    }

    KM_CALLV(ImportKey, hidl_vec<uint8_t>{}, KeyCharacteristics{});

    hidl_vec<uint8_t> blob;
    blob.setToExternal(
        reinterpret_cast<uint8_t*>(
            const_cast<char*>(response.blob().blob().data())),
        response.blob().blob().size(), false);

    KeyCharacteristics characteristics;
    pb_to_hidl_params(response.characteristics().software_enforced(),
                      &characteristics.softwareEnforced);
    error = pb_to_hidl_params(response.characteristics().tee_enforced(),
                              &characteristics.hardwareEnforced);
    if (error != ErrorCode::OK) {
        LOG(ERROR) << "KeymasterDevice::importKey: response tee_enforced :"
                   << error;
        _hidl_cb(error, hidl_vec<uint8_t>{}, KeyCharacteristics{});
        return Void();
    }

    _hidl_cb(ErrorCode::OK, blob, characteristics);
    return Void();
}

Return<void> KeymasterDevice::exportKey(
        KeyFormat exportFormat, const hidl_vec<uint8_t>& keyBlob,
        const hidl_vec<uint8_t>& clientId,
        const hidl_vec<uint8_t>& appData, exportKey_cb _hidl_cb)
{
    LOG(VERBOSE) << "Running KeymasterDevice::exportKey";

    ExportKeyRequest request;
    ExportKeyResponse response;

    request.set_format((::nugget::app::keymaster::KeyFormat)exportFormat);
    request.mutable_blob()->set_blob(&keyBlob[0], keyBlob.size());
    request.set_client_id(&clientId[0], clientId.size());
    request.set_app_data(&appData[0], appData.size());

    KM_CALLV(ExportKey, hidl_vec<uint8_t>{});

    ErrorCode error_code = translate_error_code(response.error_code());
    if (error_code != ErrorCode::OK) {
        _hidl_cb(error_code, hidl_vec<uint8_t>{});
    }

    hidl_vec<uint8_t> der;
    error_code = export_key_der(response, &der);

    _hidl_cb(error_code, der);
    return Void();
}

Return<void> KeymasterDevice::attestKey(
        const hidl_vec<uint8_t>& keyToAttest,
        const hidl_vec<KeyParameter>& attestParams,
        attestKey_cb _hidl_cb)
{
    LOG(VERBOSE) << "Running KeymasterDevice::attestKey";

    AttestKeyRequest request;
    AttestKeyResponse response;

    request.mutable_blob()->set_blob(&keyToAttest[0], keyToAttest.size());

    vector<hidl_vec<uint8_t> > chain;
    if (hidl_params_to_pb(
            attestParams, request.mutable_params()) != ErrorCode::OK) {
      _hidl_cb(ErrorCode::INVALID_ARGUMENT, chain);
      return Void();
    }

    KM_CALLV(AttestKey, hidl_vec<hidl_vec<uint8_t> >{});

    for (int i = 0; i < response.chain().certificates_size(); i++) {
        hidl_vec<uint8_t> blob;
        blob.setToExternal(
            reinterpret_cast<uint8_t*>(
                const_cast<char*>(
                    response.chain().certificates(i).data().data())),
            response.chain().certificates(i).data().size(), false);
        chain.push_back(blob);
    }

    _hidl_cb(translate_error_code(response.error_code()),
             hidl_vec<hidl_vec<uint8_t> >(chain));
    return Void();
}

Return<void> KeymasterDevice::upgradeKey(
        const hidl_vec<uint8_t>& keyBlobToUpgrade,
        const hidl_vec<KeyParameter>& upgradeParams,
        upgradeKey_cb _hidl_cb)
{
    LOG(VERBOSE) << "Running KeymasterDevice::upgradeKey";

    UpgradeKeyRequest request;
    UpgradeKeyResponse response;

    request.mutable_blob()->set_blob(&keyBlobToUpgrade[0],
                                     keyBlobToUpgrade.size());

    hidl_vec<uint8_t> blob;
    if (hidl_params_to_pb(
            upgradeParams, request.mutable_params()) != ErrorCode::OK) {
      _hidl_cb(ErrorCode::INVALID_ARGUMENT, blob);
      return Void();
    }

    KM_CALLV(UpgradeKey, hidl_vec<uint8_t>{});

    blob.setToExternal(
        reinterpret_cast<uint8_t*>(
            const_cast<char*>(response.blob().blob().data())),
        response.blob().blob().size(), false);

    _hidl_cb(translate_error_code(response.error_code()), blob);
    return Void();
}

Return<ErrorCode> KeymasterDevice::deleteKey(const hidl_vec<uint8_t>& keyBlob)
{
    LOG(VERBOSE) << "Running KeymasterDevice::deleteKey";

    DeleteKeyRequest request;
    DeleteKeyResponse response;

    request.mutable_blob()->set_blob(&keyBlob[0], keyBlob.size());

    KM_CALL(DeleteKey);

    return translate_error_code(response.error_code());
}

Return<ErrorCode> KeymasterDevice::deleteAllKeys()
{
    LOG(VERBOSE) << "Running KeymasterDevice::deleteAllKeys";

    DeleteAllKeysRequest request;
    DeleteAllKeysResponse response;

    KM_CALL(DeleteAllKeys);

    return translate_error_code(response.error_code());
}

Return<ErrorCode> KeymasterDevice::destroyAttestationIds()
{
    LOG(VERBOSE) << "Running KeymasterDevice::destroyAttestationIds";

    DestroyAttestationIdsRequest request;
    DestroyAttestationIdsResponse response;

    KM_CALL(DestroyAttestationIds);

    return translate_error_code(response.error_code());
}

Return<void> KeymasterDevice::begin(
        KeyPurpose purpose, const hidl_vec<uint8_t>& key,
        const hidl_vec<KeyParameter>& inParams,
        const HardwareAuthToken& authToken,
        begin_cb _hidl_cb)
{
    LOG(VERBOSE) << "Running KeymasterDevice::begin";

    BeginOperationRequest request;
    BeginOperationResponse response;

    request.set_purpose((::nugget::app::keymaster::KeyPurpose)purpose);
    request.mutable_blob()->set_blob(&key[0], key.size());

    hidl_vec<KeyParameter> params;
    if (translate_auth_token(
            authToken, request.mutable_auth_token()) != ErrorCode::OK) {
        _hidl_cb(ErrorCode::INVALID_ARGUMENT, params,
                 response.handle().handle());
        return Void();
    }
    if (hidl_params_to_pb(
            inParams, request.mutable_params()) != ErrorCode::OK) {
      _hidl_cb(ErrorCode::INVALID_ARGUMENT, params,
               response.handle().handle());
      return Void();
    }

    KM_CALLV(BeginOperation, hidl_vec<KeyParameter>{}, 0);

    pb_to_hidl_params(response.params(), &params);

    _hidl_cb(translate_error_code(response.error_code()), params,
             response.handle().handle());
    return Void();
}

Return<void> KeymasterDevice::update(
        uint64_t operationHandle,
        const hidl_vec<KeyParameter>& inParams,
        const hidl_vec<uint8_t>& input,
        const HardwareAuthToken& authToken,
        const VerificationToken& verificationToken,
        update_cb _hidl_cb)
{
    LOG(VERBOSE) << "Running KeymasterDevice::update";

    UpdateOperationRequest request;
    UpdateOperationResponse response;

    if (input.size() > KM_MAX_PROTO_FIELD_SIZE) {
        LOG(ERROR) << "Excess input length: " << input.size()
                   << "max allowed: " << KM_MAX_PROTO_FIELD_SIZE;
        if (this->abort(operationHandle) != ErrorCode::OK) {
            LOG(ERROR) << "abort( " << operationHandle
                       << ") failed";
        }
        _hidl_cb(ErrorCode::INVALID_INPUT_LENGTH, 0,
                 hidl_vec<KeyParameter>{}, hidl_vec<uint8_t>{});
        return Void();
    }

    request.mutable_handle()->set_handle(operationHandle);

    hidl_vec<KeyParameter> params;
    hidl_vec<uint8_t> output;
    if (hidl_params_to_pb(
            inParams, request.mutable_params()) != ErrorCode::OK) {
      _hidl_cb(ErrorCode::INVALID_ARGUMENT, 0, params, output);
      return Void();
    }

    request.set_input(&input[0], input.size());
    if (translate_auth_token(
            authToken, request.mutable_auth_token()) != ErrorCode::OK) {
        _hidl_cb(ErrorCode::INVALID_ARGUMENT, 0, params, output);
        return Void();
    }
    translate_verification_token(verificationToken,
                                 request.mutable_verification_token());

    KM_CALLV(UpdateOperation, 0, hidl_vec<KeyParameter>{}, hidl_vec<uint8_t>{});

    pb_to_hidl_params(response.params(), &params);
    output.setToExternal(
        reinterpret_cast<uint8_t*>(const_cast<char*>(response.output().data())),
        response.output().size(), false);

    _hidl_cb(ErrorCode::OK, response.consumed(), params, output);
    return Void();
}

Return<void> KeymasterDevice::finish(
        uint64_t operationHandle,
        const hidl_vec<KeyParameter>& inParams,
        const hidl_vec<uint8_t>& input,
        const hidl_vec<uint8_t>& signature,
        const HardwareAuthToken& authToken,
        const VerificationToken& verificationToken,
        finish_cb _hidl_cb)
{
    LOG(VERBOSE) << "Running KeymasterDevice::finish";

    FinishOperationRequest request;
    FinishOperationResponse response;

    if (input.size() > KM_MAX_PROTO_FIELD_SIZE) {
        LOG(ERROR) << "Excess input length: " << input.size()
                   << "max allowed: " << KM_MAX_PROTO_FIELD_SIZE;
        if (this->abort(operationHandle) != ErrorCode::OK) {
            LOG(ERROR) << "abort( " << operationHandle
                       << ") failed";
        }
        _hidl_cb(ErrorCode::INVALID_INPUT_LENGTH,
                 hidl_vec<KeyParameter>{}, hidl_vec<uint8_t>{});
        return Void();
    }

    request.mutable_handle()->set_handle(operationHandle);

    hidl_vec<KeyParameter> params;
    hidl_vec<uint8_t> output;
    if (hidl_params_to_pb(
            inParams, request.mutable_params()) != ErrorCode::OK) {
      _hidl_cb(ErrorCode::INVALID_ARGUMENT, params, output);
      return Void();
    }

    request.set_input(&input[0], input.size());
    request.set_signature(&signature[0], signature.size());

    if (translate_auth_token(
            authToken, request.mutable_auth_token()) != ErrorCode::OK) {
        _hidl_cb(ErrorCode::INVALID_ARGUMENT, params, output);
        return Void();
    }
    translate_verification_token(verificationToken,
                                 request.mutable_verification_token());

    KM_CALLV(FinishOperation, hidl_vec<KeyParameter>{}, hidl_vec<uint8_t>{});

    pb_to_hidl_params(response.params(), &params);
    output.setToExternal(
        reinterpret_cast<uint8_t*>(const_cast<char*>(response.output().data())),
        response.output().size(), false);

    _hidl_cb(ErrorCode::OK, params, output);
    return Void();
}

Return<ErrorCode> KeymasterDevice::abort(uint64_t operationHandle)
{
    LOG(VERBOSE) << "Running KeymasterDevice::abort";

    AbortOperationRequest request;
    AbortOperationResponse response;

    request.mutable_handle()->set_handle(operationHandle);

    KM_CALL(AbortOperation);

    return ErrorCode::OK;
}

// Methods from ::android::hardware::keymaster::V4_0::IKeymasterDevice follow.
Return<void> KeymasterDevice::importWrappedKey(
    const hidl_vec<uint8_t>& wrappedKeyData,
    const hidl_vec<uint8_t>& wrappingKeyBlob,
    const hidl_vec<uint8_t>& maskingKey,
    const hidl_vec<KeyParameter>& /* unwrappingParams */,
    uint64_t /* passwordSid */, uint64_t /* biometricSid */,
    importWrappedKey_cb _hidl_cb)
{
    LOG(VERBOSE) << "Running KeymasterDevice::importWrappedKey";

    ErrorCode error;
    ImportWrappedKeyRequest request;
    ImportKeyResponse response;

    if (maskingKey.size() != KM_WRAPPER_MASKING_KEY_SIZE) {
        _hidl_cb(ErrorCode::INVALID_ARGUMENT, hidl_vec<uint8_t>{},
                 KeyCharacteristics{});
        return Void();
    }

    error = import_wrapped_key_request(wrappedKeyData, wrappingKeyBlob,
                                       maskingKey, &request);
    if (error != ErrorCode::OK) {
        LOG(ERROR) << "ImportWrappedKey request parsing failed with error "
                   << error;
        _hidl_cb(error, hidl_vec<uint8_t>{}, KeyCharacteristics{});
        return Void();
    }

    KM_CALLV(ImportWrappedKey, hidl_vec<uint8_t>{}, KeyCharacteristics{});

    hidl_vec<uint8_t> blob;
    blob.setToExternal(
        reinterpret_cast<uint8_t*>(
            const_cast<char*>(response.blob().blob().data())),
        response.blob().blob().size(), false);

    KeyCharacteristics characteristics;
    // TODO: anything to do here with softwareEnforced?
    pb_to_hidl_params(response.characteristics().software_enforced(),
                      &characteristics.softwareEnforced);
    error = pb_to_hidl_params(response.characteristics().tee_enforced(),
                              &characteristics.hardwareEnforced);
    if (error != ErrorCode::OK) {
        LOG(ERROR) <<
            "KeymasterDevice::importWrappedKey: response tee_enforced :"
                   << error;
        _hidl_cb(error, hidl_vec<uint8_t>{}, KeyCharacteristics{});
        return Void();
    }

    _hidl_cb(ErrorCode::OK, blob, characteristics);
    return Void();
}

}  // namespace keymaster
}  // namespace hardware
}  // namespace android

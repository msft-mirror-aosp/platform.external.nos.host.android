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

namespace android {
namespace hardware {
namespace keymaster {

// HAL
using ::android::hardware::keymaster::V3_0::Tag;
using ::android::hardware::keymaster::V3_0::TagType;

static TagType type_from_tag(Tag tag)
{
    return static_cast<TagType>(tag & (0xF << 28));
}

static ErrorCode key_parameter_to_pb(const KeyParameter& param,
                                     nosapp::KeyParameter *pb)
{
    pb->set_tag((uint32_t)param.tag);
    switch (type_from_tag(param.tag)) {
    case TagType::INVALID:
        LOG(ERROR) << "key_parameter_to_pb: invalid TagType received: "
                   << (uint32_t)type_from_tag(param.tag);
        return ErrorCode::INVALID_ARGUMENT;
        break;
    case TagType::ENUM:
    case TagType::ENUM_REP:
    case TagType::UINT:
    case TagType::UINT_REP:
    case TagType::BOOL:
        /* 32-bit values. */
        pb->set_integer(param.f.integer);
        break;
    case TagType::ULONG:
    case TagType::DATE:
    case TagType::ULONG_REP:
        /* 64-bit values. */
        pb->set_long_integer(param.f.longInteger);
        break;
    case TagType::BIGNUM:
    case TagType::BYTES:
        /* Byte arrays. */
        pb->set_blob(&param.blob[0], param.blob.size());
        break;
    default:
        LOG(ERROR) << "map_params_to_pb: unknown TagType received: "
                   << (uint32_t)type_from_tag(param.tag);
        return ErrorCode::INVALID_ARGUMENT;
        break;
    }

    return ErrorCode::OK;
}

void hidl_params_to_pb(const hidl_vec<KeyParameter>& params,
                       nosapp::KeyParameters *pb)
{
    for (size_t i = 0; i < params.size(); i++) {
        nosapp::KeyParameter *param = pb->add_params();
        key_parameter_to_pb(params[i], param);
    }
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

        kp.tag = static_cast<Tag>(pbParams.params(i).tag());
        switch(type_from_tag(kp.tag)) {
        case TagType::INVALID:
            LOG(ERROR) << "pb_to_hidl_params: invalid TagType received: "
                       << (uint32_t)type_from_tag(kp.tag);
            // TODO: this is invalid data from citadel.
            return ErrorCode::INVALID_ARGUMENT;
            break;
        case TagType::ENUM:
        case TagType::ENUM_REP:
        case TagType::UINT:
        case TagType::UINT_REP:
            /* 32-bit values. */
            kp.f.integer = param.integer();
            break;
        case TagType::BOOL:
            /* Special handling for boolean. */
            kp.f.boolValue = (param.integer() != 0);
            break;
        case TagType::ULONG:
        case TagType::DATE:
        case TagType::ULONG_REP:
            /* 64-bit values. */
            kp.f.longInteger = param.long_integer();
            break;
        case TagType::BIGNUM:
        case TagType::BYTES:
            /* Byte arrays. */
            kp.blob.setToExternal(
                reinterpret_cast<uint8_t*>(const_cast<char*>(
                                               param.blob().data())),
                param.blob().size());
            break;
        default:
            // TODO: this is invalid data from citadel.
            LOG(ERROR) << "pb_to_hidl_params: unknown TagType received: "
                       << (uint32_t)type_from_tag(kp.tag);
            break;
        }

        kpv.push_back(kp);
    }

    *params = kpv;

    return ErrorCode::OK;
}

}  // namespace keymaster
}  // hardware
}  // android

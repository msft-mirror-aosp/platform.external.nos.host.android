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

#ifndef NOS_ANDROID_CITADEL_H
#define NOS_ANDROID_CITADEL_H

#include <nos/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Open a connection to a Nugget device.
 *
 * Returns 0 on success or negative on failure.
 */
int nos_android_citadel_device_open(const char* device_name, struct nos_device *dev);

/*
 * Close the connection to a Nugget device.
 */
void nos_android_citadel_device_close(struct nos_device *dev);

#ifdef __cplusplus
}
#endif

#endif /* NOS_ANDROID_CITADEL_H */

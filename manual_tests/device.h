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
#ifndef MANUAL_TESTS_DEVICE_H
#define MANUAL_TESTS_DEVICE_H

#include <nos/device.h>

/* Use /dev/citadel0 if devicename is NULL */
int OpenDev(const char *devicename, struct nos_device *dev);

/* Guess */
void CloseDev(struct nos_device *);

#endif	/* MANUAL_TESTS_DEVICE_H */

#ifndef ANDROID_HARDWARE_KEYMASTER_HAL_SUPPORT_H
#define ANDROID_HARDWARE_KEYMASTER_HAL_SUPPORT_H

#include <android/hardware/keymaster/3.0/IKeymasterDevice.h>

using ::android::hardware::keymaster::V3_0::KeyParameter;

bool operator==(const KeyParameter& a, const KeyParameter& b);

#endif  // ANDROID_HARDWARE_KEYMASTER_KEYMASTER_DEVICE_H

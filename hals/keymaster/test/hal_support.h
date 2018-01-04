#ifndef ANDROID_HARDWARE_KEYMASTER_HAL_SUPPORT_H
#define ANDROID_HARDWARE_KEYMASTER_HAL_SUPPORT_H

#include <android/hardware/keymaster/4.0/IKeymasterDevice.h>

using ::android::hardware::keymaster::V4_0::KeyParameter;

bool operator==(const KeyParameter& a, const KeyParameter& b);

#endif  // ANDROID_HARDWARE_KEYMASTER_KEYMASTER_DEVICE_H

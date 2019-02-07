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

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <limits>
#include <mutex>
#include <thread>

#include <android-base/logging.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>

#include <app_nugget.h>
#include <nos/NuggetClient.h>
#include <nos/device.h>

#include <android/hardware/citadel/BnCitadeld.h>

using ::android::OK;
using ::android::defaultServiceManager;
using ::android::sp;
using ::android::status_t;
using ::android::IPCThreadState;
using ::android::IServiceManager;
using ::android::ProcessState;

using ::android::binder::Status;

using ::nos::NuggetClient;

using ::android::hardware::citadel::BnCitadeld;
using ::android::hardware::citadel::ICitadeld;

namespace {

using namespace std::chrono_literals;

// This attaches a timer to a function call. Call .schedule() to start the
// timer, and the function will be called (once) after the time has elapsed. If
// you call .schedule() again before that happens, it just restarts the timer.
// There's no way to cancel the function call after it's scheduled; you can only
// postpone it.
class DeferredCallback {
  public:
    DeferredCallback(std::chrono::milliseconds delay, std::function<void()> fn)
        : _armed(false),
          _delay(delay),
          _func(fn),
          _waiter_thread(std::bind(&DeferredCallback::waiter_task, this)) {}
    ~DeferredCallback() {}

    // [re]start the timer for the delayed call
    void schedule() {
        std::unique_lock<std::mutex> _lock(_cv_mutex);
        _armed = true;
        _cv.notify_one();
    }

  private:
    void waiter_task(void) {
        std::unique_lock<std::mutex> _lock(_cv_mutex);
        while (true) {
            if (!_armed) {
                _cv.wait(_lock);
            }
            auto timeout = std::chrono::steady_clock::now() + _delay;
            if (_cv.wait_until(_lock, timeout) == std::cv_status::timeout) {
                _func();
                _armed = false;
            }
        }
    }

    bool _armed;
    const std::chrono::milliseconds _delay;
    const std::function<void()> _func;
    std::thread _waiter_thread;
    std::mutex _cv_mutex;
    std::condition_variable _cv;
};

class CitadelProxy : public BnCitadeld {
  public:
    CitadelProxy(NuggetClient& client)
        : _client{client},
          _stats_collection(500ms, std::bind(&CitadelProxy::cacheStats, this)) {
    }
    ~CitadelProxy() override = default;

    Status callApp(const int32_t _appId, const int32_t _arg,
                   const std::vector<uint8_t>& request,
                   std::vector<uint8_t>* const response,
                   int32_t* const _aidl_return) override {
        // AIDL doesn't support integers less than 32-bit so validate it before
        // casting
        if (_appId < 0 || _appId > kMaxAppId) {
            LOG(ERROR) << "App ID " << _appId << " is outside the app ID range";
            return Status::fromExceptionCode(Status::EX_ILLEGAL_ARGUMENT);
        }
        if (_arg < 0 || _arg > std::numeric_limits<uint16_t>::max()) {
            LOG(ERROR) << "Argument " << _arg
                       << " is outside the unsigned 16-bit range";
            return Status::fromExceptionCode(Status::EX_ILLEGAL_ARGUMENT);
        }

        const uint8_t appId = static_cast<uint32_t>(_appId);
        const uint16_t arg = static_cast<uint16_t>(_arg);
        uint32_t* const appStatus = reinterpret_cast<uint32_t*>(_aidl_return);

        // Make the call to the app while holding the lock for that app
        std::unique_lock<std::mutex> lock(_appLocks[appId]);
        *appStatus = _client.CallApp(appId, arg, request, response);

        _stats_collection.schedule();

        return Status::ok();
    }

    Status reset(bool* const _aidl_return) override {
        // This doesn't use the transport API to talk to any app so doesn't need
        // to hold any app locks.
        const nos_device& device = *_client.Device();
        *_aidl_return = (device.ops.reset(device.ctx) == 0);
        return Status::ok();
    }

    Status getCachedStats(std::vector<uint8_t>* const response) override {
        std::unique_lock<std::mutex> lock(_stats_mutex);

        response->resize(sizeof(_stats));
        memcpy(response->data(), &_stats, sizeof(_stats));

        return Status::ok();
    }

private:
    static constexpr auto kMaxAppId = std::numeric_limits<uint8_t>::max();

    NuggetClient& _client;
    std::mutex _appLocks[kMaxAppId + 1];
    struct nugget_app_low_power_stats _stats;
    DeferredCallback _stats_collection;
    std::mutex _stats_mutex;

    void cacheStats(void) {
        std::vector<uint8_t> buffer;
        buffer.reserve(sizeof(_stats));
        uint32_t rv;

        {
            std::unique_lock<std::mutex> lock(_appLocks[APP_ID_NUGGET]);
            rv = _client.CallApp(APP_ID_NUGGET,
                                 NUGGET_PARAM_GET_LOW_POWER_STATS, buffer,
                                 &buffer);
        }

        if (rv == APP_SUCCESS) {
            std::unique_lock<std::mutex> lock(_stats_mutex);
            memcpy(&_stats, buffer.data(),
                   std::min(sizeof(_stats), buffer.size()));
        }
    }
};

[[noreturn]] void CitadelEventDispatcher(const nos_device& device) {
    LOG(INFO) << "Event dispatcher startup.";
    while(1) {
        if (device.ops.wait_for_interrupt(device.ctx, -1) > 0) {
            LOG(INFO) << "Citadel has dispatched an event";
        } else {
            LOG(INFO) << "Citadel did something unexpected";
        }

        // This is a placeholder for the message handling that gives citadel a
        // chance to deassert CTLD_AP_IRQ, so this doesn't spam the logs.
        // TODO(b/62713383) Replace this with the code to contact citadel.
        sleep(1);
    }
}

} // namespace

int main() {
    LOG(INFO) << "Starting citadeld";

    // Connect to Citadel
    NuggetClient citadel;
    citadel.Open();
    if (!citadel.IsOpen()) {
        LOG(FATAL) << "Failed to open Citadel client";
    }

    // Citadel HALs will communicate with this daemon via /dev/vndbinder as this
    // is vendor code
    ProcessState::initWithDriver("/dev/vndbinder");

    sp<CitadelProxy> proxy = new CitadelProxy(citadel);
    const status_t status = defaultServiceManager()->addService(ICitadeld::descriptor, proxy);
    if (status != OK) {
        LOG(FATAL) << "Failed to register citadeld as a service (status " << status << ")";
    }

    // Handle interrupts triggered by Citadel and dispatch any events to
    // registered listeners.
    std::thread event_dispatcher(CitadelEventDispatcher, *citadel.Device());

    // Start handling binder requests with multiple threads
    ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();
    return 0;
}

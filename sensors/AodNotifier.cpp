/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "AodNotifier"

#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <display/drm/mi_disp.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <bitset>
#include <cerrno>
#include <cstring>

#include "AodNotifier.h"
#include "SensorNotifierUtils.h"
#include "Config.h"

using android::hardware::Return;
using android::hardware::Void;
using android::hardware::sensors::V1_0::Event;

namespace {

void requestDozeBrightness(int fd, __u32 doze_brightness) {
    if (fd < 0) {
        LOG(ERROR) << "Invalid file descriptor for doze brightness request";
        return;
    }
    
    disp_doze_brightness_req req;
    req.base.flag = 0;
    req.base.disp_id = MI_DISP_PRIMARY;
    req.doze_brightness = doze_brightness;
    
    if (ioctl(fd, MI_DISP_IOCTL_SET_DOZE_BRIGHTNESS, &req) == -1) {
        LOG(ERROR) << "Failed to set doze brightness, errno: " << errno << " - " << strerror(errno);
    } else {
        LOG(VERBOSE) << "Doze brightness set to: " << doze_brightness;
    }
}

class AodSensorCallback : public IEventQueueCallback {
  public:
    AodSensorCallback() {
        auto& config = Config::getInstance().getSettings();
        disp_fd_ = android::base::unique_fd(open(config.displayDevicePath.c_str(), O_RDWR));
        if (disp_fd_.get() == -1) {
            LOG(ERROR) << "Failed to open " << config.displayDevicePath << ", errno: " << errno << " - " << strerror(errno);
        } else {
            LOG(INFO) << "Successfully opened display feature device: " << config.displayDevicePath;
        }
    }

    Return<void> onEvent(const Event& e) {
        if (disp_fd_.get() == -1) {
            LOG(ERROR) << "Display FD not available for event processing";
            return Void();
        }
        
        LOG(VERBOSE) << "Received AOD sensor event with scalar: " << e.u.scalar;
        requestDozeBrightness(disp_fd_.get(), (e.u.scalar == 3 || e.u.scalar == 5)
                                                      ? DOZE_BRIGHTNESS_LBM
                                                      : DOZE_BRIGHTNESS_HBM);
        return Void();
    }

  private:
    android::base::unique_fd disp_fd_;
};

}  // namespace

AodNotifier::AodNotifier(sp<ISensorManager> manager) : SensorNotifier(manager) {
    initializeSensorQueue("xiaomi.sensor.aod", true, new AodSensorCallback());
}

AodNotifier::~AodNotifier() {
    deactivate();
}

void AodNotifier::pollingFunction() {
    LOG(INFO) << "AodNotifier polling thread started";
    
    auto& config = Config::getInstance().getSettings();
    Result res;
    
    android::base::unique_fd disp_fd_ = android::base::unique_fd(open(config.displayDevicePath.c_str(), O_RDWR));
    
    if (disp_fd_.get() == -1) {
        LOG(ERROR) << "Failed to open " << config.displayDevicePath << ", errno: " << errno << " - " << strerror(errno);
        return;
    }

    disp_event_req req;
    req.base.flag = 0;
    req.base.disp_id = MI_DISP_PRIMARY;
    req.type = MI_DISP_EVENT_POWER;
    
    if (ioctl(disp_fd_.get(), MI_DISP_IOCTL_REGISTER_EVENT, &req) == -1) {
        LOG(ERROR) << "Failed to register display event, errno: " << errno << " - " << strerror(errno);
        return;
    }

    struct pollfd dispEventPoll = {
            .fd = disp_fd_.get(),
            .events = POLLIN | POLLERR,
            .revents = 0,
    };

    bool sensorEnabled = false;
    int consecutiveErrors = 0;

    while (mActive.load()) {
        int rc = poll(&dispEventPoll, 1, config.pollTimeoutMs);
        
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            LOG(ERROR) << "Failed to poll display event, errno: " << errno << " - " << strerror(errno);
            if (++consecutiveErrors >= config.maxConsecutiveErrors) {
                LOG(ERROR) << "Too many consecutive errors, stopping AodNotifier";
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(config.errorRetryDelayMs));
            continue;
        }
        
        consecutiveErrors = 0;

        if (rc == 0) {
            continue;
        }

        if (dispEventPoll.revents & POLLERR) {
            LOG(ERROR) << "Poll error on display fd";
            continue;
        }

        if (!(dispEventPoll.revents & POLLIN)) {
            continue;
        }

        std::shared_ptr<disp_event_resp> response = parseDispEvent(disp_fd_.get());
        if (response == nullptr) {
            LOG(ERROR) << "Failed to parse display event";
            continue;
        }

        if (response->base.type != MI_DISP_EVENT_POWER) {
            LOG(VERBOSE) << "Unexpected display event type: " << response->base.type;
            continue;
        }

        int value = response->data[0];
        LOG(VERBOSE) << "Received display power event: " << std::bitset<8>(value);

        switch (value) {
            case MI_DISP_POWER_LP1:
            case MI_DISP_POWER_LP2:
                if (!sensorEnabled) {
                    res = mQueue->enableSensor(mSensorHandle, 
                                             config.sensorSamplePeriod,
                                             config.sensorLatency);
                    if (res != Result::OK) {
                        LOG(ERROR) << "Failed to enable AOD sensor, result: " << static_cast<int>(res);
                    } else {
                        sensorEnabled = true;
                        LOG(INFO) << "AOD sensor enabled for LP mode (period: " 
                                  << config.sensorSamplePeriod << "us)";
                    }
                }
                break;
            case MI_DISP_POWER_ON:
                if (sensorEnabled) {
                    res = mQueue->disableSensor(mSensorHandle);
                    if (res != Result::OK) {
                        LOG(ERROR) << "Failed to disable AOD sensor, result: " << static_cast<int>(res);
                    } else {
                        sensorEnabled = false;
                        LOG(INFO) << "AOD sensor disabled for ON mode";
                    }
                }
                requestDozeBrightness(disp_fd_.get(), DOZE_TO_NORMAL);
                break;
            default:
                if (sensorEnabled) {
                    res = mQueue->disableSensor(mSensorHandle);
                    if (res != Result::OK) {
                        LOG(ERROR) << "Failed to disable AOD sensor in default case, result: " << static_cast<int>(res);
                    } else {
                        sensorEnabled = false;
                        LOG(INFO) << "AOD sensor disabled by default case";
                    }
                }
                break;
        }
    }
    
    if (sensorEnabled && mQueue != nullptr) {
        res = mQueue->disableSensor(mSensorHandle);
        if (res != Result::OK) {
            LOG(ERROR) << "Failed to disable sensor during cleanup";
        }
    }
    
    LOG(INFO) << "AodNotifier polling thread stopped";
}
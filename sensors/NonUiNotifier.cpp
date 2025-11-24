/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "NonUiNotifier"

#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <linux/xiaomi_touch.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

#include "NonUiNotifier.h"
#include "SensorNotifierUtils.h"
#include "Config.h"

using android::hardware::Return;
using android::hardware::Void;
using android::hardware::sensors::V1_0::Event;

namespace {

class NonUiSensorCallback : public IEventQueueCallback {
  public:
    NonUiSensorCallback() {
        auto& config = Config::getInstance().getSettings();
        touch_fd_ = android::base::unique_fd(open(config.touchDevicePath.c_str(), O_RDWR));
        if (touch_fd_.get() == -1) {
            LOG(ERROR) << "Failed to open " << config.touchDevicePath << ", errno: " << errno << " - " << strerror(errno);
        } else {
            LOG(INFO) << "Successfully opened touch device: " << config.touchDevicePath;
        }
    }

    Return<void> onEvent(const Event& e) {
        if (touch_fd_.get() == -1) {
            LOG(ERROR) << "Touch FD not available for event processing";
            return Void();
        }
        
        struct touch_mode_request request = {
                .mode = TOUCH_MODE_NONUI_MODE,
                .value = static_cast<int>(e.u.scalar),
        };
        
        LOG(VERBOSE) << "Setting non-UI touch mode to: " << request.value;
        
        if (ioctl(touch_fd_.get(), TOUCH_IOC_SET_CUR_VALUE, &request) == -1) {
            LOG(ERROR) << "Failed to set touch mode, errno: " << errno << " - " << strerror(errno);
        } else {
            LOG(VERBOSE) << "Successfully set touch mode to: " << request.value;
        }

        return Void();
    }

  private:
    android::base::unique_fd touch_fd_;
};

}  // namespace

NonUiNotifier::NonUiNotifier(sp<ISensorManager> manager) : SensorNotifier(manager) {
    initializeSensorQueue("xiaomi.sensor.nonui", true, new NonUiSensorCallback());
}

NonUiNotifier::~NonUiNotifier() {
    deactivate();
}

void NonUiNotifier::pollingFunction() {
    LOG(INFO) << "NonUiNotifier polling thread started";
    
    auto& config = Config::getInstance().getSettings();
    Result res;
    bool sensorEnabled = false;

    const std::vector<const char*> paths = {
            "/sys/class/touch/touch_dev/fod_longpress_gesture_enabled",
            "/sys/class/touch/touch_dev/gesture_single_tap_enabled",
            "/sys/class/touch/touch_dev/gesture_double_tap_enabled"};

    std::vector<pollfd> pollfds;
    std::vector<android::base::unique_fd> fds;

    for (const auto& path : paths) {
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            LOG(WARNING) << "Failed to open " << path << ", errno: " << errno << " - " << strerror(errno);
            continue;
        }
        
        char buffer;
        ssize_t test_read = read(fd, &buffer, 1);
        if (test_read < 0) {
            LOG(WARNING) << "File " << path << " not readable, skipping. errno: " << errno;
            close(fd);
            continue;
        }
        lseek(fd, 0, SEEK_SET);
        
        pollfd pfd = {
            .fd = fd,
            .events = POLLPRI | POLLERR,
            .revents = 0
        };
        pollfds.push_back(pfd);
        fds.emplace_back(fd);
        
        LOG(INFO) << "Monitoring touch gesture: " << path << " (fd: " << fd << ")";
    }

    if (pollfds.empty()) {
        LOG(ERROR) << "No valid file descriptors for polling after initialization";
        return;
    }

    int consecutiveErrors = 0;
    int consecutivePollErrors = 0;
    const int MAX_CONSECUTIVE_POLL_ERRORS = 3;

    while (mActive.load()) {
        bool hasInvalidFds = false;
        for (size_t i = 0; i < pollfds.size(); ++i) {
            if (fcntl(pollfds[i].fd, F_GETFD) == -1) {
                LOG(ERROR) << "File descriptor " << pollfds[i].fd << " became invalid, removing from polling";
                pollfds[i].fd = -1;
                hasInvalidFds = true;
            }
        }

        if (hasInvalidFds) {
            auto new_end = std::remove_if(pollfds.begin(), pollfds.end(),
                [](const pollfd& pfd) { return pfd.fd == -1; });
            pollfds.erase(new_end, pollfds.end());
            
            if (pollfds.empty()) {
                LOG(ERROR) << "All file descriptors became invalid, stopping NonUiNotifier";
                break;
            }
        }

        int rc = poll(pollfds.data(), pollfds.size(), config.pollTimeoutMs);
        
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            
            LOG(ERROR) << "Failed to poll touch gestures, errno: " << errno << " - " << strerror(errno);
            consecutivePollErrors++;
            
            if (consecutivePollErrors >= MAX_CONSECUTIVE_POLL_ERRORS) {
                LOG(ERROR) << "Too many consecutive poll errors, stopping NonUiNotifier";
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(config.errorRetryDelayMs));
            continue;
        }
        
        consecutivePollErrors = 0;
        consecutiveErrors = 0;

        if (rc == 0) {
            continue;
        }

        bool enabled = false;
        bool hasPollError = false;
        std::vector<size_t> invalidIndices;
        
        for (size_t i = 0; i < pollfds.size(); ++i) {
            if (pollfds[i].revents & POLLERR) {
                LOG(ERROR) << "Poll error on fd: " << pollfds[i].fd << ", removing from polling";
                invalidIndices.push_back(i);
                hasPollError = true;
                continue;
            }
            
            if (pollfds[i].revents & POLLPRI) {
                bool currentState = readBool(pollfds[i].fd);
                if (errno == EBADF) {
                    LOG(ERROR) << "File descriptor " << pollfds[i].fd << " became bad, removing from polling";
                    invalidIndices.push_back(i);
                    hasPollError = true;
                    continue;
                }
                
                LOG(VERBOSE) << "Gesture state changed on fd " << pollfds[i].fd << ": " << currentState;
                enabled = enabled || currentState;
            }
        }

        if (!invalidIndices.empty()) {
            std::sort(invalidIndices.rbegin(), invalidIndices.rend());
            for (size_t idx : invalidIndices) {
                if (idx < pollfds.size()) {
                    LOG(WARNING) << "Removing invalid fd from polling: " << pollfds[idx].fd;
                    pollfds.erase(pollfds.begin() + idx);
                }
            }
            
            if (pollfds.empty()) {
                LOG(ERROR) << "All file descriptors became invalid after error handling";
                break;
            }
        }

        if (hasPollError) {
            consecutiveErrors++;
            if (consecutiveErrors >= config.maxConsecutiveErrors) {
                LOG(ERROR) << "Too many consecutive errors, stopping NonUiNotifier";
                break;
            }
            continue;
        }

        consecutiveErrors = 0;

        if (enabled && !sensorEnabled) {
            res = mQueue->enableSensor(mSensorHandle, 
                                     config.sensorSamplePeriod, 
                                     config.sensorLatency);
            if (res != Result::OK) {
                LOG(ERROR) << "Failed to enable non-UI sensor, result: " << static_cast<int>(res);
                consecutiveErrors++;
            } else {
                sensorEnabled = true;
                consecutiveErrors = 0;
                LOG(INFO) << "Non-UI sensor enabled (period: " 
                          << config.sensorSamplePeriod << "us)";
            }
        } else if (!enabled && sensorEnabled) {
            res = mQueue->disableSensor(mSensorHandle);
            if (res != Result::OK) {
                LOG(ERROR) << "Failed to disable non-UI sensor, result: " << static_cast<int>(res);
                consecutiveErrors++;
            } else {
                sensorEnabled = false;
                consecutiveErrors = 0;
                LOG(INFO) << "Non-UI sensor disabled";
            }
        }
        
        if (consecutiveErrors >= config.maxConsecutiveErrors) {
            LOG(ERROR) << "Too many consecutive operation errors, stopping NonUiNotifier";
            break;
        }
    }
    
    if (sensorEnabled && mQueue != nullptr) {
        res = mQueue->disableSensor(mSensorHandle);
        if (res != Result::OK) {
            LOG(ERROR) << "Failed to disable sensor during cleanup";
        }
    }
    
    LOG(INFO) << "NonUiNotifier polling thread stopped";
    if (!pollfds.empty()) {
        LOG(INFO) << "Remaining active file descriptors: " << pollfds.size();
    }
}
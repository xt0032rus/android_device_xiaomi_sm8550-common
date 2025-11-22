/*
 * Copyright (C) 2022-2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "UdfpsHandler.xiaomi_sm8550"

#include <aidl/android/hardware/biometrics/fingerprint/BnFingerprint.h>
#include <android-base/logging.h>
#include <android-base/unique_fd.h>

#include <fstream>

#include "UdfpsHandler.h"

#define COMMAND_NIT 10
#define TARGET_BRIGHTNESS_OFF 0
#define TARGET_BRIGHTNESS_1000NIT 1
#define TARGET_BRIGHTNESS_110NIT 6

#define COMMAND_FOD_PRESS_STATUS 1
#define COMMAND_FOD_PRESS_X 2
#define COMMAND_FOD_PRESS_Y 3
#define PARAM_FOD_PRESSED 1
#define PARAM_FOD_RELEASED 0

#define FINGERPRINT_ACQUIRED_VENDOR 7

// Sysfs paths
#define DISP_PARAM_PATH "/sys/devices/virtual/mi_display/disp_feature/disp-DSI-0/disp_param"
#define FOD_PRESS_STATUS_PATH "/sys/class/touch/touch_dev/fod_press_status"
#define FOD_FINGER_STATE_PATH "/sys/class/touch/touch_dev/fod_finger_status"

#define DISP_PARAM_LOCAL_HBM_MODE "9"
#define DISP_PARAM_LOCAL_HBM_OFF "0"
#define DISP_PARAM_LOCAL_HBM_ON "1"

#define FOD_STATUS_ON 1
#define FOD_STATUS_OFF 0

using ::aidl::android::hardware::biometrics::fingerprint::AcquiredInfo;

namespace {

template <typename T>
static void set(const std::string& path, const T& value) {
    std::ofstream file(path);
    if (!file) {
        LOG(ERROR) << "Failed to open " << path;
        return;
    }
    file << value;
    if (file.fail()) {
        LOG(ERROR) << "Failed to write to " << path;
    }
}

static bool fileExists(const std::string& path) {
    return access(path.c_str(), F_OK) != -1;
}

}  // anonymous namespace

class XiaomiSm8550UdfpsHandler : public UdfpsHandler {
  public:
    void init(fingerprint_device_t* device) {
        mDevice = device;
        
        // Проверить существование необходимых sysfs нод
        if (!fileExists(DISP_PARAM_PATH)) {
            LOG(ERROR) << DISP_PARAM_PATH << " does not exist";
        }
        if (!fileExists(FOD_PRESS_STATUS_PATH)) {
            LOG(ERROR) << FOD_PRESS_STATUS_PATH << " does not exist";
        }
        if (!fileExists(FOD_FINGER_STATE_PATH)) {
            LOG(DEBUG) << FOD_FINGER_STATE_PATH << " does not exist (optional)";
        }
        
        LOG(INFO) << "Xiaomi SM8550 UDFPS handler initialized";
    }

    void onFingerDown(uint32_t x, uint32_t y, float /*minor*/, float /*major*/) {
        LOG(DEBUG) << __func__ << " x: " << x << ", y: " << y;

        // Отправить координаты и статус нажатия драйверу
        mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_X, x);
        mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_Y, y);
        mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_STATUS, PARAM_FOD_PRESSED);

        // Включить локальный HBM через sysfs
        set(DISP_PARAM_PATH,
            std::string(DISP_PARAM_LOCAL_HBM_MODE) + " " + DISP_PARAM_LOCAL_HBM_ON);

        // Включить подсветку NIT
        mDevice->extCmd(mDevice, COMMAND_NIT, TARGET_BRIGHTNESS_1000NIT);

        // Обновить статус пальца в sysfs
        set(FOD_PRESS_STATUS_PATH, FOD_STATUS_ON);
        
        // Опционально: обновить fod_finger_status если существует
        if (fileExists(FOD_FINGER_STATE_PATH)) {
            set(FOD_FINGER_STATE_PATH, "1");
        }

        LOG(DEBUG) << "Finger down processed";
    }

    void onFingerUp() {
        LOG(DEBUG) << __func__;

        // Сбросить координаты и статус нажатия
        mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_X, 0);
        mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_Y, 0);
        mDevice->extCmd(mDevice, COMMAND_FOD_PRESS_STATUS, PARAM_FOD_RELEASED);

        // Выключить локальный HBM через sysfs
        set(DISP_PARAM_PATH,
            std::string(DISP_PARAM_LOCAL_HBM_MODE) + " " + DISP_PARAM_LOCAL_HBM_OFF);

        // Выключить подсветку NIT
        mDevice->extCmd(mDevice, COMMAND_NIT, TARGET_BRIGHTNESS_OFF);

        // Обновить статус пальца в sysfs
        set(FOD_PRESS_STATUS_PATH, FOD_STATUS_OFF);
        
        // Опционально: сбросить fod_finger_status если существует
        if (fileExists(FOD_FINGER_STATE_PATH)) {
            set(FOD_FINGER_STATE_PATH, "0");
        }

        LOG(DEBUG) << "Finger up processed";
    }

    void onAcquired(int32_t result, int32_t vendorCode) {
        LOG(DEBUG) << __func__ << " result: " << result << " vendorCode: " << vendorCode;
        
        if (result == FINGERPRINT_ACQUIRED_VENDOR) {
            // Обработка vendor-specific событий
            switch (vendorCode) {
                case 21: // waiting for fingerprint authentication
                case 23: // waiting for fingerprint enroll
                    set(FOD_PRESS_STATUS_PATH, FOD_STATUS_ON);
                    break;
                case 22: // fingerprint authentication succeed
                case 24: // fingerprint enroll succeed  
                case 44: // fingerprint scan failed
                    onFingerUp();
                    break;
                default:
                    LOG(DEBUG) << "Unhandled vendor code: " << vendorCode;
                    break;
            }
        } else {
            // Обработка стандартных событий
            switch (static_cast<AcquiredInfo>(result)) {
                case AcquiredInfo::GOOD:
                case AcquiredInfo::PARTIAL:
                case AcquiredInfo::INSUFFICIENT:
                case AcquiredInfo::SENSOR_DIRTY:
                case AcquiredInfo::TOO_SLOW:
                case AcquiredInfo::TOO_FAST:
                case AcquiredInfo::TOO_DARK:
                case AcquiredInfo::TOO_BRIGHT:
                case AcquiredInfo::IMMOBILE:
                case AcquiredInfo::LIFT_TOO_SOON:
                    onFingerUp();
                    break;
                default:
                    LOG(DEBUG) << "Unhandled acquired info: " << result;
                    break;
            }
        }
    }

    void onAuthenticationSucceeded() {
        LOG(DEBUG) << __func__;
        onFingerUp();
    }

    void onAuthenticationFailed() {
        LOG(DEBUG) << __func__;
        onFingerUp();
    }

    void cancel() {
        LOG(DEBUG) << __func__;
        onFingerUp();
    }

  private:
    fingerprint_device_t* mDevice;
};

static UdfpsHandler* create() {
    return new XiaomiSm8550UdfpsHandler();
}

static void destroy(UdfpsHandler* handler) {
    delete handler;
}

extern "C" UdfpsHandlerFactory UDFPS_HANDLER_FACTORY = {
        .create = create,
        .destroy = destroy,
};
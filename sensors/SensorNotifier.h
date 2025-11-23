/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <android/frameworks/sensorservice/1.0/ISensorManager.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

using android::sp;
using android::frameworks::sensorservice::V1_0::IEventQueue;
using android::frameworks::sensorservice::V1_0::IEventQueueCallback;
using android::frameworks::sensorservice::V1_0::ISensorManager;
using android::frameworks::sensorservice::V1_0::Result;

class SensorNotifier {
  public:
    SensorNotifier(sp<ISensorManager> manager);
    virtual ~SensorNotifier();

    void activate();
    void deactivate();
    bool isActive() const { return mActive.load(); }

    struct Statistics {
        int64_t eventsProcessed = 0;
        int64_t errorsCount = 0;
        int64_t restartsCount = 0;
        std::chrono::steady_clock::time_point startTime;
    };
    
    Statistics getStatistics() const;
    std::chrono::seconds getUptime() const;
    
    void incrementEventsProcessed();
    void incrementErrors();
    void incrementRestarts();

  protected:
    Result initializeSensorQueue(std::string typeAsString, bool wakeup, sp<IEventQueueCallback>);
    virtual void pollingFunction() = 0;

    sp<IEventQueue> mQueue;
    int32_t mSensorHandle = -1;
    std::atomic<bool> mActive{false};

  private:
    sp<ISensorManager> mManager;
    std::thread mPollingThread;
    mutable std::mutex mThreadMutex;
    mutable std::mutex mStatsMutex;
    Statistics mStatistics;
};
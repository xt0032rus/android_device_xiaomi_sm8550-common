/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "SensorNotifier"

#include <android-base/logging.h>
#include <algorithm>
#include <chrono>

#include "SensorNotifier.h"
#include "Config.h"

using android::hardware::sensors::V1_0::SensorFlagBits;
using android::hardware::sensors::V1_0::SensorInfo;

SensorNotifier::SensorNotifier(sp<ISensorManager> manager) : mManager(manager) {
    auto& config = Config::getInstance().getSettings();
    mStatistics.startTime = std::chrono::steady_clock::now();
    
    if (config.logLevel == "VERBOSE") {
        LOG(VERBOSE) << "SensorNotifier constructor called";
    }
}

SensorNotifier::~SensorNotifier() {
    auto& config = Config::getInstance().getSettings();
    
    if (config.logLevel == "VERBOSE") {
        LOG(VERBOSE) << "SensorNotifier destructor called";
    }
    
    deactivate();
    if (mQueue != nullptr) {
        /*
         * Free the event queue.
         * kernel calls decStrong() on server side implementation of IEventQueue,
         * hence resources (including the callback) are freed as well.
         */
        mQueue = nullptr;
    }
    
    if (config.logLevel == "DEBUG" || config.logLevel == "VERBOSE") {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - mStatistics.startTime);
        LOG(INFO) << "SensorNotifier statistics - Duration: " << duration.count() 
                  << "s, Events: " << mStatistics.eventsProcessed
                  << ", Errors: " << mStatistics.errorsCount
                  << ", Restarts: " << mStatistics.restartsCount;
    }
}

Result SensorNotifier::initializeSensorQueue(std::string typeAsString, bool wakeup,
                                             sp<IEventQueueCallback> callback) {
    auto& config = Config::getInstance().getSettings();
    Result res;
    std::vector<SensorInfo> sensorList;

    if (mManager == nullptr) {
        LOG(ERROR) << "ISensorManager is null";
        incrementErrors();
        return Result::BAD_VALUE;
    }

    if (config.logLevel == "VERBOSE" || config.logLevel == "DEBUG") {
        LOG(INFO) << "Initializing sensor queue for: " << typeAsString 
                  << " (wakeup: " << wakeup << ")";
    }

    mManager->getSensorList([&sensorList, &res](const auto& l, auto r) {
        sensorList = l;
        res = r;
    });
    
    if (res != Result::OK) {
        LOG(ERROR) << "Failed to get sensors list, result: " << static_cast<int>(res);
        incrementErrors();
        return res;
    }
    
    if (config.logLevel == "VERBOSE") {
        LOG(VERBOSE) << "Available sensors (" << sensorList.size() << "):";
        for (const auto& sensor : sensorList) {
            LOG(VERBOSE) << "  - " << sensor.typeAsString << " (wakeup: " 
                         << ((sensor.flags & SensorFlagBits::WAKE_UP) != 0) << ")";
        }
    }
    
    auto it = std::find_if(sensorList.begin(), sensorList.end(),
                           [this, &typeAsString, &wakeup](const SensorInfo& sensor) {
                               return (sensor.typeAsString == typeAsString) &&
                                      ((sensor.flags & SensorFlagBits::WAKE_UP) == wakeup);
                           });

    if (it != sensorList.end()) {
        mSensorHandle = it->sensorHandle;
        LOG(INFO) << "Found sensor: " << typeAsString << " with handle: " << mSensorHandle;
        
        if (config.logLevel == "VERBOSE") {
            LOG(VERBOSE) << "Sensor details - Name: " << it->name 
                         << ", Vendor: " << it->vendor
                         << ", Version: " << it->version
                         << ", Type: " << static_cast<int>(it->type)
                         << ", MaxRange: " << it->maxRange
                         << ", Resolution: " << it->resolution
                         << ", Power: " << it->power << " mA";
        }
    } else {
        LOG(ERROR) << "Failed to get " << typeAsString << " sensor with wake-up: " << wakeup;
        incrementErrors();
        
        if (config.logLevel == "DEBUG" || config.logLevel == "VERBOSE") {
            LOG(DEBUG) << "Available sensors matching type:";
            for (const auto& sensor : sensorList) {
                if (sensor.typeAsString == typeAsString) {
                    LOG(DEBUG) << "  - " << sensor.typeAsString 
                              << " (wakeup: " << ((sensor.flags & SensorFlagBits::WAKE_UP) != 0)
                              << ", handle: " << sensor.sensorHandle << ")";
                }
            }
        }
        return Result::NOT_EXIST;
    }

    mManager->createEventQueue(callback, [this, &res](const auto& q, auto r) {
        this->mQueue = q;
        res = r;
    });
    
    if (res != Result::OK) {
        LOG(ERROR) << "Failed to create event queue, result: " << static_cast<int>(res);
        incrementErrors();
        return res;
    }

    LOG(INFO) << "Successfully initialized sensor queue for: " << typeAsString;
    return Result::OK;
}

void SensorNotifier::activate() {
    auto& config = Config::getInstance().getSettings();
    std::lock_guard<std::mutex> lock(mThreadMutex);
    
    if (mActive.load()) {
        LOG(WARNING) << "SensorNotifier already active";
        return;
    }
    
    if (mQueue == nullptr) {
        LOG(ERROR) << "Cannot activate - event queue is null";
        incrementErrors();
        return;
    }
    
    mActive = true;
    
    mPollingThread = std::thread(&SensorNotifier::pollingFunction, this);
    
    if (!mPollingThread.joinable()) {
        LOG(ERROR) << "Failed to start polling thread";
        incrementErrors();
        mActive = false;
        return;
    }
    
    if (config.logLevel == "VERBOSE") {
        LOG(VERBOSE) << "SensorNotifier polling thread started successfully";
    }
    
    LOG(INFO) << "SensorNotifier activated successfully";
}

void SensorNotifier::deactivate() {
    auto& config = Config::getInstance().getSettings();
    std::lock_guard<std::mutex> lock(mThreadMutex);
    
    if (!mActive.load()) {
        if (config.logLevel == "VERBOSE") {
            LOG(VERBOSE) << "SensorNotifier already deactivated";
        }
        return;
    }
    
    if (config.logLevel == "DEBUG") {
        LOG(DEBUG) << "Deactivating SensorNotifier...";
    }
    
    mActive = false;
    
    if (mPollingThread.joinable()) {
        if (config.logLevel == "VERBOSE") {
            LOG(VERBOSE) << "Waiting for polling thread to finish...";
        }
        
        mPollingThread.join();
        
        if (config.logLevel == "VERBOSE") {
            LOG(VERBOSE) << "Polling thread finished successfully";
        }
    }
    
    if (mQueue != nullptr && mSensorHandle != -1) {
        auto result = mQueue->disableSensor(mSensorHandle);
        if (result.isOk()) {
            Result disableResult = result;
            if (disableResult != Result::OK && config.logLevel != "ERROR") {
                LOG(WARNING) << "Sensor was already disabled or error during disable: " 
                            << static_cast<int>(disableResult);
            } else if (config.logLevel == "VERBOSE") {
                LOG(VERBOSE) << "Sensor disabled successfully during deactivation";
            }
        } else {
            LOG(ERROR) << "Failed to disable sensor: transport error";
            incrementErrors();
        }
    }
    
    LOG(INFO) << "SensorNotifier deactivated";
    
    if (config.logLevel == "DEBUG") {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - mStatistics.startTime);
        LOG(DEBUG) << "Session statistics - Active time: " << duration.count() 
                  << "s, Events processed: " << mStatistics.eventsProcessed;
    }
}

void SensorNotifier::incrementEventsProcessed() {
    std::lock_guard<std::mutex> lock(mStatsMutex);
    mStatistics.eventsProcessed++;
    
    auto& config = Config::getInstance().getSettings();
    if (config.logLevel == "VERBOSE" && mStatistics.eventsProcessed % 100 == 0) {
        LOG(VERBOSE) << "Processed " << mStatistics.eventsProcessed << " events";
    }
}

void SensorNotifier::incrementErrors() {
    std::lock_guard<std::mutex> lock(mStatsMutex);
    mStatistics.errorsCount++;
    
    auto& config = Config::getInstance().getSettings();
    if (config.logLevel == "DEBUG" && mStatistics.errorsCount % 10 == 0) {
        LOG(DEBUG) << "Error count: " << mStatistics.errorsCount;
    }
}

void SensorNotifier::incrementRestarts() {
    std::lock_guard<std::mutex> lock(mStatsMutex);
    mStatistics.restartsCount++;
    
    auto& config = Config::getInstance().getSettings();
    LOG(WARNING) << "Component restarted (count: " << mStatistics.restartsCount << ")";
}

SensorNotifier::Statistics SensorNotifier::getStatistics() const { 
    std::lock_guard<std::mutex> lock(mStatsMutex);
    return mStatistics; 
}

std::chrono::seconds SensorNotifier::getUptime() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - mStatistics.startTime);
}
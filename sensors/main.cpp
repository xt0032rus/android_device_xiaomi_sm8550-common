/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "SensorNotifier"

#include <android-base/logging.h>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <csignal>
#include <sys/stat.h>

#include "AodNotifier.h"
#include "NonUiNotifier.h"
#include "Config.h"
#include "HealthMonitor.h"

std::atomic<bool> gRunning{true};
std::unique_ptr<AodNotifier> gAodNotifier;
std::unique_ptr<NonUiNotifier> gNonUiNotifier;
sp<ISensorManager> gSensorManager;

void printConfiguration(const Config::Settings& config) {
    LOG(INFO) << "=== Loaded Configuration ===";
    LOG(INFO) << "Log Level: " << config.logLevel;
    LOG(INFO) << "Poll Timeout: " << config.pollTimeoutMs << "ms";
    LOG(INFO) << "Max Consecutive Errors: " << config.maxConsecutiveErrors;
    LOG(INFO) << "Error Retry Delay: " << config.errorRetryDelayMs << "ms";
    LOG(INFO) << "Heartbeat Interval: " << config.heartbeatIntervalSec << "s";
    LOG(INFO) << "AOD Notifier: " << (config.enableAodNotifier ? "ENABLED" : "DISABLED");
    LOG(INFO) << "NonUI Notifier: " << (config.enableNonUiNotifier ? "ENABLED" : "DISABLED");
    LOG(INFO) << "Sensor Sample Period: " << config.sensorSamplePeriod << "us";
    LOG(INFO) << "Sensor Latency: " << config.sensorLatency << "us";
    LOG(INFO) << "Health Check Interval: " << config.healthCheckIntervalSec << "s";
    LOG(INFO) << "Component Timeout: " << config.componentTimeoutSec << "s";
    LOG(INFO) << "Max Restart Attempts: " << config.maxRestartAttempts;
    LOG(INFO) << "Recovery Retry Delay: " << config.recoveryRetryDelayMs << "ms";
    LOG(INFO) << "Auto Recovery: " << (config.enableAutoRecovery ? "ENABLED" : "DISABLED");
    LOG(INFO) << "Touch Device: " << config.touchDevicePath;
    LOG(INFO) << "Display Device: " << config.displayDevicePath;
    LOG(INFO) << "=================================";
}

void onComponentHealthChange(const std::string& component, bool healthy) {
    auto& config = Config::getInstance().getSettings();
    
    if (healthy) {
        LOG(INFO) << "Component " << component << " is now healthy";
    } else {
        LOG(ERROR) << "Component " << component << " is unhealthy";
        
        if (!config.enableAutoRecovery) {
            LOG(WARNING) << "Auto recovery is disabled, skipping recovery for " << component;
            return;
        }

        if (component == "AodNotifier" && gAodNotifier) {
            LOG(WARNING) << "Attempting to recover AodNotifier...";
            
            gAodNotifier->deactivate();
            std::this_thread::sleep_for(std::chrono::milliseconds(config.recoveryRetryDelayMs));
            
            gAodNotifier = std::make_unique<AodNotifier>(gSensorManager);
            if (gAodNotifier) {
                gAodNotifier->activate();
                LOG(INFO) << "AodNotifier recovery completed successfully";
                
                HealthMonitor::getInstance().updateHeartbeat("AodNotifier");
            } else {
                LOG(ERROR) << "AodNotifier recovery failed: memory allocation error";
            }
            
        } else if (component == "NonUiNotifier" && gNonUiNotifier) {
            LOG(WARNING) << "Attempting to recover NonUiNotifier...";
            
            gNonUiNotifier->deactivate();
            std::this_thread::sleep_for(std::chrono::milliseconds(config.recoveryRetryDelayMs));
            
            gNonUiNotifier = std::make_unique<NonUiNotifier>(gSensorManager);
            if (gNonUiNotifier) {
                gNonUiNotifier->activate();
                LOG(INFO) << "NonUiNotifier recovery completed successfully";
                
                HealthMonitor::getInstance().updateHeartbeat("NonUiNotifier");
            } else {
                LOG(ERROR) << "NonUiNotifier recovery failed: memory allocation error";
            }
        }
    }
}

bool onRecoveryRequested(const std::string& component) {
    auto& config = Config::getInstance().getSettings();
    
    LOG(INFO) << "Recovery requested for: " << component;
    
    if (component == "AodNotifier" && gAodNotifier) {
        gAodNotifier->deactivate();
        std::this_thread::sleep_for(std::chrono::milliseconds(config.recoveryRetryDelayMs));
        gAodNotifier->activate();
        return true;
    } else if (component == "NonUiNotifier" && gNonUiNotifier) {
        gNonUiNotifier->deactivate();
        std::this_thread::sleep_for(std::chrono::milliseconds(config.recoveryRetryDelayMs));
        gNonUiNotifier->activate();
        return true;
    }
    
    return false;
}

void signalHandler(int signal) {
    LOG(INFO) << "Received signal: " << signal;
    gRunning = false;
}

bool checkDeviceAccessibility(const Config::Settings& config) {
    const std::vector<std::string> devices = {
        config.touchDevicePath,
        config.displayDevicePath
    };
    
    bool allAccessible = true;
    for (const auto& device : devices) {
        struct stat st;
        if (stat(device.c_str(), &st) != 0) {
            LOG(ERROR) << "Device not accessible: " << device;
            allAccessible = false;
        } else if (!S_ISCHR(st.st_mode)) {
            LOG(ERROR) << "Not a character device: " << device;
            allAccessible = false;
        } else {
            LOG(INFO) << "Device accessible: " << device;
        }
    }
    
    return allAccessible;
}

void recoverAllComponents() {
    auto& config = Config::getInstance().getSettings();
    
    LOG(WARNING) << "Attempting to recover all components...";
    
    if (gAodNotifier) {
        LOG(INFO) << "Recovering AodNotifier...";
        gAodNotifier->deactivate();
        std::this_thread::sleep_for(std::chrono::milliseconds(config.recoveryRetryDelayMs));
        gAodNotifier->activate();
        HealthMonitor::getInstance().updateHeartbeat("AodNotifier");
    }
    
    if (gNonUiNotifier) {
        LOG(INFO) << "Recovering NonUiNotifier...";
        gNonUiNotifier->deactivate();
        std::this_thread::sleep_for(std::chrono::milliseconds(config.recoveryRetryDelayMs));
        gNonUiNotifier->activate();
        HealthMonitor::getInstance().updateHeartbeat("NonUiNotifier");
    }
    
    LOG(INFO) << "All components recovery completed";
}

int main(int argc, char** argv) {
    LOG(INFO) << "SensorNotifier service starting...";
    
    Config::getInstance().loadConfig();
    auto& config = Config::getInstance().getSettings();
    
    printConfiguration(config);
    
    if (config.logLevel == "VERBOSE") {
        android::base::SetMinimumLogSeverity(android::base::VERBOSE);
    } else if (config.logLevel == "DEBUG") {
        android::base::SetMinimumLogSeverity(android::base::DEBUG);
    } else if (config.logLevel == "WARNING") {
        android::base::SetMinimumLogSeverity(android::base::WARNING);
    } else if (config.logLevel == "ERROR") {
        android::base::SetMinimumLogSeverity(android::base::ERROR);
    }
    
    if (!checkDeviceAccessibility(config)) {
        LOG(ERROR) << "Required devices are not accessible";
        return EXIT_FAILURE;
    }
    
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGINT, signalHandler);
    std::signal(SIGPIPE, SIG_IGN);
    std::signal(SIGUSR1, [](int) { recoverAllComponents(); });
    
    HealthMonitor& healthMonitor = HealthMonitor::getInstance();
    healthMonitor.setHealthCallback(onComponentHealthChange);
    healthMonitor.setRecoveryCallback(onRecoveryRequested);
    
    if (config.enableAodNotifier) {
        healthMonitor.registerComponent("AodNotifier", 
                                      std::chrono::seconds(config.componentTimeoutSec),
                                      config.maxRestartAttempts);
    }
    if (config.enableNonUiNotifier) {
        healthMonitor.registerComponent("NonUiNotifier", 
                                      std::chrono::seconds(config.componentTimeoutSec),
                                      config.maxRestartAttempts);
    }
    
    healthMonitor.startMonitoring();
    
    int retryCount = 0;
    
    while (retryCount < config.maxConsecutiveErrors && gRunning) {
        gSensorManager = ISensorManager::getService();
        if (gSensorManager != nullptr) {
            break;
        }
        LOG(WARNING) << "Failed to get ISensorManager, retry " << (retryCount + 1) << "/" << config.maxConsecutiveErrors;
        std::this_thread::sleep_for(std::chrono::milliseconds(config.errorRetryDelayMs));
        retryCount++;
    }

    if (gSensorManager == nullptr) {
        LOG(ERROR) << "Failed to get ISensorManager after " << config.maxConsecutiveErrors << " attempts";
        healthMonitor.stopMonitoring();
        return EXIT_FAILURE;
    }

    if (config.enableAodNotifier) {
        gAodNotifier = std::make_unique<AodNotifier>(gSensorManager);
        if (gAodNotifier) {
            gAodNotifier->activate();
            healthMonitor.updateHeartbeat("AodNotifier");
            LOG(INFO) << "AodNotifier initialized and activated";
        } else {
            LOG(ERROR) << "Failed to create AodNotifier";
        }
    }
    
    if (config.enableNonUiNotifier) {
        gNonUiNotifier = std::make_unique<NonUiNotifier>(gSensorManager);
        if (gNonUiNotifier) {
            gNonUiNotifier->activate();
            healthMonitor.updateHeartbeat("NonUiNotifier");
            LOG(INFO) << "NonUiNotifier initialized and activated";
        } else {
            LOG(ERROR) << "Failed to create NonUiNotifier";
        }
    }

    LOG(INFO) << "SensorNotifier service started successfully";
    LOG(INFO) << "Send SIGUSR1 to force recovery of all components: kill -SIGUSR1 " << getpid();

    int heartbeatCounter = 0;
    auto lastHealthReport = std::chrono::steady_clock::now();
    
    while (gRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        heartbeatCounter++;
        
        auto now = std::chrono::steady_clock::now();
        
        if (gAodNotifier && gAodNotifier->isActive()) {
            healthMonitor.updateHeartbeat("AodNotifier");
        }
        if (gNonUiNotifier && gNonUiNotifier->isActive()) {
            healthMonitor.updateHeartbeat("NonUiNotifier");
        }
        
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastHealthReport) > 
            std::chrono::seconds(config.heartbeatIntervalSec)) {
            lastHealthReport = now;
            
            auto unhealthy = healthMonitor.getUnhealthyComponents();
            if (unhealthy.empty()) {
                LOG(INFO) << "Health report: All components healthy";
            } else {
                LOG(WARNING) << "Health report: " << unhealthy.size() << " unhealthy components";
                for (const auto& comp : unhealthy) {
                    auto health = healthMonitor.getComponentHealth(comp);
                    LOG(WARNING) << " - " << comp << " (restart attempts: " << health.restartCount << ")";
                }
            }
            
            if (config.enableAodNotifier && gAodNotifier) {
                LOG(INFO) << "AodNotifier restarts: " << healthMonitor.getRestartCount("AodNotifier");
            }
            if (config.enableNonUiNotifier && gNonUiNotifier) {
                LOG(INFO) << "NonUiNotifier restarts: " << healthMonitor.getRestartCount("NonUiNotifier");
            }
        }
    }

    LOG(INFO) << "Shutting down SensorNotifier service...";
    
    if (gAodNotifier) {
        gAodNotifier->deactivate();
    }
    if (gNonUiNotifier) {
        gNonUiNotifier->deactivate();
    }
    
    healthMonitor.stopMonitoring();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(config.recoveryRetryDelayMs));
    
    LOG(INFO) << "SensorNotifier service stopped gracefully";
    return EXIT_SUCCESS;
}
#include "HealthMonitor.h"
#include <android-base/logging.h>

HealthMonitor& HealthMonitor::getInstance() {
    static HealthMonitor instance;
    return instance;
}

HealthMonitor::HealthMonitor() {}

void HealthMonitor::registerComponent(const std::string& name, 
                                    std::chrono::seconds timeout,
                                    int maxRestarts) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto now = std::chrono::steady_clock::now();
    mComponents[name] = {name, true, now, 0, now, maxRestarts, timeout};
    LOG(INFO) << "Registered health monitor for: " << name 
              << " (timeout: " << timeout.count() << "s, maxRestarts: " << maxRestarts << ")";
}

void HealthMonitor::updateHeartbeat(const std::string& name) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mComponents.find(name);
    if (it != mComponents.end()) {
        it->second.lastHeartbeat = std::chrono::steady_clock::now();
        if (!it->second.isAlive) {
            it->second.isAlive = true;
            it->second.restartCount = 0;
            LOG(INFO) << "Component " << name << " recovered automatically";
            
            if (mHealthCallback) {
                mHealthCallback(name, true);
            }
        }
    }
}

void HealthMonitor::setHealthCallback(HealthCallback callback) {
    std::lock_guard<std::mutex> lock(mMutex);
    mHealthCallback = callback;
}

void HealthMonitor::setRecoveryCallback(RecoveryCallback callback) {
    std::lock_guard<std::mutex> lock(mMutex);
    mRecoveryCallback = callback;
}

void HealthMonitor::startMonitoring() {
    if (mMonitoring.exchange(true)) {
        return;
    }
    
    mMonitorThread = std::thread(&HealthMonitor::monitoringLoop, this);
    LOG(INFO) << "Health monitoring started";
}

void HealthMonitor::stopMonitoring() {
    mMonitoring = false;
    if (mMonitorThread.joinable()) {
        mMonitorThread.join();
    }
    LOG(INFO) << "Health monitoring stopped";
}

bool HealthMonitor::canRestartComponent(const std::string& name) const {
    auto it = mComponents.find(name);
    if (it != mComponents.end()) {
        return it->second.restartCount < it->second.maxRestarts;
    }
    return false;
}

void HealthMonitor::monitoringLoop() {
    while (mMonitoring) {
        std::this_thread::sleep_for(mCheckInterval);
        
        std::vector<std::string> newlyUnhealthyComponents;
        auto now = std::chrono::steady_clock::now();
        
        {
            std::lock_guard<std::mutex> lock(mMutex);
            for (auto& [name, health] : mComponents) {
                auto timeSinceHeartbeat = std::chrono::duration_cast<std::chrono::seconds>(
                    now - health.lastHeartbeat);
                
                if (timeSinceHeartbeat > health.timeout) {
                    bool wasHealthy = health.isAlive;
                    health.isAlive = false;
                    
                    if (wasHealthy) {
                        newlyUnhealthyComponents.push_back(name);
                        LOG(ERROR) << "Component " << name << " became unhealthy "
                                   << "(no heartbeat for " << timeSinceHeartbeat.count() << "s)";
                        
                        if (mHealthCallback) {
                            mHealthCallback(name, false);
                        }
                    }
                }
            }
        }
        
        for (const auto& name : newlyUnhealthyComponents) {
            if (canRestartComponent(name)) {
                LOG(WARNING) << "Attempting automatic recovery for: " << name;
                triggerRecovery(name);
            } else {
                LOG(ERROR) << "Cannot restart " << name << " - max restart attempts exceeded";
            }
        }
    }
}

HealthMonitor::ComponentHealth HealthMonitor::getComponentHealth(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mComponents.find(name);
    return it != mComponents.end() ? it->second : ComponentHealth{name, false, {}, 0, {}};
}

std::vector<std::string> HealthMonitor::getUnhealthyComponents() const {
    std::vector<std::string> unhealthy;
    std::lock_guard<std::mutex> lock(mMutex);
    
    for (const auto& [name, health] : mComponents) {
        if (!health.isAlive) {
            unhealthy.push_back(name);
        }
    }
    
    return unhealthy;
}

bool HealthMonitor::triggerRecovery(const std::string& component) {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mComponents.find(component);
    if (it == mComponents.end()) {
        LOG(ERROR) << "Component not registered: " << component;
        return false;
    }
    
    if (it->second.restartCount >= it->second.maxRestarts) {
        LOG(ERROR) << "Max restart attempts exceeded for: " << component;
        return false;
    }
    
    it->second.restartCount++;
    LOG(WARNING) << "Triggering recovery for " << component 
                 << " (attempt " << it->second.restartCount << "/" << it->second.maxRestarts << ")";
    
    if (mRecoveryCallback) {
        bool success = mRecoveryCallback(component);
        if (success) {
            it->second.isAlive = true;
            it->second.lastHeartbeat = std::chrono::steady_clock::now();
            LOG(INFO) << "Recovery successful for: " << component;
        } else {
            LOG(ERROR) << "Recovery failed for: " << component;
        }
        return success;
    }
    
    return false;
}

int HealthMonitor::getRestartCount(const std::string& component) const {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mComponents.find(component);
    return it != mComponents.end() ? it->second.restartCount : 0;
}
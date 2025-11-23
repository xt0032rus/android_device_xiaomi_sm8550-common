#pragma once

#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <functional>
#include <vector>
#include <unordered_map>
#include <string>

class HealthMonitor {
public:
    struct ComponentHealth {
        std::string name;
        bool isAlive;
        std::chrono::steady_clock::time_point lastHeartbeat;
        int restartCount;
        std::chrono::steady_clock::time_point startTime;
        int maxRestarts;
        std::chrono::seconds timeout;
    };
    
    using HealthCallback = std::function<void(const std::string& component, bool healthy)>;
    using RecoveryCallback = std::function<bool(const std::string& component)>;
    
    static HealthMonitor& getInstance();
    
    void registerComponent(const std::string& name, 
                         std::chrono::seconds timeout = std::chrono::seconds(60),
                         int maxRestarts = 3);
    void updateHeartbeat(const std::string& name);
    void setHealthCallback(HealthCallback callback);
    void setRecoveryCallback(RecoveryCallback callback);
    void startMonitoring();
    void stopMonitoring();
    ComponentHealth getComponentHealth(const std::string& name) const;
    std::vector<std::string> getUnhealthyComponents() const;
    bool triggerRecovery(const std::string& component);
    int getRestartCount(const std::string& component) const;
    
private:
    HealthMonitor();
    void monitoringLoop();
    bool canRestartComponent(const std::string& name) const;
    
    mutable std::mutex mMutex;
    std::unordered_map<std::string, ComponentHealth> mComponents;
    HealthCallback mHealthCallback;
    RecoveryCallback mRecoveryCallback;
    std::atomic<bool> mMonitoring{false};
    std::thread mMonitorThread;
    std::chrono::seconds mCheckInterval{30};
};
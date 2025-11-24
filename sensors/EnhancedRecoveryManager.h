#pragma once

#include <string>
#include <functional>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <unordered_map>

class EnhancedRecoveryManager {
public:
    enum class ComponentState {
        UNINITIALIZED,
        INITIALIZING,
        RUNNING,
        DEGRADED,
        RECOVERING,
        FAILED
    };
    
    enum class RecoveryAction {
        NONE,
        QUICK_RESTART,
        FULL_RECONSTRUCT,
        DEVICE_REINIT,
        DELAYED_RESTART,
        FALLBACK_MODE
    };
    
    struct ComponentStatus {
        std::string name;
        ComponentState state = ComponentState::UNINITIALIZED;
        int restartCount = 0;
        int errorCount = 0;
        std::chrono::steady_clock::time_point lastHeartbeat;
        std::chrono::steady_clock::time_point lastStateChange;
        RecoveryAction lastRecoveryAction = RecoveryAction::NONE;
        bool usingFallback = false;
    };
    
    static EnhancedRecoveryManager& getInstance();
    
    void registerComponent(const std::string& name);
    void updateComponentState(const std::string& name, ComponentState state);
    void reportError(const std::string& name, const std::string& error);
    bool executeRecovery(const std::string& name);
    ComponentStatus getComponentStatus(const std::string& name) const;
    
    void setStateChangeCallback(std::function<void(const std::string&, ComponentState, ComponentState)> callback);
    void setRecoveryStrategy(std::function<RecoveryAction(const ComponentStatus&)> strategy);
    
private:
    EnhancedRecoveryManager();
    RecoveryAction determineRecoveryAction(const ComponentStatus& status);
    bool performRecovery(const std::string& name, RecoveryAction action);
    
    mutable std::mutex mMutex;
    std::unordered_map<std::string, ComponentStatus> mComponents;
    std::function<void(const std::string&, ComponentState, ComponentState)> mStateChangeCallback;
    std::function<RecoveryAction(const ComponentStatus&)> mRecoveryStrategy;
};
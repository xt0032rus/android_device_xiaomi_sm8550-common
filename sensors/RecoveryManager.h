#pragma once

#include <string>
#include <functional>
#include <vector>

class RecoveryManager {
public:
    enum class RecoveryStrategy {
        IMMEDIATE_RESTART,
        GRACEFUL_RESTART,
        FULL_RECONSTRUCT,
        DELAYED_RESTART
    };
    
    struct RecoveryPolicy {
        RecoveryStrategy strategy;
        int maxAttempts;
        int baseDelayMs;
        bool enableBackoff;
    };
    
    static RecoveryManager& getInstance();
    
    void setRecoveryPolicy(const std::string& component, RecoveryPolicy policy);
    bool executeRecovery(const std::string& component);
    int getRecoveryAttempts(const std::string& component) const;
    void resetRecoveryStats(const std::string& component);
    
private:
    RecoveryManager() = default;
    
    std::unordered_map<std::string, RecoveryPolicy> mPolicies;
    std::unordered_map<std::string, int> mAttemptCounters;
    mutable std::mutex mMutex;
};
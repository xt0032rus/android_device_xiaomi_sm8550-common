#include "Config.h"
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

Config& Config::getInstance() {
    static Config instance;
    return instance;
}

void Config::loadConfig(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) {
        LOG(WARNING) << "Config file not found: " << configPath << ", using defaults";
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        parseConfigLine(line);
    }
    
    applySettings();
    LOG(INFO) << "Configuration loaded successfully from: " << configPath;
}

void Config::parseConfigLine(const std::string& line) {
    std::string trimmed = android::base::Trim(line);
    if (trimmed.empty() || trimmed[0] == '#') return;
    
    size_t pos = trimmed.find('=');
    if (pos == std::string::npos) return;
    
    std::string key = android::base::Trim(trimmed.substr(0, pos));
    std::string value = android::base::Trim(trimmed.substr(pos + 1));
    
    if (!key.empty() && !value.empty()) {
        mConfigMap[key] = value;
        LOG(VERBOSE) << "Config: " << key << " = " << value;
    }
}

void Config::applySettings() {
    mSettings.logLevel = getString("log_level", "INFO");
    mSettings.pollTimeoutMs = getInt("poll_timeout_ms", 5000);
    mSettings.maxConsecutiveErrors = getInt("max_consecutive_errors", 5);
    mSettings.errorRetryDelayMs = getInt("error_retry_delay_ms", 1000);
    mSettings.heartbeatIntervalSec = getInt("heartbeat_interval_sec", 60);
    
    mSettings.enableAodNotifier = getBool("enable_aod_notifier", true);
    mSettings.enableNonUiNotifier = getBool("enable_non_ui_notifier", true);
    
    mSettings.sensorSamplePeriod = getInt("sensor_sample_period", 20000);
    mSettings.sensorLatency = getInt("sensor_latency", 0);
    
    mSettings.healthCheckIntervalSec = getInt("health_check_interval_sec", 30);
    mSettings.componentTimeoutSec = getInt("component_timeout_sec", 45);
    mSettings.maxRestartAttempts = getInt("max_restart_attempts", 5);
    
    mSettings.recoveryRetryDelayMs = getInt("recovery_retry_delay_ms", 200);
    mSettings.enableAutoRecovery = getBool("enable_auto_recovery", true);
    
    mSettings.touchDevicePath = getString("touch_device_path", "/dev/xiaomi-touch");
    mSettings.displayDevicePath = getString("display_device_path", "/dev/mi_display/disp_feature");
}

std::string Config::getString(const std::string& key, const std::string& defaultValue) {
    auto it = mConfigMap.find(key);
    return it != mConfigMap.end() ? it->second : defaultValue;
}

int Config::getInt(const std::string& key, int defaultValue) {
    auto it = mConfigMap.find(key);
    if (it != mConfigMap.end()) {
        const std::string& value = it->second;
        if (value.empty()) {
            LOG(ERROR) << "Empty integer value for " << key;
            return defaultValue;
        }
        
        char* end;
        long result = strtol(value.c_str(), &end, 10);
        if (end == value.c_str() || *end != '\0') {
            LOG(ERROR) << "Invalid integer value for " << key << ": " << value;
            return defaultValue;
        }
        return static_cast<int>(result);
    }
    return defaultValue;
}

bool Config::getBool(const std::string& key, bool defaultValue) {
    auto it = mConfigMap.find(key);
    if (it != mConfigMap.end()) {
        std::string value = it->second;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        return value == "true" || value == "1" || value == "yes" || value == "on";
    }
    return defaultValue;
}

std::chrono::milliseconds Config::getMilliseconds(const std::string& key, int defaultValue) {
    return std::chrono::milliseconds(getInt(key, defaultValue));
}

std::chrono::seconds Config::getSeconds(const std::string& key, int defaultValue) {
    return std::chrono::seconds(getInt(key, defaultValue));
}
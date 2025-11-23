#pragma once

#include <string>
#include <unordered_map>
#include <chrono>

class Config {
public:
    static Config& getInstance();
    
    void loadConfig(const std::string& configPath = "/vendor/etc/sensor_notifier.conf");
    std::string getString(const std::string& key, const std::string& defaultValue = "");
    int getInt(const std::string& key, int defaultValue = 0);
    bool getBool(const std::string& key, bool defaultValue = false);
    std::chrono::milliseconds getMilliseconds(const std::string& key, int defaultValue = 0);
    std::chrono::seconds getSeconds(const std::string& key, int defaultValue = 0);
    
    struct Settings {
        std::string logLevel = "INFO";
        int pollTimeoutMs = 5000;
        int maxConsecutiveErrors = 5;
        int errorRetryDelayMs = 1000;
        int heartbeatIntervalSec = 60;
        
        bool enableAodNotifier = true;
        bool enableNonUiNotifier = true;
        
        int sensorSamplePeriod = 20000;
        int sensorLatency = 0;
        
        int healthCheckIntervalSec = 30;
        int componentTimeoutSec = 45;
        int maxRestartAttempts = 5;
        
        int recoveryRetryDelayMs = 200;
        bool enableAutoRecovery = true;
        
        std::string touchDevicePath = "/dev/xiaomi-touch";
        std::string displayDevicePath = "/dev/mi_display/disp_feature";
    };
    
    const Settings& getSettings() const { return mSettings; }
    void setSettings(const Settings& settings) { mSettings = settings; }

private:
    Config() = default;
    void parseConfigLine(const std::string& line);
    void applySettings();
    
    Settings mSettings;
    std::unordered_map<std::string, std::string> mConfigMap;
};
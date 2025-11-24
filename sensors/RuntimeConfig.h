class RuntimeConfig {
public:
    static RuntimeConfig& getInstance();
    
    void updateFromSystemProperties();
    void applyDynamicTuning();
    void setBatterySaverMode(bool enabled);
    void setPerformanceMode(bool enabled);
    
private:
    void loadDeviceSpecificTuning();
    void applyThermalThrottling();
};
class PerformanceMonitor {
public:
    struct Metrics {
        int64_t totalCycles = 0;
        int64_t errorCount = 0;
        std::chrono::microseconds totalProcessingTime{0};
        std::chrono::microseconds minCycleTime{0};
        std::chrono::microseconds maxCycleTime{0};
        std::chrono::steady_clock::time_point startTime;
    };
    
    PerformanceMonitor(const std::string& componentName);
    ~PerformanceMonitor();
    
    void startCycle();
    void endCycle();
    void recordError();
    Metrics getMetrics() const;
    
private:
    std::string mComponentName;
    Metrics mMetrics;
    std::chrono::steady_clock::time_point mCycleStart;
    mutable std::mutex mMutex;
};
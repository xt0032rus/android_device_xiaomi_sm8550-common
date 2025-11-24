class EnhancedAodNotifier : public SensorNotifier {
private:
    enum class DisplayState {
        UNKNOWN,
        OFF,
        ON,
        LP1,
        LP2,
        AOD_ACTIVE
    };
    
    struct AodContext {
        DisplayState currentState = DisplayState::UNKNOWN;
        DisplayState previousState = DisplayState::UNKNOWN;
        bool sensorEnabled = false;
        int consecutiveReadErrors = 0;
        int stateTransitionCount = 0;
        std::chrono::steady_clock::time_point lastStateChange;
        int currentBrightness = 0;
    };
    
    AodContext mContext;
    android::base::unique_fd mDisplayFd;
    EnhancedRecoveryManager& mRecoveryManager;
    
public:
    EnhancedAodNotifier(sp<ISensorManager> manager) 
        : SensorNotifier(manager)
        , mRecoveryManager(EnhancedRecoveryManager::getInstance()) {
        
        initializeEnhanced();
    }
    
private:
    void initializeEnhanced() {
        auto& config = Config::getInstance().getSettings();
        
        mDisplayFd.reset(open(config.devicePaths.display.c_str(), O_RDWR | O_NONBLOCK));
        
        if (mDisplayFd.get() == -1 && config.features.enableFallbackDevices) {
            LOG(WARNING) << "Primary display device unavailable, trying fallback...";
            mDisplayFd.reset(open(config.devicePaths.backupDisplay.c_str(), O_RDWR | O_NONBLOCK));
        }
        
        if (mDisplayFd.get() == -1) {
            LOG(ERROR) << "All display devices unavailable";
            mRecoveryManager.reportError("AodNotifier", "Display device initialization failed");
            return;
        }
        
        fcntl(mDisplayFd.get(), F_SETFL, O_NONBLOCK);
        
        if (initializeSensorQueueEnhanced()) {
            mRecoveryManager.updateComponentState("AodNotifier", 
                EnhancedRecoveryManager::ComponentState::RUNNING);
        }
    }
    
    bool initializeSensorQueueEnhanced() {
        auto& config = Config::getInstance().getSettings();
        
        for (int attempt = 0; attempt < config.limits.maxRestartAttempts; attempt++) {
            Result result = initializeSensorQueue("xiaomi.sensor.aod", true, new AodSensorCallback());
            
            if (result == Result::OK) {
                LOG(INFO) << "AOD sensor queue initialized successfully";
                return true;
            }
            
            if (result == Result::NOT_EXIST) {
                LOG(WARNING) << "AOD sensor not available, waiting...";
                std::this_thread::sleep_for(std::chrono::milliseconds(config.timing.initializationDelayMs));
                continue;
            }
            
            LOG(ERROR) << "Failed to initialize AOD sensor queue, attempt " 
                      << (attempt + 1) << "/" << config.limits.maxRestartAttempts;
            
            if (attempt < config.limits.maxRestartAttempts - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(config.timing.recoveryDelayMs));
            }
        }
        
        mRecoveryManager.reportError("AodNotifier", "Sensor queue initialization failed");
        return false;
    }
    
    void pollingFunction() override {
        LOG(INFO) << "Enhanced AodNotifier polling started";
        
        auto& config = Config::getInstance().getSettings();
        initializeDisplayEventMonitoring();
        
        StateMachine stateMachine;
        PerformanceMonitor perfMonitor("AodNotifier");
        
        while (mActive.load()) {
            auto cycleStart = std::chrono::steady_clock::now();
            
            if (!processDisplayEvents()) {
                handleDisplayError();
                continue;
            }
            
            stateMachine.update(mContext);
            
            if (!healthCheck()) {
                mRecoveryManager.reportError("AodNotifier", "Health check failed");
            }
            
            mRecoveryManager.updateComponentState("AodNotifier",
                EnhancedRecoveryManager::ComponentState::RUNNING);
            
            adaptiveSleep(cycleStart, config);
        }
        
        cleanup();
    }
    
    bool processDisplayEvents() {
        struct pollfd fds = {mDisplayFd.get(), POLLIN | POLLERR, 0};
        
        int rc = poll(&fds, 1, mConfig.pollTimeoutMs);
        if (rc < 0) {
            return false;
        }
        
        if (rc == 0 || !(fds.revents & POLLIN)) {
            return true;
        }
        
        auto event = parseDisplayEventEnhanced();
        if (!event) {
            mContext.consecutiveReadErrors++;
            return mContext.consecutiveReadErrors < mConfig.limits.maxConsecutiveErrors;
        }
        
        mContext.consecutiveReadErrors = 0;
        handleDisplayEvent(*event);
        return true;
    }
    
    std::shared_ptr<disp_event_resp> parseDisplayEventEnhanced() {
        disp_event header;
        
        for (int i = 0; i < 3; i++) {
            ssize_t bytesRead = read(mDisplayFd.get(), &header, sizeof(header));
            
            if (bytesRead == sizeof(header)) {
                return parseCompleteEvent(header);
            } else if (bytesRead == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            } else if (bytesRead < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return nullptr;
                }
                break;
            }
        }
        
        return nullptr;
    }
    
    void handleDisplayEvent(const disp_event_resp& event) {
        DisplayState newState = parseDisplayState(event);
        
        if (newState != mContext.currentState) {
            LOG(INFO) << "Display state transition: " 
                     << toString(mContext.currentState) << " -> " << toString(newState);
            
            performStateTransition(mContext.currentState, newState);
            mContext.previousState = mContext.currentState;
            mContext.currentState = newState;
            mContext.lastStateChange = std::chrono::steady_clock::now();
            mContext.stateTransitionCount++;
        }
    }
    
    void performStateTransition(DisplayState from, DisplayState to) {
        auto& config = Config::getInstance().getSettings();
        
        switch (to) {
            case DisplayState::LP1:
            case DisplayState::LP2:
            case DisplayState::AOD_ACTIVE:
                enableAodSensor();
                setAodBrightness(calculateOptimalBrightness());
                break;
                
            case DisplayState::ON:
                disableAodSensor();
                setNormalBrightness();
                break;
                
            case DisplayState::OFF:
                disableAodSensor();
                break;
                
            default:
                break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(config.timing.stabilizationDelayMs));
    }
    
    int calculateOptimalBrightness() {
        auto& config = Config::getInstance().getSettings();
        return config.advanced.aodBrightnessLevels[0];
    }
    
    void adaptiveSleep(std::chrono::steady_clock::time_point cycleStart, const Settings& config) {
        auto cycleTime = std::chrono::steady_clock::now() - cycleStart;
        auto sleepTime = std::chrono::milliseconds(config.timing.pollTimeoutMs) - cycleTime;
        
        if (sleepTime > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(sleepTime);
        } else if (config.features.enableAdaptivePolling) {
            LOG(WARNING) << "AodNotifier cycle overload: " 
                        << std::chrono::duration_cast<std::chrono::milliseconds>(cycleTime).count() << "ms";
        }
    }
};
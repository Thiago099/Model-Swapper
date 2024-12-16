#pragma once
#include "TimeClass.h"

class Config {
private:
    static inline Config* singleton = nullptr;
    Config() {
    }
public:
    static Config* GetSingleton() {
        if (!singleton) {
            singleton = new Config();
        }
        return singleton;
    }
    bool BypassTemporalActivation = false;
    Time NowOverride;
};
#pragma once
#include "TimeClass.h"
#include "Singleton.h"
class Config :public Singleton<Config>{
public:
    bool BypassTemporalActivation = false;
    Time NowOverride;
};
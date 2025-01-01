#pragma once
#include "Lib/TimeClass.h"
#include "Lib/Singleton.h"

class Config :public Singleton<Config>{
public:
    bool BypassTemporalActivation = false;
    Time NowOverride;
};
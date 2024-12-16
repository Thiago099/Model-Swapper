#pragma once
#include "Str.h"
#include "Wrapper.h"


class Manager {
    inline static Manager* singleton = nullptr;
    models sources;
    void Process(AVObject* arma, RE::FormID id);
public:
    static inline Manager* GetSingleton() {
        if (!singleton) {
            singleton = new Manager();
        }
        return singleton;
    }
    void Register(std::string key, variants value);
    void Process(RE::TESBoundObject* base, RE::FormID id);
    void Process(RE::TESObjectARMA* base, RE::FormID id);
};
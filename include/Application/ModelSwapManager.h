#pragma once

#include "Application/Model.h"
#include "Lib/Singleton.h"

class ModelSwapManager : public Singleton<ModelSwapManager> {
    models sources;
public:
    void Apply(RE::TESForm* base, variantId variant);
    const variantId Process(RE::TESForm* base, RefID id);
    void Register(std::string key, variants value);
};

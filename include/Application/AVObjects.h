#pragma once

#include "Application/Model.h"


class AVObject {
public:
    virtual ~AVObject() = default;
    virtual int32_t GetVariant(const models& models, int seed) = 0;
    virtual void Apply(const models& models, int32_t variant) = 0;
    virtual RE::TESForm* GetBase() = 0;
};


class AVObjectARMA final : public AVObject {
    const char* initialMaleThirdPersonModle = nullptr;
    const char* initialFemaleThirdPersonModle = nullptr;
    const char* initialMaleFirstPersonModle = nullptr;
    const char* initialFemaleFirstPersonModel = nullptr;
    RE::TESObjectARMA* base = nullptr;

public:

    explicit AVObjectARMA(RE::TESObjectARMA* base);
    RE::TESForm* GetBase() override {
        return base;
    }

    int32_t GetVariant(const models& models, int seed) override;
    virtual void Apply(const models& models, int32_t _variant) override;

};


class AVModel final : public AVObject {
    const char* model = nullptr;
    RE::TESForm* base = nullptr;
public:
    explicit AVModel(RE::TESForm* base);

    RE::TESForm* GetBase() override { return base; }

    int32_t GetVariant(const models& models, int seed) override;
    virtual void Apply(const models& models, int32_t _variant) override;
};


class AVObjectARMO : public AVObject {
    const char* male = nullptr;
    const char* female = nullptr;
    RE::TESObjectARMO* base = nullptr;

public:
    ~AVObjectARMO() {}
    AVObjectARMO(RE::TESObjectARMO* base);

    RE::TESForm* GetBase() override { return base; }

    int32_t GetVariant(const models& models, int seed) override;
    virtual void Apply(const models& models, int32_t _variant) override;
};

class AVObjectWEAP : public AVObject {
    const char* firstPersonModel = nullptr;
    const char* model = nullptr;
    RE::TESObjectWEAP* base = nullptr;

public:
    ~AVObjectWEAP() {}
    AVObjectWEAP(RE::TESObjectWEAP* base) ;
    RE::TESForm* GetBase() override { return base; }

        int32_t GetVariant(const models& models, int seed) override;
    virtual void Apply(const models& models, int32_t _variant) override;
};

class AVObjectFactory
{
    public:

    static AVObject* Create(RE::TESForm* form);
};
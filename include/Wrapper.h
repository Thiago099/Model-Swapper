#pragma once
#include "Str.h"
#include "TimeClass.h"
#include "Config.h"
struct variant {
    const char* model;
    const char* key;
    Time startDate;
    Time endDate;
};

using variants = std::vector<variant*>;
using models = std::map<std::string, variants>;

class AVObject {
public:
    virtual ~AVObject() = default;
    virtual const variant* Match(const models& models, int variant) = 0;
    virtual RE::TESForm* GetBase() = 0;
};

inline variant* find(const models& models, const char* str, const uint32_t seed) {
    const auto key = Str::processString(str);
    if (const auto it = models.find(key); it != models.end()) {
        std::mt19937 engine(seed);
        std::uniform_int_distribution<uint32_t> dist(0, it->second.size() - 1);
        if (const uint32_t random_number = dist(engine); random_number < it->second.size()) {

            const auto result = it->second.at(random_number);
            const auto config = Config::GetSingleton();

            if (config->BypassTemporalActivation) {
                return result;
            }

            auto now = config->NowOverride.exists ? config->NowOverride : Time::now();

            #ifndef NDEBUG

            now.log("now");
            result->startDate.log("start");
            result->endDate.log("end");

            #endif  // !NDEBUG
 
            if (!result->startDate.exists || !now.exists || !result->endDate.exists) {
                logger::trace("date is fault, fallback yes");
                return result;
            }
            if (now.isBetweenMD(result->startDate, result->endDate)) {
                logger::trace("is in between replacing");
                return result;
            }

            logger::trace("not in between doing nothing");
            
            return nullptr;
        }
    }
    return nullptr;
}

class AVObjectARMA final : public AVObject {
    const char* initialMaleThirdPersonModle = nullptr;
    const char* initialFemaleThirdPersonModle = nullptr;
    const char* initialMaleFirstPersonModle = nullptr;
    const char* initialFemaleFirstPersonModel = nullptr;
    RE::TESObjectARMA* base = nullptr;

public:
    ~AVObjectARMA() override {
    }

    explicit AVObjectARMA(RE::TESObjectARMA* base) : base(base) {
        if (!base) {
            return;    
        }
        if (base->bipedModels) {
            initialMaleThirdPersonModle = base->bipedModels[RE::SEXES::kMale].GetModel();
            initialFemaleThirdPersonModle = base->bipedModels[RE::SEXES::kFemale].GetModel();
        }
        if (base->bipedModel1stPersons) {
            initialMaleFirstPersonModle = base->bipedModel1stPersons[RE::SEXES::kMale].GetModel();
            initialFemaleFirstPersonModel = base->bipedModel1stPersons[RE::SEXES::kFemale].GetModel();
        }

    }
    RE::TESForm* GetBase() override {
        return base;
    }

    static int64_t RequestModel2(const char* src) {
        int64_t a2 = 0;
        int64_t a3 = 3;
        using func_t = int64_t(const char* , int64_t*, int64_t*);
        const REL::Relocation<func_t> func{RELOCATION_ID(74039, 75781)};
        return func(src, &a2, &a3);
    }

    const variant* Match(const models& models, const int _variant) override {

        const variant* result = nullptr;

        if (!base) {
			return nullptr;
        }

        if (base->bipedModels) {
            if (const auto item = find(models, initialMaleThirdPersonModle, _variant)) {
                result = item;
                base->bipedModels[RE::SEXES::kMale].SetModel(item->model);
            }
            if (const auto item = find(models, initialFemaleThirdPersonModle, _variant)) {
                result = item;
                base->bipedModels[RE::SEXES::kFemale].SetModel(item->model);
            }
        }
        if (base->bipedModel1stPersons) {

            if (const auto item = find(models, initialMaleFirstPersonModle, _variant)) {
                result = item;
                base->bipedModel1stPersons[RE::SEXES::kMale].SetModel(item->model);
            }
            if (const auto item = find(models, initialFemaleFirstPersonModel, _variant)) {
                result = item;
                base->bipedModel1stPersons[RE::SEXES::kFemale].SetModel(item->model);
            }
        }
        return result;  // for now we don't need to return anything with NPCs
    }
};


class AVModel final : public AVObject {
    const char* model = nullptr;
    RE::TESForm* base = nullptr;
public:
    ~AVModel() override {
    }

    explicit AVModel(RE::TESForm* base) : base(base) {
        if (!base) {
            return;
        }
        if (const auto bm = base->As<RE::TESModel>()) {
            model = bm->GetModel();
        }
    }

    RE::TESForm* GetBase() override { return base; }

    const variant* Match(const models& models, const int variant) override {
        if (!base) {
			return nullptr;
        }
        if (const auto bm = base->As<RE::TESModel>()) {
            if (const auto item = find(models, model, variant)) {
                bm->SetModel(item->model);
				logger::info("Applied model {}", item->model);
				return item;
            }

        }
		return nullptr;
    }
};


class AVObjectARMO : public AVObject {
    const char* male = nullptr;
    const char* female = nullptr;
    RE::TESObjectARMO* base = nullptr;

public:
    ~AVObjectARMO() {}
    AVObjectARMO(RE::TESObjectARMO* base) : base(base) {
        if (!base) {
            return;
        }

        male = base->worldModels[RE::SEXES::kMale].GetModel();
        female = base->worldModels[RE::SEXES::kFemale].GetModel();
    }

    RE::TESForm* GetBase() override { return base; }

    const variant* Match(const models& models, const int _variant) override {
        const variant* result = nullptr;
        if (!base) {
            return result;
        }
        if (auto item = find(models, male, _variant)) {
            result = item;
            base->worldModels[RE::SEXES::kMale].SetModel(item->model);
        }
        if (auto item = find(models, female, _variant)) {
            result = item;
            base->worldModels[RE::SEXES::kFemale].SetModel(item->model);
        }
        return result;
    }
};

class AVObjectWEAP : public AVObject {
    const char* firstPersonModel = nullptr;
    const char* model = nullptr;
    RE::TESObjectWEAP* base = nullptr;

public:
    ~AVObjectWEAP() {}
    AVObjectWEAP(RE::TESObjectWEAP* base) : base(base) {
        if (!base) {
            return;
        }

        firstPersonModel = base->firstPersonModelObject->GetModel();
        model = base->GetModel();
    }
    RE::TESForm* GetBase() override { return base; }


    const variant* Match(const models& models, const int _variant) override {
        const variant* result = nullptr;
        if (!base) {
            return result;
        }
        if (auto item = find(models, firstPersonModel, _variant)) {
            result = item;
            base->firstPersonModelObject->SetModel(item->model);
        }
        if (auto item = find(models, model, _variant)) {
            result = item;  
            base->SetModel(item->model);
        }
        return result;
    }
};
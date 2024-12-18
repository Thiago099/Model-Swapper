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
    virtual void Reset() = 0;
    virtual const variant* Match(models models, int variant) = 0;
    virtual RE::TESForm* GetBase() = 0;
};

inline variant* find(models& models, const char* str, const uint32_t seed) {
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

class AVObjectARMA : public AVObject {
    const char* initialMaleThirdPersonModle = nullptr;
    const char* initialFemaleThirdPersonModle = nullptr;
    const char* initialMaleFirstPersonModle = nullptr;
    const char* initialFemaleFirstPersonModel = nullptr;
    RE::TESObjectARMA* base = nullptr;

public:
    ~AVObjectARMA() {
    }
    AVObjectARMA(RE::TESObjectARMA* base) : base(base) {
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

    void Reset() override {
        if (!base) {
            return;
        }
        if (base->bipedModels) {
            base->bipedModels[RE::SEXES::kMale].SetModel(initialMaleThirdPersonModle);
            base->bipedModels[RE::SEXES::kFemale].SetModel(initialFemaleThirdPersonModle);
        }
        if (base->bipedModel1stPersons) {
            base->bipedModel1stPersons[RE::SEXES::kMale].SetModel(initialMaleFirstPersonModle);
            base->bipedModel1stPersons[RE::SEXES::kFemale].SetModel(initialFemaleFirstPersonModel);
        }
    }
    RE::TESForm* GetBase() override {
        return base;
    }
    int64_t RequestModel2(const char* src) {
        int64_t a2 = 0;
        int64_t a3 = 3;
        using func_t = int64_t(const char* , int64_t*, int64_t*);
        const REL::Relocation<func_t> func{RELOCATION_ID(74039, 75781)};
        return func(src, &a2, &a3);
    }

    const variant* Match(models models, const int variant) override {
        if (!base) {
			return nullptr;
        }

        if (base->bipedModels) {
            if (const auto item = find(models, initialMaleThirdPersonModle, variant)) {
                base->bipedModels[RE::SEXES::kMale].SetModel(item->model);
            }
            if (const auto item = find(models, initialFemaleThirdPersonModle, variant)) {
                base->bipedModels[RE::SEXES::kFemale].SetModel(item->model);
            }
        }
        if (base->bipedModel1stPersons) {

            if (const auto item = find(models, initialMaleFirstPersonModle, variant)) {
                base->bipedModel1stPersons[RE::SEXES::kMale].SetModel(item->model);
            }
            if (const auto item = find(models, initialFemaleFirstPersonModel, variant)) {
                base->bipedModel1stPersons[RE::SEXES::kFemale].SetModel(item->model);
            }
        }
		return nullptr; // for now we don't need to return anything with NPCs
    }
};


class AVModel: public AVObject {
    const char* model = nullptr;
    RE::TESForm* base = nullptr;
public:
    ~AVModel() {
    }
    AVModel(RE::TESForm* base) : base(base) {
        if (!base) {
            return;
        }
        if (const auto bm = base->As<RE::TESModel>()) {
            model = bm->GetModel();
        }
    }

    RE::TESForm* GetBase() override { return base; }

    void Reset() override {
        if (!base) {
            return;
        }
    }

    const variant* Match(models models, const int variant) override {
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

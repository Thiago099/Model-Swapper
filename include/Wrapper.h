#pragma once
#include "Str.h"
#include "TimeClass.h"
#include "Config.h"
#include "FormFilter.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
struct variant {
    const char* model;
    const char* key;
    Time startDate;
    Time endDate;
    FormFilter filter;
    double priority = 0.0;
    double weight = 1.0;
};

using variants = std::vector<variant*>;
using models = std::map<std::string, variants>;

class AVObject {
public:
    virtual ~AVObject() = default;
    virtual void Reset() = 0;
    virtual std::unique_ptr<AVObject> Snapshot() = 0;
    virtual void Match(const models& models, RE::FormID referenceID, RE::FormID baseFormID,
                       const FormFilterContext* context = nullptr) = 0;
    virtual RE::TESForm* GetBase() = 0;
};

inline variant* find(const models& models, const char* str, uint32_t seed, RE::FormID baseFormID = 0,
                     const FormFilterContext* context = nullptr) {
    variants eligible;
    double highestPriority = 0.0;
    const auto appendEligible = [&](const std::string& key) {
        auto it = models.find(key);
        if (it != models.end()) {
            for (auto item : it->second) {
                if (item->filter.Matches(baseFormID, seed, context)) {
                    if (eligible.empty() || item->priority > highestPriority) {
                        eligible.clear();
                        highestPriority = item->priority;
                    }
                    if (item->priority == highestPriority) {
                        eligible.push_back(item);
                    }
                }
            }
        }
    };
    if (str) {
        auto key = Str::processString(str);
        if (!key.empty()) {
            appendEligible(key);
        }
    }
    appendEligible("");
    std::erase_if(eligible, [](const variant* item) {
        return !std::isfinite(item->weight) || item->weight <= 0.0;
    });
    if (!eligible.empty()) {
        std::mt19937 engine(seed);
        std::size_t random_number;
        const double firstWeight = eligible.front()->weight;
        const bool uniform = std::all_of(eligible.begin(), eligible.end(), [firstWeight](const variant* item) {
            return item->weight == firstWeight;
        });
        if (uniform) {
            // Preserve the existing reference-seeded choices for uniform pools.
            std::uniform_int_distribution<uint32_t> dist(0, static_cast<uint32_t>(eligible.size() - 1));
            random_number = dist(engine);
        } else {
            double maximumWeight = firstWeight;
            for (const auto item : eligible) {
                maximumWeight = std::max(maximumWeight, item->weight);
            }
            std::vector<double> weights;
            weights.reserve(eligible.size());
            for (const auto item : eligible) {
                // Scaling prevents overflow when several finite weights are large.
                weights.push_back(item->weight / maximumWeight);
            }
            std::discrete_distribution<std::size_t> dist(weights.begin(), weights.end());
            random_number = dist(engine);
        }
        if (random_number < eligible.size()) {

            auto result = eligible.at(random_number);
            auto config = Config::GetSingleton();

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
    RE::BSFixedString initialMaleThirdPersonModle;
    RE::BSFixedString initialFemaleThirdPersonModle;
    RE::BSFixedString initialMaleFirstPersonModle;
    RE::BSFixedString initialFemaleFirstPersonModel;
    bool changedMaleThirdPerson = false;
    bool changedFemaleThirdPerson = false;
    bool changedMaleFirstPerson = false;
    bool changedFemaleFirstPerson = false;
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
            if (changedMaleThirdPerson) {
                base->bipedModels[RE::SEXES::kMale].SetModel(initialMaleThirdPersonModle.c_str());
            }
            if (changedFemaleThirdPerson) {
                base->bipedModels[RE::SEXES::kFemale].SetModel(initialFemaleThirdPersonModle.c_str());
            }
        }
        if (base->bipedModel1stPersons) {
            if (changedMaleFirstPerson) {
                base->bipedModel1stPersons[RE::SEXES::kMale].SetModel(initialMaleFirstPersonModle.c_str());
            }
            if (changedFemaleFirstPerson) {
                base->bipedModel1stPersons[RE::SEXES::kFemale].SetModel(initialFemaleFirstPersonModel.c_str());
            }
        }
    }
    RE::TESForm* GetBase() override {
        return base;
    }
    std::unique_ptr<AVObject> Snapshot() override {
        auto snapshot = std::make_unique<AVObjectARMA>(base);
        snapshot->changedMaleThirdPerson = changedMaleThirdPerson;
        snapshot->changedFemaleThirdPerson = changedFemaleThirdPerson;
        snapshot->changedMaleFirstPerson = changedMaleFirstPerson;
        snapshot->changedFemaleFirstPerson = changedFemaleFirstPerson;
        return snapshot;
    }
    int64_t RequestModel2(const char* src) {
        int64_t a2 = 0;
        int64_t a3 = 3;
        using func_t = int64_t(const char* , int64_t*, int64_t*);
        const REL::Relocation<func_t> func{RELOCATION_ID(74039, 75781)};
        return func(src, &a2, &a3);
    }

    void Match(const models& models, RE::FormID referenceID, RE::FormID baseFormID,
               const FormFilterContext* context = nullptr) override {
        if (!base) {
            return;
        }

        if (base->bipedModels) {
            if (auto item = find(models, initialMaleThirdPersonModle.c_str(), referenceID, baseFormID, context)) {
                base->bipedModels[RE::SEXES::kMale].SetModel(item->model);
                changedMaleThirdPerson = true;
            }
            if (auto item = find(models, initialFemaleThirdPersonModle.c_str(), referenceID, baseFormID, context)) {
                base->bipedModels[RE::SEXES::kFemale].SetModel(item->model);
                changedFemaleThirdPerson = true;
            }
        }
        if (base->bipedModel1stPersons) {

            if (auto item = find(models, initialMaleFirstPersonModle.c_str(), referenceID, baseFormID, context)) {
                base->bipedModel1stPersons[RE::SEXES::kMale].SetModel(item->model);
                changedMaleFirstPerson = true;
            }
            if (auto item = find(models, initialFemaleFirstPersonModel.c_str(), referenceID, baseFormID, context)) {
                base->bipedModel1stPersons[RE::SEXES::kFemale].SetModel(item->model);
                changedFemaleFirstPerson = true;
            }
        }
    }
};


class AVModel: public AVObject {
    RE::BSFixedString model;
    bool changed = false;
    RE::TESForm* base = nullptr;
public:
    ~AVModel() {
    }
    AVModel(RE::TESForm* base) : base(base) {
        if (!base) {
            return;
        }
        if (auto bm = base->As<RE::TESModel>()) {
            model = bm->GetModel();
        }
    }

    RE::TESForm* GetBase() override { return base; }
    std::unique_ptr<AVObject> Snapshot() override {
        auto snapshot = std::make_unique<AVModel>(base);
        snapshot->changed = changed;
        return snapshot;
    }

    void Reset() override {
        if (!base || !changed) {
            return;
        }
        if (auto bm = base->As<RE::TESModel>()) {
            bm->SetModel(model.c_str());
        }
    }

    void Match(const models& models, RE::FormID referenceID, RE::FormID baseFormID,
               const FormFilterContext* context = nullptr) override {
        if (!base) {
            return;
        }
        if (auto bm = base->As<RE::TESModel>()) {
            if (auto item = find(models, model.c_str(), referenceID, baseFormID, context)) {
                bm->SetModel(item->model);
                changed = true;
            }

        }
    }
};

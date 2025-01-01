#include "Application/AVObjects.h"
#include "Lib/Str.h"
#include "Lib/TimeClass.h"
#include "Application/Config.h"
#include "Application/Variants.h"

#define RETURN_IF_NOT_NEGATIVE(x) if (x != -1) return x;

AVObjectARMA::AVObjectARMA(RE::TESObjectARMA* base) : base(base) {
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

int32_t AVObjectARMA::GetVariant(const models& models, int seed) {
    if (!base) {
        return -1;
    }

    int32_t result = -1;

    if (base->bipedModels) {

        result = Variants::pickVariant(models, initialMaleThirdPersonModle, seed);

        RETURN_IF_NOT_NEGATIVE(result);

        result = Variants::pickVariant(models, initialFemaleThirdPersonModle, seed);

        RETURN_IF_NOT_NEGATIVE(result);

    }
    if (base->bipedModel1stPersons) {

        result = Variants::pickVariant(models, initialMaleFirstPersonModle, seed);

        RETURN_IF_NOT_NEGATIVE(result);

        result = Variants::pickVariant(models, initialFemaleFirstPersonModel, seed);

        RETURN_IF_NOT_NEGATIVE(result);
    }
    return result;
}

void AVObjectARMA::Apply(const models& models, int32_t _variant)  {
    if (!base) {
        return;
    }

    if (base->bipedModels) {
        if (const auto item = Variants::getVariant(models, initialMaleThirdPersonModle, _variant)) {
            base->bipedModels[RE::SEXES::kMale].SetModel(item->model);
        }
        if (const auto item = Variants::getVariant(models, initialFemaleThirdPersonModle, _variant)) {
            base->bipedModels[RE::SEXES::kFemale].SetModel(item->model);
        }
    }
    if (base->bipedModel1stPersons) {
        if (const auto item = Variants::getVariant(models, initialMaleFirstPersonModle, _variant)) {
            base->bipedModel1stPersons[RE::SEXES::kMale].SetModel(item->model);
        }
        if (const auto item = Variants::getVariant(models, initialFemaleFirstPersonModel, _variant)) {
            base->bipedModel1stPersons[RE::SEXES::kFemale].SetModel(item->model);
        }
    }
}

AVModel::AVModel(RE::TESForm* base) : base(base) {
    if (!base) {
        return;
    }
    if (const auto bm = base->As<RE::TESModel>()) {
        model = bm->GetModel();
    }
}

int32_t AVModel::GetVariant(const models& models, int seed) {
    if (!base) {
        return -1;
    }
    int32_t result = -1;
    if (const auto bm = base->As<RE::TESModel>()) {

        result = Variants::pickVariant(models, model, seed);

        RETURN_IF_NOT_NEGATIVE(result);
    }

    return result;
}

void AVModel::Apply(const models& models, int32_t _variant) {
    if (!base) {
        return;
    }

    if (const auto bm = base->As<RE::TESModel>()) {
        if (const auto item = Variants::getVariant(models, model, _variant)) {
            bm->SetModel(item->model);
        }
    }
}

AVObjectARMO::AVObjectARMO(RE::TESObjectARMO* base) : base(base) {
    if (!base) {
        return;
    }

    male = base->worldModels[RE::SEXES::kMale].GetModel();
    female = base->worldModels[RE::SEXES::kFemale].GetModel();
}

int32_t AVObjectARMO::GetVariant(const models& models, int seed) {
    if (!base) {
        return -1;
    }
    int32_t result = -1;

    result = Variants::pickVariant(models, male, seed);

    RETURN_IF_NOT_NEGATIVE(result);

    result = Variants::pickVariant(models, female, seed);

    RETURN_IF_NOT_NEGATIVE(result);

    return -1;
}

void AVObjectARMO::Apply(const models& models, int32_t _variant) {
    if (!base) {
        return;
    }

    if (auto item = Variants::getVariant(models, male, _variant)) {
        base->worldModels[RE::SEXES::kMale].SetModel(item->model);
    }
    if (auto item = Variants::getVariant(models, female, _variant)) {
        base->worldModels[RE::SEXES::kFemale].SetModel(item->model);
    }
}

AVObjectWEAP::AVObjectWEAP(RE::TESObjectWEAP* base) : base(base) {
    if (!base) {
        return;
    }

    firstPersonModel = base->firstPersonModelObject->GetModel();
    model = base->GetModel();
}

int32_t AVObjectWEAP::GetVariant(const models& models, int seed) {
    if (!base) {
        return -1;
    }
    int32_t result = -1;

    result = Variants::pickVariant(models, firstPersonModel, seed);

    RETURN_IF_NOT_NEGATIVE(result);

    result = Variants::pickVariant(models, model, seed);

    RETURN_IF_NOT_NEGATIVE(result);

    return result;
}

void AVObjectWEAP::Apply(const models& models, int32_t _variant) {
    if (!base) {
        return;
    }

    if (auto item = Variants::getVariant(models, firstPersonModel, _variant)) {
        base->firstPersonModelObject->SetModel(item->model);
    }
    if (auto item = Variants::getVariant(models, model, _variant)) {
        base->SetModel(item->model);
    }
}

AVObject* AVObjectFactory::Create(RE::TESForm* form) {
    if (auto obj = form->As<RE::TESObjectARMO>()) {
        return new AVObjectARMO(obj);
    } else if (auto obj = form->As<RE::TESObjectARMA>()) {
        return new AVObjectARMA(obj);
    } else if (auto obj = form->As<RE::TESObjectWEAP>()) {
        return new AVObjectWEAP(obj);
    } else if (auto obj = form->As<RE::TESModel>()) {
        return new AVModel(form);
    }
    return nullptr;
}

#include "Application/AVObjects.h"
#include "Lib/Str.h"
#include "Lib/TimeClass.h"
#include "Application/Config.h"
#include "Application/Variants.h"

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

    if (base->bipedModels) {
        if (const auto item = Variants::pickVariant(models, initialMaleThirdPersonModle, seed); item != -1) {
            return item;
        }
        if (const auto item = Variants::pickVariant(models, initialFemaleThirdPersonModle, seed); item != -1) {
            return item;
        }
    }
    if (base->bipedModel1stPersons) {
        if (const auto item = Variants::pickVariant(models, initialMaleFirstPersonModle, seed); item != -1) {
            return item;
        }
        if (const auto item = Variants::pickVariant(models, initialFemaleFirstPersonModel, seed); item != -1) {
            return item;
        }
    }
    return -1;
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

    if (const auto bm = base->As<RE::TESModel>()) {
        if (const auto item = Variants::pickVariant(models, model, seed)) {
            return item;
        }
    }
    return -1;
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

    if (auto item = Variants::pickVariant(models, male, seed)) {
        return item;
    }
    if (auto item = Variants::pickVariant(models, female, seed)) {
        return item;
    }

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

    if (auto item = Variants::pickVariant(models, firstPersonModel, seed)) {
        return item;
    }
    if (auto item = Variants::pickVariant(models, model, seed)) {
        return item;
    }

    return -1;
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

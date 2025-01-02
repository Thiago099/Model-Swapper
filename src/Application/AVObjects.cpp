#include "Application/AVObjects.h"
#include "Lib/Str.h"
#include "Lib/TimeClass.h"
#include "Application/Config.h"
#include "Application/ModelSwap.h"

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

int32_t AVObjectARMA::GetVariant(int seed) {
    if (!base) {
        return -1;
    }

    int32_t result = -1;

    if (base->bipedModels) {

        result = ModelSwap::GetSingleton()
            ->PickVariant(initialMaleThirdPersonModle, seed);

        RETURN_IF_NOT_NEGATIVE(result);

        result = ModelSwap::GetSingleton()
            ->PickVariant(initialFemaleThirdPersonModle, seed);

        RETURN_IF_NOT_NEGATIVE(result);

    }
    if (base->bipedModel1stPersons) {

        result = ModelSwap::GetSingleton()
            ->PickVariant(initialMaleFirstPersonModle, seed);

        RETURN_IF_NOT_NEGATIVE(result);

        result = ModelSwap::GetSingleton()
            ->PickVariant(initialFemaleFirstPersonModel, seed);

        RETURN_IF_NOT_NEGATIVE(result);
    }
    return result;
}

void AVObjectARMA::Apply(int32_t _variant)  {
    if (!base) {
        return;
    }

    if (base->bipedModels) {
        if (const auto item =
                ModelSwap::GetSingleton()
            ->GetVariant(initialMaleThirdPersonModle, _variant)) {
            base->bipedModels[RE::SEXES::kMale].SetModel(item->model);
        }
        if (const auto item =
                ModelSwap::GetSingleton()
            ->GetVariant(initialFemaleThirdPersonModle, _variant)) {
            base->bipedModels[RE::SEXES::kFemale].SetModel(item->model);
        }
    }
    if (base->bipedModel1stPersons) {
        if (const auto item =
            ModelSwap::GetSingleton()
            ->GetVariant(initialMaleFirstPersonModle, _variant)) {
            base->bipedModel1stPersons[RE::SEXES::kMale].SetModel(item->model);
        }
        if (const auto item =
            ModelSwap::GetSingleton()
            ->GetVariant(initialFemaleFirstPersonModel, _variant)) {
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

int32_t AVModel::GetVariant(int seed) {
    if (!base) {
        return -1;
    }
    int32_t result = -1;
    if (const auto bm = base->As<RE::TESModel>()) {

        result = ModelSwap::GetSingleton()
            ->PickVariant(model, seed);

        RETURN_IF_NOT_NEGATIVE(result);
    }

    return result;
}

void AVModel::Apply(int32_t _variant) {
    if (!base) {
        return;
    }

    if (const auto bm = base->As<RE::TESModel>()) {
        if (const auto item = ModelSwap::GetSingleton()
            ->GetVariant(model, _variant)) {
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

int32_t AVObjectARMO::GetVariant(int seed) {
    if (!base) {
        return -1;
    }
    int32_t result = -1;

    result = ModelSwap::GetSingleton()
        ->PickVariant(male, seed);

    RETURN_IF_NOT_NEGATIVE(result);

    result = ModelSwap::GetSingleton()
        ->PickVariant(female, seed);

    RETURN_IF_NOT_NEGATIVE(result);

    return -1;
}

void AVObjectARMO::Apply(int32_t _variant) {
    if (!base) {
        return;
    }

    if (auto item = ModelSwap::GetSingleton()
        ->GetVariant(male, _variant)) {
        base->worldModels[RE::SEXES::kMale].SetModel(item->model);
    }
    if (auto item = ModelSwap::GetSingleton()
        ->GetVariant(female, _variant)) {
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

int32_t AVObjectWEAP::GetVariant(int seed) {
    if (!base) {
        return -1;
    }
    int32_t result = -1;

    result = ModelSwap::GetSingleton()
        ->PickVariant(firstPersonModel, seed);

    RETURN_IF_NOT_NEGATIVE(result);

    result = ModelSwap::GetSingleton()
        ->PickVariant(model, seed);

    RETURN_IF_NOT_NEGATIVE(result);

    return result;
}

void AVObjectWEAP::Apply(int32_t _variant) {
    if (!base) {
        return;
    }

    if (auto item = ModelSwap::GetSingleton()
        ->GetVariant(firstPersonModel, _variant)) {
        base->firstPersonModelObject->SetModel(item->model);
    }
    if (auto item = ModelSwap::GetSingleton()
        ->GetVariant(model, _variant)) {
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

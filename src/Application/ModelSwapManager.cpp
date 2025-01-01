#include "Application/ModelSwapManager.h"
#include "Adaptors/Serialization.h"
#include "Application/AVObjects.h"

void ModelSwapManager::Apply(RE::TESForm* base, variantId variant) {

    if (variant == -1) {
		return;
	}

    if (AVObject* wrapper = AVObjectFactory::Create(base)) {
        wrapper->Apply(sources, variant);
        delete wrapper;
    }
}
const int32_t ModelSwapManager::Process(RE::TESForm* base, const RefID id) {
    int32_t result = -1;

    if (AVObject* wrapper = AVObjectFactory::Create(base)) {
        result = wrapper->GetVariant(sources, id);
        delete wrapper;
    }

    return result;
}

void ModelSwapManager::Register(std::string key, variants value) {
    key = Str::processString(key);

    if (const auto it = sources.find(key); it != sources.end()) {
        it->second.insert(it->second.end(), value.begin(), value.end());
    } else {
        sources[key] = value;
    }

    for (const auto item : value) {
        sources[item->key] = value;
    }
}

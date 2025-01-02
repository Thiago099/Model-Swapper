#include "Application/ModelSwapManager.h"
#include "Adaptors/Serialization.h"
#include "Application/AVObjects.h"

void ModelSwapManager::Apply(RE::TESForm* base, variantId variant) {

    if (variant == -1) {
		return;
	}

    if (AVObject* wrapper = AVObjectFactory::Create(base)) {
        wrapper->Apply(variant);
        delete wrapper;
    }
}
const int32_t ModelSwapManager::Process(RE::TESForm* base, const RefID id) {
    int32_t result = -1;

    if (AVObject* wrapper = AVObjectFactory::Create(base)) {
        result = wrapper->GetVariant(id);
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

int32_t ModelSwapManager::PickVariant(const char* str, const uint32_t seed) {
    const auto key = Str::processString(str);
    if (const auto it = sources.find(key); it != sources.end()) {
        std::mt19937 engine(seed);
        std::uniform_int_distribution<uint32_t> dist(0, it->second.size() - 1);
        if (const uint32_t random_number = dist(engine); random_number < it->second.size()) {
            const auto result = it->second.at(random_number);
            const auto config = Config::GetSingleton();

            if (config->BypassTemporalActivation) {
                return random_number;
            }

            auto now = config->NowOverride.exists ? config->NowOverride : Time::now();

#ifndef NDEBUG

            now.log("now");
            result->startDate.log("start");
            result->endDate.log("end");

#endif

            if (!result->startDate.exists || !now.exists || !result->endDate.exists) {
                logger::trace("date is fault, fallback yes {}", random_number);
                return random_number;
            }
            if (now.isBetweenMD(result->startDate, result->endDate)) {
                logger::trace("is in between replacing {}", random_number);
                return random_number;
            }

            logger::trace("not in between doing nothing would be {}", random_number);
        }
    }
    return -1;
}

variant* ModelSwapManager::GetVariant(const char* str, const uint32_t variant) {
    const auto key = Str::processString(str);
    if (const auto it = sources.find(key); it != sources.end()) {
        if (variant < it->second.size()) {
            return it->second[variant];
        }
        return nullptr;
    }
    return nullptr;
}

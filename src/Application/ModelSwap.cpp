#include "Application/ModelSwap.h"
#include "Adaptors/Serialization.h"
#include "Application/AVObjects.h"



variantId ModelSwap::PickRandomVariant(variants& source, const uint32_t seed) { 
    std::mt19937 engine(seed);
    std::uniform_int_distribution<uint32_t> dist(0, source.size() - 1);
    return dist(engine);
}

bool ModelSwap::DoesTemporalOverrideStopTheReplacement(variant* item) {
    const auto config = Config::GetSingleton();

    if (config->BypassTemporalActivation) {
        return false;
    }

    auto now = config->NowOverride.exists ? config->NowOverride : Time::now();

    #ifndef NDEBUG

        now.log("now");
        item->startDate.log("start");
        item->endDate.log("end");

    #endif

    if (!item->startDate.exists || !now.exists || !item->endDate.exists) {
        return false;
    }

    if (now.isBetweenMD(item->startDate, item->endDate)) {
        return false;
    }

    return true;
}

void ModelSwap::Apply(RE::TESForm* base, variantId variant) {

    if (variant == -1) {
		return;
	}

    if (AVObject* wrapper = AVObjectFactory::Create(base)) {
        wrapper->Apply(variant);
        delete wrapper;
    }
}
const int32_t ModelSwap::Process(RE::TESForm* base, const RefID id) {
    int32_t result = -1;

    if (AVObject* wrapper = AVObjectFactory::Create(base)) {
        result = wrapper->GetVariant(id);
        delete wrapper;
    }

    return result;
}

void ModelSwap::Register(std::string key, variants value) {
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

int32_t ModelSwap::PickVariant(const char* str, const uint32_t seed) {
    const auto key = Str::processString(str);
    if (const auto it = sources.find(key); it != sources.end()) {

        auto random_number = PickRandomVariant(it->second, seed);

        const auto result = it->second.at(random_number);

        const auto config = Config::GetSingleton();

        if (DoesTemporalOverrideStopTheReplacement(result)) {
			logger::trace("Replacement by variant {} stopped by temporal override", random_number);
			return -1;
		}

        return random_number;

        logger::trace("not in between doing nothing would be {}", random_number);
    }
    return -1;
}

variant* ModelSwap::GetVariant(const char* str, const uint32_t variant) {
    const auto key = Str::processString(str);
    if (const auto it = sources.find(key); it != sources.end()) {
        if (variant < it->second.size()) {
            return it->second[variant];
        }
        return nullptr;
    }
    return nullptr;
}

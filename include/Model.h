
#pragma once

#include "Config.h"
#include "Str.h"
struct variant {
    const char* model;
    const char* key;
    Time startDate;
    Time endDate;
};
using variantId = int32_t;
using v_variant = std::vector<variantId>;
using inventory_stack = std::map<FormID, v_variant>;
using variants = std::vector<variant*>;
using models = std::map<std::string, variants>;


namespace Variants {
    inline int32_t pickVariant(const models& models, const char* str, const uint32_t seed) {
        const auto key = Str::processString(str);
        if (const auto it = models.find(key); it != models.end()) {
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

#endif  // !NDEBUG

                if (!result->startDate.exists || !now.exists || !result->endDate.exists) {
                    logger::trace("date is fault, fallback yes");
                    return random_number;
                }
                if (now.isBetweenMD(result->startDate, result->endDate)) {
                    logger::trace("is in between replacing");
                    return random_number;
                }

                logger::trace("not in between doing nothing");

                return random_number;
            }
        }
        return -1;
    }

    inline variant* getVariant(const models& models, const char* str, const uint32_t variant) {
        const auto key = Str::processString(str);
        if (const auto it = models.find(key); it != models.end()) {
            if (variant < it->second.size()) {
                return it->second[variant];
            }
            return nullptr;
        }
        return nullptr;
    }
}

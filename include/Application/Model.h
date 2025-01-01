
#pragma once

#include "Application/Config.h"
#include "Lib/Str.h"

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

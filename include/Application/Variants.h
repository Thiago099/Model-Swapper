#pragma once

#include "Application/Model.h"

namespace Variants {
    int32_t pickVariant(const models& models, const char* str, const uint32_t seed);
    variant* getVariant(const models& models, const char* str, const uint32_t variant);

}


#pragma once

#include "FormFilter.h"

namespace RE {
    class TESObjectREFR;
}

namespace FormFilters {
    FormFilterContext CaptureContext(const RE::TESObjectREFR* reference);
    bool ValidateContextIDs(const FormFilter& filter, std::string_view& invalidField);
}

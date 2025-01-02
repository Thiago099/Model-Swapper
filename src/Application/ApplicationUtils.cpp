#include "Application/ApplicationUtils.h"

v_variant ApplicationUtils::GetNItems(v_variant& stack, const int32_t count) {
    v_variant result;
    if (static_cast<uint32_t>(stack.size()) >= count) {
        result.insert(result.end(), stack.end() - count, stack.end());
    } else {
        result.insert(result.end(), stack.begin(), stack.end());
    }
    return result;
}
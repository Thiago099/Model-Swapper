#include "Application/ApplicationUtils.h"

v_variant ApplicationUtils::GetTopOfStack(v_variant& stack, const int32_t count) {
    v_variant result;
    if (static_cast<uint32_t>(stack.size()) >= count) {
        // Copy the last a_count elements
        result.insert(result.end(), stack.end() - count, stack.end());
    } else {
        // Add nullptrs to the beginning if count is larger than the stack size
        result.insert(result.end(), count - stack.size(), -1);
        result.insert(result.end(), stack.begin(), stack.end());
    }
    return result;
}
#include "Application/DropQueue.h"

void DropQueue::ClearData() {
    std::unique_lock lock_queue(queue_mutex_);
    variants_queue.clear();
}

void DropQueue::Add(FormID formid, v_variant& variant_vector) {
    std::unique_lock lock(queue_mutex_);
    const auto pair = std::make_pair(formid, variant_vector);
    variants_queue.push_back(pair);
}

v_variant DropQueue::Get(const FormID formId) {
    std::unique_lock lock(queue_mutex_);

    if (variants_queue.empty()) {
        return {};
    }
    for (auto it = variants_queue.begin(); it != variants_queue.end(); ++it) {
        if (it->first == formId) {
            auto result = it->second;
            variants_queue.erase(it);
            return result;
        }
    }
    return {};
}

std::vector<std::pair<FormID, v_variant>> DropQueue::GetAll() { return variants_queue; }
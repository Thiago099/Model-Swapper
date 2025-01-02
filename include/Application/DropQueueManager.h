#pragma once
#include "Lib/Singleton.h"
#include <shared_mutex>
#include "Model.h"

class DropQueueManager : public Singleton<DropQueueManager> {
    std::shared_mutex queue_mutex_;
    std::vector<std::pair<FormID, v_variant>> variants_queue;

public:
    void ClearData();

    void Add(FormID formid, v_variant& variant_vector);

    v_variant Get(const FormID formId);

    std::vector<std::pair<FormID, v_variant>> GetAll();
};
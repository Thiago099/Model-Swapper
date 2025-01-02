#pragma once
#include <shared_mutex>
#include "Application/Model.h"
#include "Lib/Singleton.h"

class WorldStackManager : public Singleton<WorldStackManager> {
    std::shared_mutex applied_variants_mutex_;
    std::map<RefID, v_variant> world_object_stacks;

public:
    v_variant& GetByReference(const RefID refid);

    void Remove(FormID baseId, RefID obj_refid);
    void Add(RefID id, int32_t item);
    void Set(RefID id, v_variant items);
    void CleanData();

    std::shared_mutex& GetMutex();
    std::map<RefID, v_variant> GetAll();
};
#include "Application/WorldStack.h"

v_variant& WorldStack::GetByReference(const RefID refid) { return world_object_stacks[refid]; }

void WorldStack::Remove(FormID baseId, RefID obj_refid) {
    if (std::unique_lock lock(applied_variants_mutex_); world_object_stacks.contains(baseId)) {
        world_object_stacks.erase(obj_refid);
    }
}

void WorldStack::Add(RefID id, int32_t item) { 
    world_object_stacks[id].push_back(item); 
}

void WorldStack::Set(RefID id, v_variant items) { 
    world_object_stacks[id] = items; 
}

void WorldStack::CleanData() {
    std::unique_lock lock_var(applied_variants_mutex_);
    world_object_stacks.clear();
}

std::shared_mutex& WorldStack::GetMutex() { return applied_variants_mutex_; }

std::map<RefID, v_variant> WorldStack::GetAll() { return world_object_stacks; }

#include "Application/WorldStackManager.h"

v_variant& WorldStackManager::GetByReference(const RefID refid) { return world_object_stacks[refid]; }

void WorldStackManager::Remove(FormID baseId, RefID obj_refid) {
    if (std::unique_lock lock(applied_variants_mutex_); world_object_stacks.contains(baseId)) {
        world_object_stacks.erase(obj_refid);
    }
}

void WorldStackManager::Add(RefID id, int32_t item) { 
    world_object_stacks[id].push_back(item); 
}

void WorldStackManager::Set(RefID id, v_variant items) { 
    world_object_stacks[id] = items; 
}

void WorldStackManager::Clean() {
    std::unique_lock lock_var(applied_variants_mutex_);
    world_object_stacks.clear();
}

std::shared_mutex& WorldStackManager::GetMutex() { return applied_variants_mutex_; }

std::map<RefID, v_variant> WorldStackManager::GetWorldObjectStacks() { return world_object_stacks; }

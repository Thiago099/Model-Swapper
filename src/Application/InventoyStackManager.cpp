#include "Application/InventoyStackManager.h"
#include "Application/ApplicationUtils.h"
#include "Application/ModelSwapManager.h"

void InventoyStackManager::ClearData() {
    std::unique_lock lock_inv(inventory_stacks_mutex_);

    inventory_stacks.clear();
}

void InventoyStackManager::Sync(RE::TESObjectREFR* inventory_owner) {
    std::map<FormID, int32_t> actual_inventory;
    const auto inv = inventory_owner->GetInventory();
    for (const auto& [bound, entry] : inv) {
        actual_inventory[bound->GetFormID()] = entry.first;
    }
    std::unique_lock lock(inventory_stacks_mutex_);
    // if we have less->add nullptr, if we have more->remove
    for (const auto& [item, stack] : inventory_stacks[inventory_owner->GetFormID()]) {
        const auto actual_count = actual_inventory.contains(item) ? actual_inventory[item] : 0;
        const auto stack_count = static_cast<int32_t>(stack.size());
        if (const auto bound = RE::TESForm::LookupByID<RE::TESBoundObject>(item); !bound) {
            logger::warn("Bound object not found: {:x}", item);
            continue;
        }
        if (actual_count > stack_count) {
            auto diff = actual_count - stack_count;
            while (diff > 0) {
                Add(inventory_owner->GetFormID(), item, -1);
                --diff;
            }
        } else if (actual_count < stack_count) {
            auto diff = stack_count - actual_count;
            while (diff > 0) {
                Remove(inventory_owner->GetFormID(), item);
                --diff;
            }
        }
    }
}

void InventoyStackManager::Remove(const RefID owner_id, const FormID item_id) {
    if (!inventory_stacks[owner_id][item_id].empty()) {
        inventory_stacks[owner_id][item_id].pop_back();
    }
}

std::map<RefID, inventory_stack> InventoyStackManager::GetAll() { return inventory_stacks; }

std::shared_mutex& InventoyStackManager::GetMutex() { return inventory_stacks_mutex_; }

void InventoyStackManager::Add(RefID owner, RefID item, int a_variant) {
    if (a_variant != -1) {
        inventory_stacks[owner][item].push_back(a_variant);
    }
}

void InventoyStackManager::AddMultiple(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count,
                               v_variant& add_vector) {
    std::unique_lock lock(inventory_stacks_mutex_);
    for (int i = 0; i < add_vector.size(); ++i) {
        logger::trace("Add I: {}", add_vector[i]);
        Add(a_owner->GetFormID(), a_obj->GetFormID(), add_vector[i]);
    }
}

void InventoyStackManager::Remove(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count) {
    Sync(a_owner);
    std::unique_lock lock(inventory_stacks_mutex_);
    for (int i = 0; i < a_count; ++i) {
        Remove(a_owner->GetFormID(), a_obj->GetFormID());
    }
}

const int32_t InventoyStackManager::GetInventoryModel(const RE::TESObjectREFR* a_owner,
                                                      const RE::TESBoundObject* a_item) {
    std::shared_lock lock(inventory_stacks_mutex_);
    if (const auto it = inventory_stacks.find(a_owner->GetFormID()); it != inventory_stacks.end()) {
        if (const auto it2 = it->second.find(a_item->GetFormID()); it2 != it->second.end()) {
            if (!it2->second.empty()) {
                return it2->second.back();
            }
        }
    }
    return -1;
}

std::vector<int32_t> InventoyStackManager::GetAllInventoryModels(const RE::TESObjectREFR* a_owner,
                                                                 const RE::TESBoundObject* a_item,
                                                                 const int32_t a_count) {
    std::shared_lock lock(inventory_stacks_mutex_);
    if (const auto it = inventory_stacks.find(a_owner->GetFormID()); it != inventory_stacks.end()) {
        if (const auto it2 = it->second.find(a_item->GetFormID()); it2 != it->second.end()) {
            if (!it2->second.empty()) {
                // need to collect from the back of the vector <-> top of the stack
                return ApplicationUtils::GetTopOfStack(it2->second, a_count);
            }
        }
    }
    return {};
}

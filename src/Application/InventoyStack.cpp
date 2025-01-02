#include "Application/InventoyStack.h"
#include "Application/ApplicationUtils.h"
#include "Application/ModelSwap.h"

void InventoyStack::ClearData() {
    std::unique_lock lock_inv(inventory_stacks_mutex_);

    inventory_stacks.clear();
}


std::map<RefID, inventory_stack> InventoyStack::GetAll() { return inventory_stacks; }

std::shared_mutex& InventoyStack::GetMutex() { return inventory_stacks_mutex_; }

void InventoyStack::Add(RefID owner, RefID item, int a_variant) {
    if (a_variant != -1) {
        inventory_stacks[owner][item].push_back(a_variant);
    }
}

void InventoyStack::AddMultiple(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj,
                               v_variant& add_vector) {
    std::unique_lock lock(inventory_stacks_mutex_);
    for (int i = 0; i < add_vector.size(); ++i) {
        logger::trace("Add I: {}", add_vector[i]);
        Add(a_owner->GetFormID(), a_obj->GetFormID(), add_vector[i]);
    }
}

void InventoyStack::Remove(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count) {
    std::unique_lock lock(inventory_stacks_mutex_);
    for (int i = 0; i < a_count; ++i) {
        auto owner_id = a_owner->GetFormID();
        auto item_id = a_obj->GetFormID();
        if (!inventory_stacks[owner_id][item_id].empty()) {
            inventory_stacks[owner_id][item_id].pop_back();
        }
    }
}

const int32_t InventoyStack::GetInventoryModel(const RE::TESObjectREFR* a_owner,
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

std::vector<int32_t> InventoyStack::GetItemsByContainerAndBase(const RE::TESObjectREFR* a_owner,
                                                                 const RE::TESBoundObject* a_item) {
    std::shared_lock lock(inventory_stacks_mutex_);
    if (const auto it = inventory_stacks.find(a_owner->GetFormID()); it != inventory_stacks.end()) {
        if (const auto it2 = it->second.find(a_item->GetFormID()); it2 != it->second.end()) {
            if (!it2->second.empty()) {
                return it2->second;
            }
        }
    }
    return {};
}

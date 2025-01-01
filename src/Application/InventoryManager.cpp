#include "Application/InventoryManager.h"
#include "Application/ModelSwapManager.h"

void InventoryManager::ClearData() {
    std::unique_lock lock_inv(inventory_stacks_mutex_);
    std::unique_lock lock_queue(queue_mutex_);

    inventory_stacks.clear();
    variants_queue.clear();
}




void InventoryManager::SyncInventory(RE::TESObjectREFR* inventory_owner) {
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
                AddToStack(inventory_owner->GetFormID(), item, -1);
                --diff;
            }
        } else if (actual_count < stack_count) {
            auto diff = stack_count - actual_count;
            while (diff > 0) {
                RemoveFromStack(inventory_owner->GetFormID(), item);
                --diff;
            }
        }
    }
}


void InventoryManager::AddToStack(const RefID owner_id, const FormID item_id, variantId a_variant) {
    if (a_variant != -1) {
        inventory_stacks[owner_id][item_id].push_back(a_variant);
    }
}

void InventoryManager::RemoveFromStack(const RefID owner_id, const FormID item_id) {
    if (!inventory_stacks[owner_id][item_id].empty()) {
        inventory_stacks[owner_id][item_id].pop_back();
    }
}




v_variant InventoryManager::GetTopOfStack(v_variant& stack, const int32_t count) {
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



std::map<RefID, inventory_stack> InventoryManager::GetInventoryStacks() { return inventory_stacks; }

std::vector<std::pair<FormID, v_variant>> InventoryManager::GetVariantsQueue() {
    return variants_queue; }


std::shared_mutex& InventoryManager::GetInventoryMutex() {
    return inventory_stacks_mutex_; }
void InventoryManager::Add(RefID owner, RefID item, int model) {
    inventory_stacks[owner][item].push_back(model);
}
void InventoryManager::UpdateStackOnAdd(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj,
                                        const int32_t a_count,
                               v_variant& add_vector) {
    std::unique_lock lock(inventory_stacks_mutex_);
    for (int i = 0; i < add_vector.size(); ++i) {
        logger::trace("Add I: {}", add_vector[i]);
        AddToStack(a_owner->GetFormID(), a_obj->GetFormID(), add_vector[i]);
    }
}

void InventoryManager::UpdateStackOnRemove(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj,
                                           const int32_t a_count) {
    SyncInventory(a_owner);
    std::unique_lock lock(inventory_stacks_mutex_);
    for (int i = 0; i < a_count; ++i) {
        RemoveFromStack(a_owner->GetFormID(), a_obj->GetFormID());
    }
}

const int32_t InventoryManager::GetInventoryModel(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_item) {
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

std::vector<int32_t> InventoryManager::GetInventoryModels(const RE::TESObjectREFR* a_owner,
                                                          const RE::TESBoundObject* a_item,
                                                 const int32_t a_count) {
    std::shared_lock lock(inventory_stacks_mutex_);
    if (const auto it = inventory_stacks.find(a_owner->GetFormID()); it != inventory_stacks.end()) {
        if (const auto it2 = it->second.find(a_item->GetFormID()); it2 != it->second.end()) {
            if (!it2->second.empty()) {
                // need to collect from the back of the vector <-> top of the stack
                return GetTopOfStack(it2->second, a_count);
            }
        }
    }
    return {};
}


void InventoryManager::AddToQueue(FormID formid, v_variant& variant_vector) {
    std::unique_lock lock(queue_mutex_);
    const auto pair = std::make_pair(formid, variant_vector);
    variants_queue.push_back(pair);
}

v_variant InventoryManager::FetchFromQueue(const FormID formId) {
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


void InventoryManager::OnItemDrop(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count) {
    if (auto variants = GetInventoryModels(a_owner, a_obj, a_count); !variants.empty()) {
        AddToQueue(a_obj->GetFormID(), variants);
    }
    UpdateStackOnRemove(a_owner, a_obj, a_count);
}

bool InventoryManager::GetItemFromQueue(RE::TESObjectREFR* a_ref) {
    //TODO: REFACTOR THIS
    const auto refid = a_ref->GetFormID();
    const auto base = a_ref->GetBaseObject();
    auto ref_count = a_ref->extraList.GetCount();
    ref_count = ref_count > 0 ? ref_count : 1;

    auto modelSwap = ModelSwapManager::GetSingleton();


   return false;
}



void InventoryManager::OnItemTransfer(RE::TESObjectREFR* a_this, const RE::TESBoundObject* a_item,
                                const int32_t a_count, RE::TESObjectREFR* a_other) {
    auto inv_variants = GetInventoryModels(a_this, a_item, a_count);
    UpdateStackOnRemove(a_this, a_item, a_count);
    UpdateStackOnAdd(a_other, a_item, a_count, inv_variants);
}

void InventoryManager::OnItemDrop(RE::ITEM_REMOVE_REASON a_reason, RE::TESObjectREFR* a_this,
                                const RE::TESBoundObject* a_item, const int32_t a_count) {
    if (a_reason == RE::ITEM_REMOVE_REASON::kDropping) {
        OnItemDrop(a_this, a_item, a_count);
    } else {
        UpdateStackOnRemove(a_this, a_item, a_count);
    }
}


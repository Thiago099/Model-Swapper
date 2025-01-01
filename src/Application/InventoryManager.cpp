#include "Application/InventoryManager.h"
#include "Application/ModelSwapManager.h"

void InventoryManager::ClearData() {
    std::unique_lock lock_inv(inventory_stacks_mutex_);
    std::unique_lock lock_var(applied_variants_mutex_);
    std::unique_lock lock_queue(queue_mutex_);

    inventory_stacks.clear();
    variants_queue.clear();
}


void InventoryManager::LoadSerializedData(const char* filename) {
    logger::info("Loading data from {}", filename);

    ClearData();

    Serialization::Data saved_data;
    Serialization::loadDataBinary(saved_data, filename);

    std::unique_lock lock_inv(inventory_stacks_mutex_);
    std::unique_lock lock_var(applied_variants_mutex_);

    for (const auto& [owner_refid, item_map] : saved_data.inventory) {
        for (const auto& [item_refid, model_indices] : item_map) {
            for (const auto model_index : model_indices) {
                inventory_stacks[owner_refid][item_refid].push_back(model_index);
            }
        }
    }

    for (const auto& [owner_refid, model_indices] : saved_data.worldobject) {
        for (const auto model_index : model_indices) {
            world_object_stacks[owner_refid].push_back(model_index);
        }
    }
}

void InventoryManager::SerializeData(const char* filename) {
    const auto file_path = Serialization::serialization_path + filename;

    std::unique_lock lock_inv(inventory_stacks_mutex_);
    std::unique_lock lock_var(applied_variants_mutex_);

    const Serialization::Data data(inventory_stacks, world_object_stacks);
    Serialization::saveDataBinary(data, file_path);
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


v_variant& InventoryManager::GetWOStack(const RefID refid) { return world_object_stacks[refid]; }

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

void InventoryManager::AddToOWStack(RefID id, int32_t item) {
    world_object_stacks[id].push_back(item);
}

void InventoryManager::SetOWStack(RefID id, v_variant items) {
    world_object_stacks[id] = items;
}

std::map<RefID, inventory_stack> InventoryManager::GetInventoryStacks() { return inventory_stacks; }

std::map<RefID, v_variant> InventoryManager::GetWorldObjectStacks() { return world_object_stacks; }

std::vector<std::pair<FormID, v_variant>> InventoryManager::GetVariantsQueue() {
    return variants_queue; }

void InventoryManager::OnItemPickup(RE::TESObjectREFR* a_owner, RE::TESObjectREFR* a_obj, const int32_t a_count) {
    SyncInventory(a_owner);

    const auto base = a_obj->GetBaseObject();
    const auto obj_refid = a_obj->GetFormID();

    // TODO: discuss whether this should have been already stored
    auto& wo_stack = GetWOStack(obj_refid);

    UpdateStackOnAdd(a_owner, base, a_count, wo_stack);

    if (std::unique_lock lock(applied_variants_mutex_); world_object_stacks.contains(a_obj->GetFormID())) {
        world_object_stacks.erase(obj_refid);
    }
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

void InventoryManager::ProcessReference(RE::TESObjectREFR* a_ref) {
    const auto refid = a_ref->GetFormID();
    const auto base = a_ref->GetBaseObject();
    auto ref_count = a_ref->extraList.GetCount();
    ref_count = ref_count > 0 ? ref_count : 1;

    auto modelSwap = ModelSwapManager::GetSingleton();

    if (auto ref_variant = GetWOStack(refid); ref_variant.size() > 0) {
        logger::trace("Already applied");
        modelSwap->Apply(base, ref_variant.back());
    } else if (auto variant_vector = FetchFromQueue(base->GetFormID()); !variant_vector.empty()) {
        logger::trace("Queued");
        auto top_stack = GetTopOfStack(variant_vector, ref_count);
        top_stack = top_stack.empty() ? std::vector<int32_t>(ref_count) : top_stack;
        if (top_stack.back() == -1) {
            auto id = modelSwap->Process(base, refid);
            if (id != -1) {
                modelSwap->Apply(base, id);
                AddToOWStack(refid, id);
            }
        } else {
            modelSwap->Apply(base, top_stack.back());
            SetOWStack(refid, top_stack);
        }
    } else {
        logger::trace("None");
        auto id = modelSwap->Process(base, refid);
        if (id != -1) {
            logger::trace("Found variant");
            modelSwap->Apply(base, id);
            AddToOWStack(refid, id);
        }
        #ifndef NDEBUG
		else {
			logger::warn("No variant found for refid: {:x}", refid);
		}
        #endif
    }
}



void InventoryManager::MoveItem(RE::TESObjectREFR* a_this, const RE::TESBoundObject* a_item,
                                const int32_t a_count, RE::TESObjectREFR* a_other) {
    auto inv_variants = GetInventoryModels(a_this, a_item, a_count);
    UpdateStackOnRemove(a_this, a_item, a_count);
    UpdateStackOnAdd(a_other, a_item, a_count, inv_variants);
}

void InventoryManager::DropItem(RE::ITEM_REMOVE_REASON a_reason, RE::TESObjectREFR* a_this,
                                const RE::TESBoundObject* a_item, const int32_t a_count) {
    if (a_reason == RE::ITEM_REMOVE_REASON::kDropping) {
        OnItemDrop(a_this, a_item, a_count);
    } else {
        UpdateStackOnRemove(a_this, a_item, a_count);
    }
}


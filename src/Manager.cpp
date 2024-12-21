#include "Manager.h"
#include "Serialization.h"
#include <ranges>


void Manager::ClearData()
{
	std::unique_lock lock_inv(inventory_stacks_mutex_);
	std::unique_lock lock_var(applied_variants_mutex_);
	std::unique_lock lock_queue(queue_mutex_);

	inventory_stacks.clear();
	applied_variants.clear();
	variants_queue.clear();
}

std::map<std::string, uint32_t> Manager::GetModelPathMap(){
	std::map<std::string, uint32_t> model_map;
	std::set<const variant*> uniqueVariants;

	uint32_t index = 0;
	for (std::shared_lock lock(applied_variants_mutex_);
        const auto& a_variant : applied_variants | std::views::values) {
		if (uniqueVariants.insert(a_variant).second) {
			model_map[a_variant->model] = index;
			++index;
		}
	}

	for (std::shared_lock lock(inventory_stacks_mutex_); const auto& stack : inventory_stacks | std::views::values) {
        for (const auto& item : stack | std::views::values) {
            for (const auto& a_variant : item) {
                if (uniqueVariants.insert(a_variant).second) {
                    model_map[a_variant->model] = index;
                    ++index;
                }
            }
        }
    }
    
	return model_map;
}


void Manager::LoadSerializedData(const char* filename)
{
	logger::info("Loading data from {}", filename);

	ClearData();

	Serialization::Data saved_data;
	Serialization::loadDataBinary(saved_data, filename);

	// loop over data.applied and data.inventory and populate applied_variants and inventory_stacks
	for (const auto& [refid, model_index] : saved_data.applied) {
		if (!saved_data.lookup.contains(model_index)) {
			logger::critical("Model index not found in lookup: {}", model_index);
			continue;
		}
		const auto& model_name = saved_data.lookup.at(model_index);
		// check if model_name is a variant in current runtime
		if (const auto a_variant = GetVariant(model_name)) {
			applied_variants[refid] = a_variant;
		}
		else {
			logger::critical("Model name not found in sources: {}", model_name);
		}
	}

	for (const auto& [owner_refid, item_map] : saved_data.inventory) {
		for (const auto& [item_refid, model_indices] : item_map) {
			for (const auto model_index : model_indices) {
				if (!saved_data.lookup.contains(model_index)) {
					logger::critical("Model index not found in lookup: {}", model_index);
					continue;
				}
				const auto& model_name = saved_data.lookup.at(model_index);
				// check if model_name is a variant in current runtime
				if (const auto a_variant = GetVariant(model_name)) {
					inventory_stacks[owner_refid][item_refid].push_back(a_variant);
				}
				else {
					logger::critical("Model name not found in sources: {}", model_name);
				}
			}
		}
	}
}

void Manager::SerializeData(const char* filename)
{
	const auto file_path = Serialization::serialization_path + filename;

	const auto model_map = GetModelPathMap();

	std::unique_lock lock_inv(inventory_stacks_mutex_);
	std::unique_lock lock_var(applied_variants_mutex_);

	const Serialization::Data data(model_map, applied_variants, inventory_stacks);
	Serialization::saveDataBinary(data, file_path);
}

void Manager::SyncInventory(RE::TESObjectREFR* inventory_owner)
{
	std::map<FormID,int32_t> actual_inventory;
	const auto inv = inventory_owner->GetInventory();
	for (const auto& [bound,entry] : inv) {
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
				AddToStack(inventory_owner->GetFormID(), item, nullptr);
				--diff;
			}
		}
		else if (actual_count < stack_count) {
			auto diff = stack_count - actual_count;
			while (diff > 0) {
				RemoveFromStack(inventory_owner->GetFormID(), item);
				--diff;
			}
		}
	}
}

void Manager::AddToStack(const RefID owner_id, const FormID item_id, const variant* a_variant)
{
    inventory_stacks[owner_id][item_id].push_back(a_variant);
}

void Manager::RemoveFromStack(RefID owner_id, FormID item_id)
{
	if (!inventory_stacks[owner_id][item_id].empty()) {
		inventory_stacks[owner_id][item_id].pop_back();
	}
}

void Manager::PreLoadGame() {
	// TODO: We don't know if the player is loading the last save or not
    const auto saveLoadManager = RE::SaveLoadManager::GetSingleton();
	saveLoadManager->PopulateSaveList();
    auto newSave = Str::CopySaveFileNameWithoutExtension(saveLoadManager->lastFileFullName);

	const auto file_path = Serialization::serialization_path + newSave;

	try {
	    LoadSerializedData(file_path.c_str());
	}
	catch (const std::exception& e) {
		logger::error("Failed to load data: {}", e.what());
	}

    logger::trace("new: {}", newSave);

    delete lastSave;
    lastSave = newSave;
	logger::info("PreLoadGame completed");
}

void Manager::SaveGame(const char* save_name) {

	try {
	    SerializeData(save_name);
	}
	catch (const std::exception& e) {
		logger::error("Failed to serialize data: {}", e.what());
	}
}

void Manager::Register(std::string key, variants value) {
    key = Str::processString(key);

    if (const auto it = sources.find(key); it != sources.end()) {
        it->second.insert(it->second.end(), value.begin(), value.end());
    } else {
        sources[key] = value;
    }

    for (const auto item : value) {
        sources[item->key] = value;
    }
}

void Manager::Process(RE::TESBoundObject* base, const RefID id) {
    const auto wrapper = new AVModel(base);
    if (const auto variant = Process(wrapper, id)) {
	    std::unique_lock lock(applied_variants_mutex_);
        applied_variants[id] = variant;
    }
    delete wrapper;
}

void Manager::Process(RE::TESObjectARMA* base, const RE::FormID id) const {
    const auto wrapper = new AVObjectARMA(base);
    Process(wrapper, id);
    delete wrapper;
}

void Manager::HandleItemPickup(RE::TESObjectREFR* a_owner, RE::TESObjectREFR* a_obj, const int32_t a_count)
{

	const auto base = a_obj->GetBaseObject();
	const auto obj_refid = a_obj->GetFormID();

    // TODO: discuss whether this should have been already stored
	const auto* a_variant = GetAppliedVariant(obj_refid);

#ifndef NDEBUG
    if (!a_variant ) {
		logger::trace("No variant found for {}", obj_refid);
		// return; Don't return. Need to add nullptr to the stack.
	}
#endif
	std::vector<const variant*> variant_vector;
	for (int i = 0; i < a_count; ++i) {
		variant_vector.push_back(a_variant);
	}
	UpdateStackOnAdd(a_owner,base,a_count,variant_vector);

	if (std::unique_lock lock(applied_variants_mutex_);
        applied_variants.contains(a_obj->GetFormID())) {
        applied_variants.erase(obj_refid);
	}
}

void Manager::UpdateStackOnAdd(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, int32_t a_count, const std::vector<const variant*>& a_variant_vector)
{
	SyncInventory(a_owner);
	size_t index = 0;
	std::unique_lock lock(inventory_stacks_mutex_);
	for (int i = 0; i < a_count; ++i) {
		if (index >= a_variant_vector.size()) {
			AddToStack(a_owner->GetFormID(), a_obj->GetFormID(), nullptr);
		}
		else {
			AddToStack(a_owner->GetFormID(), a_obj->GetFormID(), a_variant_vector[index]);
			++index;
		}
	}
}

void Manager::UpdateStackOnRemove(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, int32_t a_count)
{
	SyncInventory(a_owner);
	std::unique_lock lock(inventory_stacks_mutex_);
	for (int i = 0; i < a_count; ++i) {
		RemoveFromStack(a_owner->GetFormID(), a_obj->GetFormID());
	}

}

const variant* Manager::GetInventoryModel(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_item)
{
	std::shared_lock lock(inventory_stacks_mutex_);
    if (const auto it = inventory_stacks.find(a_owner->GetFormID());
        it != inventory_stacks.end()) {
		if (const auto it2 = it->second.find(a_item->GetFormID()); it2 != it->second.end()) {
			if (!it2->second.empty()) {
				return it2->second.back();
			}
		}
	}
	return nullptr;
}

std::vector<const variant*> Manager::GetInventoryModels(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_item, int32_t a_count)
{
	std::shared_lock lock(inventory_stacks_mutex_);
	if (const auto it = inventory_stacks.find(a_owner->GetFormID());
		it != inventory_stacks.end()) {
		if (const auto it2 = it->second.find(a_item->GetFormID()); it2 != it->second.end()) {
			if (!it2->second.empty()) {
				// need to collect from the back of the vector <-> top of the stack
				std::vector<const variant*> result;
				if (static_cast<int32_t>(it2->second.size()) >= a_count) {
                    // Copy the last a_count elements
                    result.insert(result.end(), it2->second.end() - a_count, it2->second.end());
                } else {
                    // Add nullptrs to the beginning if count is larger than the stack size
                    result.insert(result.end(), a_count - it2->second.size(), nullptr);
                    result.insert(result.end(), it2->second.begin(), it2->second.end());
                }
				return result;
			}
		}
	}
	return {};
}

void Manager::SetInventoryBaseModel(RE::TESObjectREFR* owner, RE::InventoryEntryData* a_entry)
{
	if (const auto base = a_entry->GetObject()) {
		if (const auto variant = GetSingleton()->GetInventoryModel(owner,base)) {
			if (const auto bm = base->As<RE::TESModel>()) {
				bm->SetModel(variant->model);
			}
			if (base->Is(RE::TESObjectWEAP::FORMTYPE)) {
				const auto weap = base->As<RE::TESObjectWEAP>();
				if (const auto fpModelObj = weap->firstPersonModelObject) {
					fpModelObj->SetModel(variant->model);
				}
			}
			// TODO: Populate for other types

			if (const auto inv = RE::Inventory3DManager::GetSingleton()) {
                if (!inv->GetRuntimeData().loadedModels.empty()) {
					inv->Clear3D();
					inv->GetRuntimeData().loadedModels.clear();
					inv->UpdateItem3D(a_entry);
                }
			}
		}
    }
}

const variant* Manager::GetAppliedVariant(const RefID id)
{
	std::shared_lock lock(applied_variants_mutex_);
    if (const auto it = applied_variants.find(id);
        it != applied_variants.end()) {
		return it->second;
	}
	return nullptr;
}

void Manager::ApplyVariant(RE::TESBoundObject* base, const RefID id, const variant* a_variant)
{
	if (const auto bm = base->As<RE::TESModel>()) {
        bm->SetModel(a_variant->model);
        std::unique_lock lock(applied_variants_mutex_);
        applied_variants[id] = a_variant;
	}
}

void Manager::AddToQueue(FormID formid, const variant* a_variant)
{
	std::unique_lock lock(queue_mutex_);
	const auto pair = std::make_pair(formid, a_variant);
    variants_queue.push_back(pair);
}

const variant* Manager::FetchFromQueue(const FormID formId)
{
	std::unique_lock lock(queue_mutex_);

    if (variants_queue.empty()) {
		return nullptr;
	}
	for (auto it = variants_queue.begin(); it != variants_queue.end(); ++it) {
		if (it->first == formId) {
			const variant* result = it->second;
			variants_queue.erase(it);
			return result;
        }
    }
	return nullptr;
}

void Manager::HandleItemDrop(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count)
{
	if (const auto variant = GetInventoryModel(a_owner, a_obj)) {
        AddToQueue(a_obj->GetFormID(), variant);
	}
	UpdateStackOnRemove(a_owner,a_obj,a_count);
}

const variant* Manager::GetVariant(const std::string& model_name)
{
	for (const auto& value : sources | std::views::values) {
		for (const auto& item : value) {
			if (std::string(item->model) == model_name) {
				return item;
			}
		}
	}
	return nullptr;
}

const variant* Manager::Process(AVObject* arma, const RE::FormID id) const {
    return arma->Match(sources, static_cast<int>(id));
}

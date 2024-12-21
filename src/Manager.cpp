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

	model_map[""] = 0;

	uint32_t index = 1;
	for (std::shared_lock lock(applied_variants_mutex_);
        const auto& a_variant : applied_variants | std::views::values) {
		if (!a_variant) continue;
		if (uniqueVariants.insert(a_variant).second) {
			model_map[a_variant->model] = index;
			++index;
		}
	}

	for (std::shared_lock lock(inventory_stacks_mutex_); const auto& stack : inventory_stacks | std::views::values) {
        for (const auto& item : stack | std::views::values) {
            for (const auto& a_variant : item) {
				if (!a_variant) continue;
                if (uniqueVariants.insert(a_variant).second) {
                    model_map[a_variant->model] = index;
                    ++index;
                }
            }
        }
    }

	for (std::shared_lock lock(applied_variants_mutex_); const auto & stack : world_object_stacks | std::views::values) {
		for (const auto& a_variant : stack) {
			if (!a_variant) continue;
			if (uniqueVariants.insert(a_variant).second) {
				model_map[a_variant->model] = index;
				++index;
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

	std::unique_lock lock_inv(inventory_stacks_mutex_);
	std::unique_lock lock_var(applied_variants_mutex_);

	// loop over data.applied and data.inventory and populate applied_variants and inventory_stacks
	for (const auto& [refid, model_index] : saved_data.applied) {
		if (!saved_data.lookup.contains(model_index)) {
			logger::critical("Model index not found in lookup: {}", model_index);
			continue;
		}
		const auto& model_name = saved_data.lookup.at(model_index);
		// check if model_name is a variant in current runtime
		if (model_name.empty()) {
			applied_variants[refid] = nullptr;
		}
		else if (const auto a_variant = GetVariant(model_name)) {
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
				if (model_name.empty()) {
					inventory_stacks[owner_refid][item_refid].push_back(nullptr);
		        }
				else if (const auto a_variant = GetVariant(model_name)) {
					inventory_stacks[owner_refid][item_refid].push_back(a_variant);
				}
				else {
					logger::critical("Model name not found in sources: {}", model_name);
				}
			}
		}
	}

	for (const auto& [owner_refid, model_indices] : saved_data.worldobject) {
		for (const auto model_index : model_indices) {
			if (!saved_data.lookup.contains(model_index)) {
				logger::critical("Model index not found in lookup: {}", model_index);
				continue;
			}
			const auto& model_name = saved_data.lookup.at(model_index);
			// check if model_name is a variant in current runtime
			if (model_name.empty()) {
				world_object_stacks[owner_refid].push_back(nullptr);
		    }
			else if (const auto a_variant = GetVariant(model_name)) {
				world_object_stacks[owner_refid].push_back(a_variant);
			}
			else {
				logger::critical("Model name not found in sources: {}", model_name);
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

	const Serialization::Data data(model_map, applied_variants, inventory_stacks, world_object_stacks);
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

void Manager::RemoveFromStack(const RefID owner_id, const FormID item_id)
{
	if (!inventory_stacks[owner_id][item_id].empty()) {
		inventory_stacks[owner_id][item_id].pop_back();
	}
}

v_variant& Manager::GetWOStack(const RefID refid)
{
	return world_object_stacks[refid];
}

std::vector<const variant*> Manager::GetTopOfStack(const std::vector<const variant*>& stack, const uint32_t count)
{
	std::vector<const variant*> result;
	if (static_cast<uint32_t>(stack.size()) >= count) {
        // Copy the last a_count elements
        result.insert(result.end(), stack.end() - count, stack.end());
    } else {
        // Add nullptrs to the beginning if count is larger than the stack size
        result.insert(result.end(), count - stack.size(), nullptr);
        result.insert(result.end(), stack.begin(), stack.end());
    }
	return result;
}

void Manager::PreLoadGame(const std::string& filename) {
	logger::info("PreLoadGame started. Filename: {}", filename.c_str());

	const auto file_path = Serialization::serialization_path + filename;
	try {
	    LoadSerializedData(file_path.c_str());
	}
	catch (const std::exception& e) {
		logger::error("Failed to load data: {}", e.what());
	}
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

const variant* Manager::Process(RE::TESForm* base, const RefID id) {
    const variant* result = nullptr;
	if (auto obj = base->As<RE::TESObjectARMO>()) {
        result = ProcessImpl(obj, id);
	}
	else if (auto obj = base->As<RE::TESObjectARMA>()) {
        result = ProcessImpl(obj, id);
    }
	else if (auto obj = base->As<RE::TESObjectWEAP>()) {
        result = ProcessImpl(obj, id);
    } else if (auto model = base->As<RE::TESModel>()) {
        result = ProcessImpl(base, id);
	}

	if (!result) {
        return nullptr;
	}

    if (base->IsInventoryObject()) {
		std::unique_lock lock(applied_variants_mutex_);
        applied_variants[id] = result;
    }

    return result;
}

const variant* Manager::ProcessImpl(RE::TESObjectARMA* base, const RE::FormID id) const {
    const auto wrapper = new AVObjectARMA(base);
    const variant* result = wrapper->Match(sources, id);
    delete wrapper;
    return result;
}

const variant* Manager::ProcessImpl(RE::TESObjectARMO* base, RE::FormID id) const {
    const auto wrapper = new AVObjectARMO(base);
    const variant* result = wrapper->Match(sources, id);
    delete wrapper;
    return result;
}

const variant* Manager::ProcessImpl(RE::TESObjectWEAP* base, RE::FormID id) const {
    const auto wrapper = new AVObjectWEAP(base);
    const variant* result = wrapper->Match(sources, id);
    delete wrapper;
    return result;
}

const variant* Manager::ProcessImpl(RE::TESForm* base, RE::FormID id) const {
    const auto wrapper = new AVModel(base);
    const variant* result = wrapper->Match(sources, id);
    delete wrapper;
    return result;
}

void Manager::OnItemPickup(RE::TESObjectREFR* a_owner, RE::TESObjectREFR* a_obj, const int32_t a_count)
{

	const auto base = a_obj->GetBaseObject();
	const auto obj_refid = a_obj->GetFormID();

    // TODO: discuss whether this should have been already stored
	const auto* a_variant = GetAppliedVariant(obj_refid);
	auto& wo_stack = GetWOStack(obj_refid);

#ifndef NDEBUG
    if (!a_variant ) {
		logger::trace("No variant found for {}", obj_refid);
		// return; Don't return. Need to add nullptr to the stack.
	}
#endif
	// need to make sure wo_stack and a_count are in sync

	if (wo_stack.size() < static_cast<size_t>(a_count)) {
		wo_stack.insert(wo_stack.end(), a_count - wo_stack.size(), nullptr);
	}
	else if (wo_stack.size() > static_cast<size_t>(a_count)) {
		wo_stack.erase(wo_stack.begin() + a_count, wo_stack.end());
	}
	UpdateStackOnAdd(a_owner,base,a_count,wo_stack);

	if (std::unique_lock lock(applied_variants_mutex_);
        applied_variants.contains(a_obj->GetFormID())) {
        applied_variants.erase(obj_refid);
		world_object_stacks.erase(obj_refid);
	}
}

void Manager::UpdateStackOnAdd(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count, const std::vector<const variant*>& a_variant_vector)
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

void Manager::UpdateStackOnRemove(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count)
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

std::vector<const variant*> Manager::GetInventoryModels(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_item, const int32_t a_count)
{
	std::shared_lock lock(inventory_stacks_mutex_);
	if (const auto it = inventory_stacks.find(a_owner->GetFormID());
		it != inventory_stacks.end()) {
		if (const auto it2 = it->second.find(a_item->GetFormID()); it2 != it->second.end()) {
			if (!it2->second.empty()) {
				// need to collect from the back of the vector <-> top of the stack
				return GetTopOfStack(it2->second, a_count);
			}
		}
	}
	return {};
}

void Manager::SetInventoryBaseModel(RE::TESObjectREFR* owner, RE::InventoryEntryData* a_entry)
{
	if (const auto base = a_entry->GetObject()) {
		if (const auto variant = GetSingleton()->GetInventoryModel(owner, base)) {

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

void Manager::AddToQueue(FormID formid, const std::vector<const variant*>& variant_vector)
{
	std::unique_lock lock(queue_mutex_);
	const auto pair = std::make_pair(formid, variant_vector);
    variants_queue.push_back(pair);
}

v_variant Manager::FetchFromQueue(const FormID formId)
{
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

void Manager::OnItemDrop(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count)
{
	if (const auto variants = GetInventoryModels(a_owner, a_obj,a_count);!variants.empty()) {
        AddToQueue(a_obj->GetFormID(), variants);
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

void Manager::ProcessReference(RE::TESObjectREFR* a_ref)
{
    const auto refid = a_ref->GetFormID();
	const auto base = a_ref->GetBaseObject();
    auto ref_count = a_ref->extraList.GetCount();
	ref_count = ref_count > 0 ? ref_count : 1;

	if (!base) {
		logger::warn("Base object not found for refid: {:x}", refid);
		return;
	}

	if (base->IsInventoryObject()) {
        if (const auto ref_variant = GetAppliedVariant(refid)) {
            ApplyVariant(base, refid, ref_variant);
        } else if (const auto variant_vector = FetchFromQueue(base->GetFormID()); !variant_vector.empty()) {
            auto top_stack = GetTopOfStack(variant_vector, ref_count);
            top_stack = top_stack.empty() ? std::vector<const variant*>(ref_count, nullptr) : top_stack;
            ApplyVariant(base, refid, top_stack.back());
            world_object_stacks[refid] = top_stack;
        } else {
            Process(base, refid);
            if (const auto applied_variant = GetAppliedVariant(refid)) {
                world_object_stacks[refid] = std::vector(ref_count, applied_variant);
            } else {
                world_object_stacks[refid] = std::vector<const variant*>(ref_count, nullptr);
            }
        }
    } else {
        Process(base, refid);
	}

}

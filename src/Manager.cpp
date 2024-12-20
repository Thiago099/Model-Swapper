#include "Manager.h"
#include "Serialization.h"
#include <ranges>


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
	return model_map;
}


void Manager::LoadSerializedData(const char* filename)
{
	logger::info("Loading data from {}", filename);

	std::unique_lock lock_inv(inventory_stacks_mutex_);
	std::unique_lock lock_var(applied_variants_mutex_);
	std::unique_lock lock_queue(queue_mutex_);

	inventory_stacks.clear();
	applied_variants.clear();
	variants_queue.clear();

	Serialization::Data saved_data;
	Serialization::loadDataBinary(saved_data, filename);
	logger::info("Saved data loaded with applied size {}", saved_data.applied.size());
	logger::info("Saved data loaded with inventory size {}", saved_data.inventory.size());
	logger::info("Saved data loaded with lookup size {}", saved_data.lookup.size());

	// loop over data.applied and data.inventory and populate applied_variants and inventory_stacks
	for (const auto& [refid, model_index] : saved_data.applied) {
		if (!saved_data.lookup.contains(model_index)) {
			logger::critical("Model index not found in lookup: {}", model_index);
			continue;
		}
		const auto model_name = saved_data.lookup.at(model_index);
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
				const auto model_name = saved_data.lookup.at(model_index);
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
	std::shared_lock lock_inv(inventory_stacks_mutex_);
	std::shared_lock lock_var(applied_variants_mutex_);
	std::shared_lock lock_queue(queue_mutex_);

	const auto model_map = GetModelPathMap();
	const Serialization::Data data(model_map, applied_variants, inventory_stacks);
	Serialization::saveDataBinary(data, file_path);
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
	else logger::warn("No variant found for {}", id);
    delete wrapper;
}

void Manager::Process(RE::TESObjectARMA* base, const RE::FormID id) const {
    const auto wrapper = new AVObjectARMA(base);
    Process(wrapper, id);
    delete wrapper;
}

void Manager::UpdateStackOnPickUp(const RE::TESObjectREFR* a_owner, RE::TESObjectREFR* a_obj, int32_t a_count)
{

	const auto base = a_obj->GetBaseObject();

    // TODO: discuss whether this should have been already stored
	const auto* variant = GetAppliedVariant(a_obj->GetFormID());
	if (!variant) {
		logger::error("No variant found for {}", a_obj->GetFormID());
		return;
	}

	std::unique_lock lock(inventory_stacks_mutex_);
    while (a_count>0) {
		logger::info("Add.Owner: {} {:x}, Item: {:x}", a_owner->GetName(),a_owner->GetFormID(), a_obj->GetFormID());
        inventory_stacks[a_owner->GetFormID()][base->GetFormID()].push_back(variant);
		a_count--;
    }
    for (const auto& item : inventory_stacks[a_owner->GetFormID()][base->GetFormID()]) {
		if (item && item->model) logger::info("Stack: {}", item->model);
	}

	if (std::unique_lock lock2(applied_variants_mutex_);
        applied_variants.contains(a_obj->GetFormID())) {
        applied_variants.erase(a_obj->GetFormID());
	}
}

void Manager::UpdateStackOnDrop(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, int32_t a_count)
{
	std::unique_lock lock1(inventory_stacks_mutex_);
    while (a_count>0) {
		logger::info("Remove. Owner: {} {:x}, Item: {:x}", a_owner->GetName(),a_owner->GetFormID(), a_obj->GetFormID());
        if (!inventory_stacks[a_owner->GetFormID()][a_obj->GetFormID()].empty()) {
            inventory_stacks[a_owner->GetFormID()][a_obj->GetFormID()].pop_back();
            logger::info("Stack size: {}",
                         inventory_stacks[a_owner->GetFormID()][a_obj->GetFormID()].size());
			a_count--;
        }
        else break;
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

void Manager::SetInventoryBaseModel(RE::InventoryEntryData* a_entry)
{
	if (const auto base = a_entry->GetObject()) {
		if (const auto variant = Manager::GetSingleton()->GetInventoryModel(RE::PlayerCharacter::GetSingleton(),base)) {
			if (const auto bm = base->As<RE::TESModel>()) {
				bm->SetModel(variant->model);
			}
			if (base->Is(RE::TESObjectWEAP::FORMTYPE)) {
				const auto weap = base->As<RE::TESObjectWEAP>();
				weap->firstPersonModelObject->SetModel(variant->model);
			}
			const auto inv = RE::Inventory3DManager::GetSingleton();
            inv->Clear3D();
            inv->GetRuntimeData().loadedModels.clear();
            inv->UpdateItem3D(a_entry);
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

bool Manager::HasVariant(const RefID refid)
{
	std::shared_lock lock(applied_variants_mutex_);
	return applied_variants.contains(refid);
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
    return arma->Match(sources, id);
}

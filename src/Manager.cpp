#include "Manager.h"

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

	if (std::unique_lock lock2(applied_variants_mutex_); applied_variants.contains(a_obj->GetFormID())) {
		applied_variants.erase(a_obj->GetFormID());
	}
}

void Manager::UpdateStackOnDrop(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, int32_t a_count)
{
	std::unique_lock lock1(inventory_stacks_mutex_);
    while (a_count>0) {
		logger::info("Remove. Owner: {} {:x}, Item: {:x}", a_owner->GetName(),a_owner->GetFormID(), a_obj->GetFormID());
		if (inventory_stacks[a_owner->GetFormID()][a_obj->GetFormID()].size() > 0) {
			inventory_stacks[a_owner->GetFormID()][a_obj->GetFormID()].pop_back();
			logger::info("Stack size: {}", inventory_stacks[a_owner->GetFormID()][a_obj->GetFormID()].size());
			a_count--;
        }
        else break;
    }
}

const variant* Manager::GetInventoryModel(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_item)
{
	std::shared_lock lock(inventory_stacks_mutex_);
	if (const auto it = inventory_stacks.find(a_owner->GetFormID()); it != inventory_stacks.end()) {
		if (const auto it2 = it->second.find(a_item->GetFormID()); it2 != it->second.end()) {
			if (it2->second.size() > 0) {
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
				auto weap = base->As<RE::TESObjectWEAP>();
				weap->firstPersonModelObject->SetModel(variant->model);
			}
			auto inv = RE::Inventory3DManager::GetSingleton();
            inv->Clear3D();
            inv->GetRuntimeData().loadedModels.clear();
            inv->UpdateItem3D(a_entry);
		}
    }
}

const variant* Manager::GetAppliedVariant(const RefID id)
{
	std::shared_lock lock(applied_variants_mutex_);
	if (const auto it = applied_variants.find(id); it != applied_variants.end()) {
		return it->second;
	}
	return nullptr;
}

void Manager::ApplyVariant(RE::TESBoundObject* base,RefID id, const variant* a_variant)
{
	if (const auto bm = base->As<RE::TESModel>()) {
        bm->SetModel(a_variant->model);
        std::unique_lock lock(applied_variants_mutex_);
        applied_variants[id] = a_variant;
	}
}

void Manager::AddToQueue(const variant* a_variant)
{
	std::unique_lock lock(queue_mutex_);
	variants_queue.push_back(a_variant);
}

bool Manager::HasVariant(RefID refid)
{
	std::shared_lock lock(applied_variants_mutex_);
	if (applied_variants.contains(refid)) {
		return true;
	}
	return false;
}

const variant* Manager::FetchFromQueue()
{
	std::unique_lock lock(queue_mutex_);
	if (variants_queue.empty()) {
		return nullptr;
	}
	const variant* result = variants_queue.front();
	variants_queue.erase(variants_queue.begin());
    return result;
}

const variant* Manager::Process(AVObject* arma, const RE::FormID id) const {
    return arma->Match(sources, id);
}

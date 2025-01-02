#pragma once
#include <shared_mutex>
#include "Lib/Str.h"
#include "Lib/SaveLoadManager.h"
#include "Lib/Singleton.h"
#include "Application/Model.h"

class InventoyStack : public Singleton<InventoyStack> {
	std::shared_mutex inventory_stacks_mutex_;

    std::map<RefID, inventory_stack> inventory_stacks;

public:
    void ClearData();

    const int32_t GetInventoryModel(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_item);

    void Remove(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count);

    void AddMultiple(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, v_variant& add_vector);

    void Add(RefID owner, RefID item, int a_variant);

    std::shared_mutex & GetMutex();

    std::map<RefID, inventory_stack> GetAll();

    std::vector<variantId> GetItemsByContainerAndBase(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_item);
};
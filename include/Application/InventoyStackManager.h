#pragma once
#include <shared_mutex>
#include "Lib/Str.h"
#include "Lib/SaveLoadManager.h"
#include "Lib/Singleton.h"
#include "Application/Model.h"

class InventoyStackManager : public Singleton<InventoyStackManager> {
	std::shared_mutex inventory_stacks_mutex_;

    std::map<RefID, inventory_stack> inventory_stacks;

    void AddItemToStack(const RefID owner_id, const FormID item_id, variantId a_variant);

    void RemoveItemFromStack(const RefID owner_id, const FormID item_id);

    void UpdateStackOnRemove(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count);

    std::vector<int32_t> GetInventoryModels(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_item,
                                            const int32_t a_count);
    void OnItemDrop(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count);
    void SyncInventory(RE::TESObjectREFR* inventory_owner);

    void AddItemsToStack(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count,
                         v_variant& add_vector);

public:
    void ClearData();


    void OnItemTransfer(RE::TESObjectREFR* a_this, const RE::TESBoundObject* a_item, const int32_t a_count,
                  RE::TESObjectREFR* a_other);

    void OnItemDrop(RE::ITEM_REMOVE_REASON a_reason, RE::TESObjectREFR* a_this, const RE::TESBoundObject* a_item,
                  const int32_t a_count);



    void OnItemPickup(RE::TESObjectREFR* a_owner, RE::TESObjectREFR* a_obj, const int32_t a_count);

    const int32_t GetInventoryModel(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_item);

    std::shared_mutex & GetMutex();

    void Add(RefID owner, RefID item, int model);

    std::map<RefID, inventory_stack> GetAll();
};
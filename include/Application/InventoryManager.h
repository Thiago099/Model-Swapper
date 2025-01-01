#pragma once
#include <shared_mutex>
#include "Lib/Str.h"
#include "Lib/SaveLoadManager.h"
#include "Lib/Singleton.h"
#include "Application/Model.h"

class InventoryManager : public Singleton<InventoryManager> {
	std::shared_mutex inventory_stacks_mutex_;
    std::shared_mutex queue_mutex_;

    std::map<RefID, inventory_stack> inventory_stacks;
    std::vector<std::pair<FormID, v_variant>> variants_queue;

    void AddToStack(const RefID owner_id, const FormID item_id, variantId a_variant);

    void RemoveFromStack(const RefID owner_id, const FormID item_id);

    void UpdateStackOnRemove(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count);

    std::vector<int32_t> GetInventoryModels(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_item,
                                            const int32_t a_count);
    void AddToDropQueue(FormID formid, v_variant& variant_vector);
    void OnItemDrop(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count);
    void SyncInventory(RE::TESObjectREFR* inventory_owner);

public:
    void ClearData();

    v_variant GetNextItemFromDropQueue(const FormID formId);

    void OnItemTransfer(RE::TESObjectREFR* a_this, const RE::TESBoundObject* a_item, const int32_t a_count,
                  RE::TESObjectREFR* a_other);

    void OnItemDrop(RE::ITEM_REMOVE_REASON a_reason, RE::TESObjectREFR* a_this, const RE::TESBoundObject* a_item,
                  const int32_t a_count);


    void AddItemsToStack(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count,
                        v_variant& add_vector);

    void OnItemPickup(RE::TESObjectREFR* a_owner, RE::TESObjectREFR* a_obj, const int32_t a_count);

    const int32_t GetInventoryModel(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_item);

    std::shared_mutex & GetMutex();

    void Add(RefID owner, RefID item, int model);

    std::map<RefID, inventory_stack> GetAll();
    std::vector<std::pair<FormID, v_variant>> GetQueue();
};
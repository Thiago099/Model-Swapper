#pragma once
#include <shared_mutex>
#include "Str.h"
#include "SaveLoadManager.h"
#include "Serialization.h"
#include "Singleton.h"

class InventoryManager : public Singleton<InventoryManager> {
	std::shared_mutex inventory_stacks_mutex_;
    std::shared_mutex applied_variants_mutex_;
    std::shared_mutex queue_mutex_;

    std::map<RefID, inventory_stack> inventory_stacks;
    std::map<RefID, v_variant> world_object_stacks;
    std::vector<std::pair<FormID, v_variant>> variants_queue;
public:
    void ClearData();
    void LoadSerializedData(const char* filename);
    void SerializeData(const char* filename);
    void SyncInventory(RE::TESObjectREFR* inventory_owner);
    void AddToStack(const RefID owner_id, const FormID item_id, variantId a_variant);
    void RemoveFromStack(const RefID owner_id, const FormID item_id);
    v_variant& GetWOStack(const RefID refid);
    v_variant GetTopOfStack(v_variant& stack, const int32_t count);
    void AddToOWStack(RefID id, int32_t item);
    void SetOWStack(RefID id, v_variant items);


    std::map<RefID, inventory_stack> GetInventoryStacks();
    std::map<RefID, v_variant> GetWorldObjectStacks();
    std::vector<std::pair<FormID, v_variant>> GetVariantsQueue();
    void OnItemPickup(RE::TESObjectREFR* a_owner, RE::TESObjectREFR* a_obj, const int32_t a_count);
    void UpdateStackOnAdd(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count,
                          v_variant& add_vector);
    void UpdateStackOnRemove(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count);
    const int32_t GetInventoryModel(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_item);
    std::vector<int32_t> GetInventoryModels(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_item,
                                            const int32_t a_count);
    void AddToQueue(FormID formid, v_variant& variant_vector);
    v_variant FetchFromQueue(const FormID formId);
    void OnItemDrop(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count);
    void ProcessReference(RE::TESObjectREFR* refr);
    void SetInventoryBaseModel(RE::TESObjectREFR* owner, RE::InventoryEntryData* a_entry);
};
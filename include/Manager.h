#pragma once
#include <shared_mutex>
#include "Str.h"
#include "SaveLoadManager.h"
#include "Serialization.h"

class Manager {

    const char* lastSave=nullptr;
    models sources;
    const variant* Process(AVObject* arma, RE::FormID id) const;

	std::shared_mutex inventory_stacks_mutex_;
	std::shared_mutex applied_variants_mutex_;
	std::shared_mutex queue_mutex_;

    std::map<RefID, inventory_stack> inventory_stacks;
    std::map<RefID, v_variant> worldobject_stacks;
    // TODO: clear on TESFormDelete
    std::map<RefID, const variant*> applied_variants;
    std::vector<std::pair<FormID, v_variant>> variants_queue;

    void ClearData();

    std::map<std::string, uint32_t> GetModelPathMap();
	void LoadSerializedData(const char* filename);
	void SerializeData(const char* filename);

    void SyncInventory(RE::TESObjectREFR* inventory_owner);
    void AddToStack(RefID owner_id, FormID item_id, const variant* a_variant);
    void RemoveFromStack(RefID owner_id, FormID item_id);
    static v_variant GetTopOfStack(const v_variant& stack, uint32_t count);
    v_variant& GetWOStack(RefID refid);

	void ApplyVariant(RE::TESBoundObject* base,RefID id, const variant* a_variant);

public:
    static Manager* GetSingleton() {
        static Manager singleton;
        return &singleton;
    }

    void PreLoadGame(const std::string& filename);
    void SaveGame(const char* save_name);

    void Register(std::string key, variants value);
    void Process(RE::TESBoundObject* base, RefID id);
    void Process(RE::TESObjectARMA* base, RE::FormID id) const;

    const variant* GetInventoryModel(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_item);
    v_variant GetInventoryModels(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_item, int32_t a_count);
    static void SetInventoryBaseModel(RE::TESObjectREFR* owner, RE::InventoryEntryData* a_entry);

    const variant* GetVariant(const std::string& model_name);
	void ProcessReference(RE::TESObjectREFR* a_ref);
	const variant* GetAppliedVariant(RefID id);

    void AddToQueue(FormID formid, const v_variant& variant_vector);
	v_variant FetchFromQueue(FormID formId);

    void UpdateStackOnAdd(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, int32_t a_count, const v_variant& a_variant_vector);
    void UpdateStackOnRemove(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, int32_t a_count);
    void OnItemPickup(RE::TESObjectREFR* a_owner, RE::TESObjectREFR* a_obj, int32_t a_count);
    void OnItemDrop(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, int32_t a_count);
};
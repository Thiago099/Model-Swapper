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
    // TODO: clear on TESFormDelete
    std::map<RefID, const variant*> applied_variants;
    std::vector<std::pair<FormID,const variant*>> variants_queue;


    std::map<std::string, uint32_t> GetModelPathMap();
	void LoadSerializedData(const char* filename);
	void SerializeData(const char* filename);

public:
    static Manager* GetSingleton() {
        static Manager singleton;
        return &singleton;
    }
    void CleanData();
    void PreLoadGame();
    void SaveGame(const char* save_name);
    void Register(std::string key, variants value);
    void Process(RE::TESBoundObject* base, RefID id);
    void Process(RE::TESObjectARMA* base, RE::FormID id) const;

    void UpdateStackOnPickUp(const RE::TESObjectREFR* a_owner, RE::TESObjectREFR* a_obj, int32_t a_count);
    void UpdateStackOnDrop(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, int32_t a_count);
    void TransferOwnership(const RE::TESObjectREFR* a_oldOwner, const RE::TESObjectREFR* a_newOwner,
                           const RE::TESBoundObject* a_obj, int32_t a_count);
    const variant* GetInventoryModel(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_item);
    static void SetInventoryBaseModel(RE::TESObjectREFR* owner, RE::InventoryEntryData* a_entry);
    bool HasVariant(RefID refid);
    const variant* GetVariant(const std::string& model_name);
	void ApplyVariant(RE::TESBoundObject* base,RefID id, const variant* a_variant);
	const variant* GetAppliedVariant(RefID id);
    void AddToQueue(FormID formid, const variant* a_variant);
	const variant* FetchFromQueue(FormID formId);
};
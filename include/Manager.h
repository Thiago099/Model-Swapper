#pragma once
#include <shared_mutex>
#include "Str.h"
#include "Wrapper.h"
#include "SaveLoadManager.h"

using inventory_stack = std::map<FormID, std::vector<const variant*>>;

//TODO: Serialize this object into a file
struct ManagerSaveGameData {
    std::map<RefID, inventory_stack> inventory_stacks;
    // TODO: clear on TESFormDelete
    std::map<RefID, const variant*> applied_variants;
    std::vector<const variant*> variants_queue;

    ManagerSaveGameData() {}
    ManagerSaveGameData(const ManagerSaveGameData& other)
        : inventory_stacks(other.inventory_stacks),
          applied_variants(other.applied_variants),
          variants_queue(other.variants_queue) {
    }
};

class Manager {

    const char* lastSave;
    models sources;
    const variant* Process(AVObject* arma, RE::FormID id) const;

    std::map<const char*, ManagerSaveGameData> saveGameData;

	std::shared_mutex inventory_stacks_mutex_;
	std::shared_mutex applied_variants_mutex_;
	std::shared_mutex queue_mutex_;

public:
    static Manager* GetSingleton() {
        static Manager singleton;
        return &singleton;
    }
    ManagerSaveGameData& GetCurrentSaveGameData();
    void PreLoadGame();
    void SaveGame();
    void Register(std::string key, variants value);
    void Process(RE::TESBoundObject* base, RefID id);
    void Process(RE::TESObjectARMA* base, RE::FormID id) const;

    void UpdateStackOnPickUp(const RE::TESObjectREFR* a_owner, RE::TESObjectREFR* a_obj, int32_t a_count);
    void UpdateStackOnDrop(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, int32_t a_count);
    const variant* GetInventoryModel(const RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_item);
    static void SetInventoryBaseModel(RE::InventoryEntryData* a_entry);
	void ApplyVariant(RE::TESBoundObject* base,RefID id, const variant* a_variant);
	const variant* GetAppliedVariant(RefID id);
    bool HasVariant(RefID refid);
    void AddToQueue(const variant* a_variant);
	const variant* FetchFromQueue();
};
#pragma once
#include <shared_mutex>
#include "Str.h"
#include "Wrapper.h"

using inventory_stack = std::map<FormID, std::vector<const variant*>>;

class Manager {
    models sources;
    const variant* Process(AVObject* arma, RE::FormID id) const;
    std::map<RefID,inventory_stack> inventory_stacks;
    // TODO: clear on TESFormDelete
    std::map<RefID, const variant*> applied_variants;
	std::vector<const variant*> variants_queue;

	std::shared_mutex inventory_stacks_mutex_;
	std::shared_mutex applied_variants_mutex_;
	std::shared_mutex queue_mutex_;

public:
    static Manager* GetSingleton() {
        static Manager singleton;
        return &singleton;
    }
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
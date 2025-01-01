#pragma once
#include "Lib/Singleton.h"
#include "Application/Model.h"

class Manager : public Singleton<Manager>{
    const char* lastSave = "NEW";
    void SetInventoryBaseModel(RE::TESObjectREFR* owner, RE::InventoryEntryData* a_entry);
    void ApplyNewWoldStack(RE::TESForm* base, RefID refid);
    void ApplyNewNonInventoryItem(RE::TESForm* base, RefID refid);
    void ApplyNewQueuedItem(RE::TESForm* base, RefID refid, v_variant variant_vector, int ref_count);

public:
    void ApplyInventoryModel(RE::InventoryEntryData* a1);
    void ApplyNpcSkin(RE::TESObjectREFR* ref);
	void ApplyModelToReference(RE::TESObjectREFR* a_ref);

    void OnItemDrop(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj,
                                          const int32_t a_count);

    void OnItemDrop(RE::ITEM_REMOVE_REASON a_reason, RE::TESObjectREFR* a_this, const RE::TESBoundObject* a_item,
                    const int32_t a_count);

    void OnItemTransfer(RE::TESObjectREFR* a_this, const RE::TESBoundObject* a_item, const int32_t a_count,
                        RE::TESObjectREFR* a_other);

    void OnItemPickup(RE::TESObjectREFR* a_owner, RE::TESObjectREFR* a_obj, const int32_t a_count);
};
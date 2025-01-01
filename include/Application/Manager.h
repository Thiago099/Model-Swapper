#pragma once
#include "Application/InventoryManager.h"
#include "Lib/Singleton.h"
class Manager : public Singleton<Manager>{
    const char* lastSave = "NEW";
    void SetInventoryBaseModel(RE::TESObjectREFR* owner, RE::InventoryEntryData* a_entry);

public:
    void ApplyInventoryModel(RE::InventoryEntryData* a1);
    void ApplyNpcSkin(RE::TESObjectREFR* ref);
	void ApplyModelToReference(RE::TESObjectREFR* a_ref);
};
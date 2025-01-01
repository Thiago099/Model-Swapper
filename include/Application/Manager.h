#pragma once
#include "Application/InventoryManager.h"
#include "Lib/Singleton.h"
class Manager : public Singleton<Manager>{
    const char* lastSave = "NEW";
    void SetInventoryBaseModel(RE::TESObjectREFR* owner, RE::InventoryEntryData* a_entry);

public:
    void PreLoadGame(const std::string& filename);
    void SaveGame(const char* save_name);
    void ApplyInventoryModel(RE::InventoryEntryData* a1);
    void ApplyNpcSkin(RE::TESObjectREFR* ref);
	void ProcessReference(RE::TESObjectREFR* a_ref);
};
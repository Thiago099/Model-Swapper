// https://github.com/digital-apple/CommonLibSSE/blob/a94a3c4ccb993770e89146874b8b8c87fa6e7889/src/RE/B/BGSSaveLoadManager.cpp

#include "SaveLoadManager.h"

namespace RE {
    SaveLoadManager* SaveLoadManager::GetSingleton() {
        REL::Relocation<SaveLoadManager**> singleton{Offset::BGSSaveLoadManager::Singleton};
        return *singleton;
    }

    bool SaveLoadFileEntry::PopulateFileEntryData() {
        using func_t = decltype(&SaveLoadFileEntry::PopulateFileEntryData);
        REL::Relocation<func_t> func{RELOCATION_ID(34627, 35547)};
        return func(this);
    }

    void SaveLoadManager::GenerateCharacterID() {
        using func_t = decltype(&SaveLoadManager::GenerateCharacterID);
        REL::Relocation<func_t> func{RELOCATION_ID(34847, 35757)};
        return func(this);
    }

    void SaveLoadManager::Save(const char* a_fileName) { Save_Impl(2, 0, a_fileName); }

    void SaveLoadManager::Load(const char* a_fileName) { Load_Impl(a_fileName, -1, 0, true); }

    void SaveLoadManager::Load(const char* a_fileName, bool a_checkForMods) {
        Load_Impl(a_fileName, -1, 0, a_checkForMods);
    }

    bool SaveLoadManager::PopulateSaveList() {
        using func_t = decltype(&SaveLoadManager::PopulateSaveList);
        static REL::Relocation<func_t> func{RELOCATION_ID(34850, 35760)};
        return func(this);
    }

    bool SaveLoadManager::LoadMostRecentSaveGame() {
        using func_t = decltype(&SaveLoadManager::LoadMostRecentSaveGame);
        static REL::Relocation<func_t> func{RELOCATION_ID(34856, 35766)};
        return func(this);
    }

    bool SaveLoadManager::Save_Impl(std::int32_t a_deviceID, std::uint32_t a_outputStats, const char* a_fileName) {
        using func_t = decltype(&SaveLoadManager::Save_Impl);
        static REL::Relocation<func_t> func{Offset::BGSSaveLoadManager::Save};
        return func(this, a_deviceID, a_outputStats, a_fileName);
    }

    bool SaveLoadManager::Load_Impl(const char* a_fileName, std::int32_t a_deviceID, std::uint32_t a_outputStats,
                                    bool a_checkForMods) {
        using func_t = decltype(&SaveLoadManager::Load_Impl);
        static REL::Relocation<func_t> func{Offset::BGSSaveLoadManager::Load};
        return func(this, a_fileName, a_deviceID, a_outputStats, a_checkForMods);
    }
}
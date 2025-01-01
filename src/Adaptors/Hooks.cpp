#include "Adaptors/Hooks.h"
#include "Application/Manager.h"

bool Hooks::ReplaceTextureOnObjectsHook::ShouldBackgroundClone(RE::TESObjectREFR* ref) {
    if (ref) {
        Manager::GetSingleton()->ProcessReference(ref);
    }
    return originalFunction(ref);
}

void Hooks::ReplaceTextureOnObjectsHook::Install() {
    originalFunction =
        REL::Relocation<std::uintptr_t>(RE::TESObjectREFR::VTABLE[0]).write_vfunc(0x6D, ShouldBackgroundClone);
}

int64_t Hooks::InventoryHoverHook::thunk(RE::InventoryEntryData* a1) {
    #undef GetObject
    Manager::GetSingleton()->ApplyInventoryModel(a1);
    return originalFunction(a1);
}

bool Hooks::NpcSkinHook::ShouldBackgroundClone(RE::TESObjectREFR* ref) {

    Manager::GetSingleton()->ApplyNpcSkin(ref);

    return originalFunction(ref);
}


RE::BSEventNotifyControl Hooks::SaveHook::ProcessEvent(RE::SaveLoadManager* a_this, const RE::BSSaveDataEvent* a_event, RE::BSTEventSource<RE::BSSaveDataEvent>* a_eventSource)
{
	if (listenSave.load()) {
		listenSave2.store(true);
		listenSave.store(false);
	}
	return originalFunction(a_this, a_event, a_eventSource);
}

void Hooks::SaveHook::PrepareFileSavePath(RE::BSWin32SaveDataSystemUtility* a_this, const char* a_fileName, char* a_dst, bool a_tmpSave, bool a_ignoreINI)
{
    if (listenLoad.load()) {
		lastFile = a_fileName;
    }
	if (listenSave2.load()) {
		listenSave2.store(false);
	    logger::trace("SaveHook::PrepareFileSavePath");
	    logger::info("File name: {}", a_fileName);

        try {
            InventoryManager::GetSingleton()->SerializeData(a_fileName);
        } catch (const std::exception& e) {
            logger::error("Failed to serialize data: {}", e.what());
        }
	}
	originalFunction2(a_this, a_fileName, a_dst, a_tmpSave, a_ignoreINI);
}

void Hooks::InventoryHoverHook::Install() {
    auto& trampoline = SKSE::GetTrampoline();
    trampoline.create(14);
    const REL::Relocation<std::uintptr_t> function{REL::RelocationID(51019, 51897)};
    originalFunction = trampoline.write_call<5>(function.address() + REL::Relocate(0x114, 0x22c), thunk);
}

void Hooks::NpcSkinHook::Install() {
    originalFunction = REL::Relocation<std::uintptr_t>(RE::Character::VTABLE[0]).write_vfunc(0x6D, ShouldBackgroundClone);
}

void Hooks::SaveHook::Install() {
    originalFunction = REL::Relocation<std::uintptr_t>(RE::SaveLoadManager::VTABLE[0]).write_vfunc(0x1, ProcessEvent);
    originalFunction2 = REL::Relocation<std::uintptr_t>(RE::VTABLE_BSWin32SaveDataSystemUtility[0])
                            .write_vfunc(0x2, PrepareFileSavePath);
}

void Hooks::Install() {
    NpcSkinHook::Install();
    ReplaceTextureOnObjectsHook::Install();
    InventoryHoverHook::Install();
    MoveItemHooks<RE::PlayerCharacter>::install();
    MoveItemHooks<RE::TESObjectREFR>::install(false);
    MoveItemHooks<RE::Character>::install();
    SaveHook::Install();
}

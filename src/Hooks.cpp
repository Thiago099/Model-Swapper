#include "Hooks.h"

bool Hooks::ReplaceTextureOnObjectsHook::ShouldBackgroundClone(RE::TESObjectREFR* ref) {
    if (ref) {
        Manager::GetSingleton()->ProcessReference(ref);
    }
    return originalFunction(ref);
}

int64_t Hooks::InventoryHoverHook::thunk(RE::InventoryEntryData* a1) {
    #undef GetObject
    if (const auto ui = RE::UI::GetSingleton(); ui && a1) {
        if (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
			Manager::GetSingleton()->SetInventoryBaseModel(RE::PlayerCharacter::GetSingleton() ,a1);
        }
        else if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
            if (const auto cm = ui->GetMenu<RE::ContainerMenu>()) {
                if (const auto items= cm->GetRuntimeData().itemList) {
                    if (const auto selected = items->GetSelectedItem()) {
                        const auto & data = selected->data;
                        if (const auto owner = RE::TESObjectREFR::LookupByHandle(data.owner).get()) {
                            Manager::GetSingleton()->SetInventoryBaseModel(owner, a1);
                        }
                    }
                }
            }
        }
    }
    return originalFunction(a1);
}

bool Hooks::NpcSkinHook::ShouldBackgroundClone(RE::TESObjectREFR* ref) {

    if (ref) {
        if (const auto obj = ref->GetBaseObject()) {
            if (const auto npc = obj->As<RE::TESNPC>()) {
                
                if (const auto race = npc->race) {
                    if (const auto raceSkin = race->skin) {
                        for (const auto addon : raceSkin->armorAddons) {
                            const auto manager = Manager::GetSingleton();
                            manager->ProcessNew(addon, ref->GetFormID());
                        }
                    }
                }

                if (const auto skin = npc->skin) {
                    for (const auto addon : skin->armorAddons) {
                        const auto manager = Manager::GetSingleton();
                        manager->ProcessNew(addon, ref->GetFormID());
                    }
                }
            }
        }
    }
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
		Manager::GetSingleton()->SaveGame(a_fileName);
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

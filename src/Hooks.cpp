#include "Hooks.h"

bool Hooks::ReplaceTextureOnObjectsHook::ShouldBackgroundClone(RE::TESObjectREFR* ref) {
    if (ref) {
        if (const auto base = ref->GetBaseObject()) {
            const auto manager = Manager::GetSingleton();
            const auto refid = ref->GetFormID();
            if (const auto ref_variant = manager->GetAppliedVariant(refid)) {
                manager->ApplyVariant(base,refid,ref_variant);
            }
            else if (const auto variant = manager->FetchFromQueue(base->GetFormID())) {
				manager->ApplyVariant(base, refid, variant);
            }
            else {
                manager->Process(base, refid);
			}
        }
    }
    return originalFunction(ref);
}

int64_t Hooks::InventoryHoverHook::thunk(RE::InventoryEntryData* a1) {
    #undef GetObject
    if (const auto ui = RE::UI::GetSingleton(); ui && a1) {
        if (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
			Manager::SetInventoryBaseModel(RE::PlayerCharacter::GetSingleton(),a1);
        }
        else if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
            if (const auto cm = ui->GetMenu<RE::ContainerMenu>()) {
                if (const auto items= cm->GetRuntimeData().itemList) {
                    if (const auto selected = items->GetSelectedItem()) {
                        const auto & data = selected->data;
                        if (const auto owner = RE::TESObjectREFR::LookupByHandle(data.owner).get()) {
                            Manager::SetInventoryBaseModel(owner, a1);
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
                            manager->Process(addon, ref->GetFormID());
                        }
                    }
                }

                if (const auto skin = npc->skin) {
                    for (const auto addon : skin->armorAddons) {
                        const auto manager = Manager::GetSingleton();
                        manager->Process(addon, ref->GetFormID());
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
	if (listenSave2.load()) {
		listenSave2.store(false);
	    logger::trace("SaveHook::PrepareFileSavePath");
	    logger::info("File name: {}", a_fileName);
		Manager::GetSingleton()->SaveGame(a_fileName);
	}
	originalFunction2(a_this, a_fileName, a_dst, a_tmpSave, a_ignoreINI);
}
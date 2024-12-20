#include "Hooks.h"
#include "Manager.h"

bool Hooks::ReplaceTextureOnObjectsHook::ShouldBackgroundClone(RE::TESObjectREFR* ref) {
    if (ref) {
        logger::trace("ReplaceTextureOnObjectsHook::ShouldBackgroundClone");
        if (const auto base = ref->GetBaseObject()) {
            const auto manager = Manager::GetSingleton();
            const auto refid = ref->GetFormID();
			logger::trace("Processing {:x}", refid);
            if (const auto ref_variant = manager->GetAppliedVariant(refid)) {
                manager->ApplyVariant(base,refid,ref_variant);
            }
            else if (const auto variant = manager->FetchFromQueue(base->GetFormID())) {
				logger::trace("Fetched from queue");
				manager->ApplyVariant(base, refid, variant);
            }
            else {
			    logger::trace("Processing");
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
			Manager::SetInventoryBaseModel(a1);
        }
        else if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
            // TODO: Container Menu
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

void Hooks::PlayerHook::install()
{
	REL::Relocation<std::uintptr_t> player_character_vtbl{ RE::VTABLE_PlayerCharacter[0] };
    pick_up_object_ = player_character_vtbl.write_vfunc(0xCC, pickUpObject);
    remove_item_ = player_character_vtbl.write_vfunc(0x56, RemoveItem);
}

void Hooks::PlayerHook::pickUpObject(RE::Actor* a_this, RE::TESObjectREFR* a_object, int32_t a_count, bool a_arg3, bool a_play_sound)
{
    if (a_this && a_object && a_object->GetBaseObject()->IsInventoryObject() && a_count>0) {
        Manager::GetSingleton()->UpdateStackOnPickUp(RE::PlayerCharacter::GetSingleton(),a_object,a_count);
    }
    pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
}

RE::ObjectRefHandle Hooks::PlayerHook::RemoveItem(RE::Actor* a_this, RE::TESBoundObject* a_item, std::int32_t a_count, RE::ITEM_REMOVE_REASON a_reason, RE::ExtraDataList* a_extra_list, RE::TESObjectREFR* a_move_to_ref, const RE::NiPoint3* a_drop_loc, const RE::NiPoint3* a_rotate)
{
	const auto manager = Manager::GetSingleton();
	if (a_this && a_item && a_item->IsInventoryObject() && a_count > 0) {
	    if (const auto variant = manager->GetInventoryModel(RE::PlayerCharacter::GetSingleton(), a_item)) {
            manager->AddToQueue(a_item->GetFormID(), variant);
		    logger::info("Added to queue");
	    }
		Manager::GetSingleton()->UpdateStackOnDrop(RE::PlayerCharacter::GetSingleton(),a_item,a_count);
	}

	return remove_item_(a_this, a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
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
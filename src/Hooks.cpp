#include "Hooks.h"
#include "Manager.h"

bool Hooks::ReplaceTextureOnObjectsHook::ShouldBackgroundClone(RE::TESObjectREFR* ref) {
    if (ref) {
        if (const auto base = ref->GetBaseObject(); base && !Manager::GetSingleton()->GetAppliedVariant(ref->GetFormID())) {
            const auto manager = Manager::GetSingleton();
			logger::info("Processing {:x} {:x}", ref->GetFormID(),ref->CreateRefHandle().native_handle());
			if (!manager->HasVariant(ref->GetFormID())) {
			    if (const auto variant = manager->FetchFromQueue()) {
					logger::info("Fetched from queue");
				    manager->ApplyVariant(base, ref->GetFormID(), variant);
                } else manager->Process(base, ref->GetFormID());
			}
        }
    }
    return originalFunction(ref);
}

int64_t Hooks::InventoryHoverHook::thunk(RE::InventoryEntryData* a1) {
    #undef GetObject
    if (auto ui = RE::UI::GetSingleton(); ui && a1) {
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
        if (auto obj = ref->GetBaseObject()) {
            if (auto npc = obj->As<RE::TESNPC>()) {
                if (auto skin = npc->skin) {
                    for (auto addon : skin->armorAddons) {
                        auto manager = Manager::GetSingleton();
                        manager->Process(addon, ref->GetFormID());
                    }
                }
            }
        }
    }
    return originalFunction(ref);
}

void Hooks::player_hook::install()
{
	REL::Relocation<std::uintptr_t> player_character_vtbl{ RE::VTABLE_PlayerCharacter[0] };
    pick_up_object_ = player_character_vtbl.write_vfunc(0xCC, pick_up_object);
    remove_item_ = player_character_vtbl.write_vfunc(0x56, remove_item);
}

void Hooks::player_hook::pick_up_object(RE::Actor* a_this, RE::TESObjectREFR* a_object, uint32_t a_count, bool a_arg3, bool a_play_sound)
{
    if (a_this && a_object && a_object->GetBaseObject()->IsInventoryObject() && a_count>0) {
        Manager::GetSingleton()->UpdateStackOnPickUp(RE::PlayerCharacter::GetSingleton(),a_object,a_count);
    }
    pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
}

RE::ObjectRefHandle Hooks::player_hook::remove_item(RE::Actor* a_this, RE::TESBoundObject* a_item, std::int32_t a_count, RE::ITEM_REMOVE_REASON a_reason, RE::ExtraDataList* a_extra_list, RE::TESObjectREFR* a_move_to_ref, const RE::NiPoint3* a_drop_loc, const RE::NiPoint3* a_rotate)
{
	const auto manager = Manager::GetSingleton();
	if (a_this && a_item && a_item->IsInventoryObject() && a_count > 0) {
	    if (const auto variant = manager->GetInventoryModel(RE::PlayerCharacter::GetSingleton(), a_item)) {
            manager->AddToQueue(variant);
		    logger::info("Added to queue");
	    }
		Manager::GetSingleton()->UpdateStackOnDrop(RE::PlayerCharacter::GetSingleton(),a_item,a_count);
	}

	return remove_item_(a_this, a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
}
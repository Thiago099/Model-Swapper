#include "Application/Manager.h"
#include "Application/ModelSwapManager.h"

#include "Adaptors/Serialization.h"

#include <ranges>

void Manager::SetInventoryBaseModel(RE::TESObjectREFR* owner, RE::InventoryEntryData* a_entry) {
    if (const auto base = a_entry->GetObject()) {
        if (const auto variant = InventoryManager::GetSingleton()->GetInventoryModel(owner, base)) {
            auto modelSwapManger = ModelSwapManager::GetSingleton();

            modelSwapManger->Apply(base, variant);

            if (const auto inv = RE::Inventory3DManager::GetSingleton()) {
                if (!inv->GetRuntimeData().loadedModels.empty()) {
                    inv->Clear3D();
                    inv->GetRuntimeData().loadedModels.clear();
                    inv->UpdateItem3D(a_entry);
                }
            }
        }
    }
}

void Manager::ApplyInventoryModel(RE::InventoryEntryData* a1) {
    if (const auto ui = RE::UI::GetSingleton(); ui && a1) {
        if (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
            SetInventoryBaseModel(RE::PlayerCharacter::GetSingleton(), a1);
        } else if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
            if (const auto cm = ui->GetMenu<RE::ContainerMenu>()) {
                if (const auto items = cm->GetRuntimeData().itemList) {
                    if (const auto selected = items->GetSelectedItem()) {
                        const auto& data = selected->data;
                        if (const auto owner = RE::TESObjectREFR::LookupByHandle(data.owner).get()) {
                            SetInventoryBaseModel(owner, a1);
                        }
                    }
                }
            }
        }
    }
}

void Manager::ApplyNpcSkin(RE::TESObjectREFR* ref) {
    if (ref) {
        if (const auto obj = ref->GetBaseObject()) {
            if (const auto npc = obj->As<RE::TESNPC>()) {
                if (const auto race = npc->race) {
                    if (const auto raceSkin = race->skin) {
                        for (const auto addon : raceSkin->armorAddons) {
                            const auto manager = ModelSwapManager::GetSingleton();
                            auto id = manager->Process(addon, ref->GetFormID());
                            manager->Apply(obj, id);
                        }
                    }
                }

                if (const auto skin = npc->skin) {
                    for (const auto addon : skin->armorAddons) {
                        const auto manager = ModelSwapManager::GetSingleton();
                        auto id = manager->Process(addon, ref->GetFormID());
                        manager->Apply(obj, id);
                    }
                }
            }
        }
    }
}

void Manager::ApplyModelToReference(RE::TESObjectREFR* a_ref)
{
    const auto refid = a_ref->GetFormID();
	const auto base = a_ref->GetBaseObject();
    auto ref_count = a_ref->extraList.GetCount();
	ref_count = ref_count > 0 ? ref_count : 1;

	if (!base) {
		logger::warn("Base object not found for refid: {:x}", refid);
		return;
	}

	if (base->IsInventoryObject()) {
        auto invManager = InventoryManager::GetSingleton();
        invManager->ProcessReference(a_ref);
    } else {
        auto modelSwapManger = ModelSwapManager::GetSingleton();
        auto id = modelSwapManger->Process(base, refid);
        if (id != -1) {
            modelSwapManger->Apply(base, id);
		}
	}
}

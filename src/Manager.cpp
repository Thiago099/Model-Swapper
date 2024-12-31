#include "Manager.h"
#include "Serialization.h"
#include <ranges>
#include "ModelSwapManager.h"

void Manager::PreLoadGame(const std::string& filename) {
	logger::info("PreLoadGame started. Filename: {}", filename.c_str());

	const auto file_path = Serialization::serialization_path + filename;
	try {
	    InventoryManager::GetSingleton()->LoadSerializedData(file_path.c_str());
	}
	catch (const std::exception& e) {
		logger::error("Failed to load data: {}", e.what());
	}
	logger::info("PreLoadGame completed");
}

void Manager::SaveGame(const char* save_name) {

	try {
        InventoryManager::GetSingleton()->SerializeData(save_name);
	}
	catch (const std::exception& e) {
		logger::error("Failed to serialize data: {}", e.what());
	}
}

void Manager::ApplyInventoryModel(RE::InventoryEntryData* a1) {
    if (const auto ui = RE::UI::GetSingleton(); ui && a1) {
        if (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
            InventoryManager::GetSingleton()->SetInventoryBaseModel(RE::PlayerCharacter::GetSingleton(), a1);
        } else if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
            if (const auto cm = ui->GetMenu<RE::ContainerMenu>()) {
                if (const auto items = cm->GetRuntimeData().itemList) {
                    if (const auto selected = items->GetSelectedItem()) {
                        const auto& data = selected->data;
                        if (const auto owner = RE::TESObjectREFR::LookupByHandle(data.owner).get()) {
                            InventoryManager::GetSingleton()->SetInventoryBaseModel(owner, a1);
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
void Manager::ProcessReference(RE::TESObjectREFR* a_ref)
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

#include "Application/Manager.h"
#include "Application/ApplicationUtils.h"
#include "Application/ModelSwapManager.h"
#include "Application/DropQueueManager.h"
#include "Adaptors/Serialization.h"
#include "Application/WorldStackManager.h"
#include <ranges>
#include "Application/InventoryManager.h"

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

void Manager::ApplyNewWoldStack(RE::TESForm* base, RefID refid) {
    auto worldStack = WorldStackManager::GetSingleton();
    auto modelSwap = ModelSwapManager::GetSingleton();
    auto id = modelSwap->Process(base, refid);
    if (id != -1) {
        logger::trace("Found variant");
        modelSwap->Apply(base, id);
        worldStack->Add(refid, id);
    }
    #ifndef NDEBUG
        else {
            logger::warn("No variant found for refid: {:x}", refid);
        }
    #endif
}

void Manager::ApplyNewNonInventoryItem(RE::TESForm* base, RefID refid) {
    logger::trace("other stuff");

    auto modelSwapManger = ModelSwapManager::GetSingleton();
    auto id = modelSwapManger->Process(base, refid);
    modelSwapManger->Apply(base, id);

    #ifndef NDEBUG

    if (id != -1) {
        logger::trace("Model applied to refid: {:x}", refid);
    } else {
        logger::trace("Model applied");
    }

    #endif
}

void Manager::ApplyNewQueuedItem(RE::TESForm* base, RefID refid, v_variant variant_vector, int ref_count) {
    auto worldStack = WorldStackManager::GetSingleton();
    auto modelSwap = ModelSwapManager::GetSingleton();
    auto top_stack = ApplicationUtils::GetTopOfStack(variant_vector, ref_count);
    top_stack = top_stack.empty() ? std::vector<int32_t>(ref_count) : top_stack;
    if (top_stack.back() == -1) {
        auto id = modelSwap->Process(base, refid);
        if (id != -1) {
            modelSwap->Apply(base, id);
            worldStack->Add(refid, id);
        }
    } else {
        modelSwap->Apply(base, top_stack.back());
        worldStack->Set(refid, top_stack);
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
        logger::trace("inv object");
        auto dropQueueManager = DropQueueManager::GetSingleton();
        auto worldStack = WorldStackManager::GetSingleton();
        auto modelSwap = ModelSwapManager::GetSingleton();

        if (auto ref_variant = worldStack->GetByReference(refid); ref_variant.size() > 0) {
            logger::trace("Already applied");
            modelSwap->Apply(base, ref_variant.back());
        } 
        if (auto variant_vector = dropQueueManager->GetNextItemFromDropQueue(base->GetFormID()); !variant_vector.empty()) {
            logger::trace("Queued");
            ApplyNewQueuedItem(base, refid, variant_vector, ref_count);
        }
        else
        {
            logger::trace("None");
            ApplyNewWoldStack(base, refid);
        }
    } 
    else 
    {
        ApplyNewNonInventoryItem(base, refid); 
	}
}

#include "Application/EventHandler.h"
#include "Application/ApplicationUtils.h"
#include "Application/ModelSwapManager.h"
#include "Application/DropQueueManager.h"
#include "Adaptors/Serialization.h"
#include "Application/WorldStackManager.h"
#include <ranges>
#include "Application/InventoyStackManager.h"

void EventHandler::SetInventoryBaseModel(RE::TESObjectREFR* owner, RE::InventoryEntryData* a_entry) {
    if (const auto base = a_entry->GetObject()) {
        if (const auto variant = InventoyStackManager::GetSingleton()->GetInventoryModel(owner, base)) {
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

void EventHandler::ApplyNewWoldStack(RE::TESForm* base, RefID refid) {
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

void EventHandler::ApplyNewNonInventoryItem(RE::TESForm* base, RefID refid) {
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

void EventHandler::ApplyNewQueuedItem(RE::TESForm* base, RefID refid, v_variant variant_vector, int ref_count) {
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



void EventHandler::OnInventoryHover(RE::InventoryEntryData* a1) {
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

void EventHandler::OnNpcLoad(RE::TESObjectREFR* ref) {
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

void EventHandler::OnGenericLoadEvent(RE::TESObjectREFR* a_ref)
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
        if (auto variant_vector = dropQueueManager->Get(base->GetFormID()); !variant_vector.empty()) {
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

void EventHandler::OnItemDrop(RE::TESObjectREFR* a_owner, const RE::TESBoundObject* a_obj, const int32_t a_count) {
    if (auto variants = InventoyStackManager::GetSingleton()->GetAllInventoryModels(a_owner, a_obj, a_count); !variants.empty()) {
        DropQueueManager::GetSingleton()->Add(a_obj->GetFormID(), variants);
    }
    InventoyStackManager::GetSingleton()->Remove(a_owner, a_obj, a_count);
}

void EventHandler::OnItemDrop(RE::ITEM_REMOVE_REASON a_reason, RE::TESObjectREFR* a_this, const RE::TESBoundObject* a_item,
                         const int32_t a_count) {
    if (a_reason == RE::ITEM_REMOVE_REASON::kDropping) {
        OnItemDrop(a_this, a_item, a_count);
    } else {
        InventoyStackManager::GetSingleton()->Remove(a_this, a_item, a_count);
    }
}

void EventHandler::OnItemTransfer(RE::TESObjectREFR* a_this, const RE::TESBoundObject* a_item, const int32_t a_count,
                             RE::TESObjectREFR* a_other) {
    auto inv_variants = InventoyStackManager::GetSingleton()->GetAllInventoryModels(a_this, a_item, a_count);
    InventoyStackManager::GetSingleton()->Remove(a_this, a_item, a_count);
    InventoyStackManager::GetSingleton()->AddMultiple(a_other, a_item, a_count, inv_variants);
}

void EventHandler::OnItemPickup(RE::TESObjectREFR* a_owner, RE::TESObjectREFR* a_obj, const int32_t a_count) {
    auto worldStack = WorldStackManager::GetSingleton();
    auto inventoryManager = InventoyStackManager::GetSingleton();

    inventoryManager->Sync(a_owner);

    const auto base = a_obj->GetBaseObject();
    const auto obj_refid = a_obj->GetFormID();

    auto& wo_stack = worldStack->GetByReference(obj_refid);

    inventoryManager->AddMultiple(a_owner, base, a_count, wo_stack);

    WorldStackManager::GetSingleton()->Remove(base->GetFormID(), obj_refid);
}

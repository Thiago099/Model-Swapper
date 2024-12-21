#pragma once
#include "SaveLoadManager.h"
#include "Manager.h"

namespace Hooks {

    struct ReplaceTextureOnObjectsHook {

        static inline REL::Relocation<bool(RE::TESObjectREFR*)>originalFunction;
        static bool ShouldBackgroundClone(RE::TESObjectREFR* ref);

        static void Install() {
            originalFunction = REL::Relocation<std::uintptr_t>(RE::TESObjectREFR::VTABLE[0]).write_vfunc(0x6D, ShouldBackgroundClone);
        }
    };

    struct InventoryHoverHook {
        static int64_t thunk(RE::InventoryEntryData* a1);
        static inline REL::Relocation<decltype(thunk)> originalFunction;
        static void Install() {
            auto& trampoline = SKSE::GetTrampoline();
            trampoline.create(14);
            const REL::Relocation<std::uintptr_t> function{REL::RelocationID(51019, 51897)};
            originalFunction = trampoline.write_call<5>(function.address() + REL::Relocate(0x114, 0x22c), thunk);
        }
    };


     struct NpcSkinHook {
        static inline REL::Relocation<bool(RE::TESObjectREFR*)> originalFunction;

        static bool ShouldBackgroundClone(RE::TESObjectREFR* ref);

        static void Install() {
            originalFunction = REL::Relocation<std::uintptr_t>(RE::Character::VTABLE[0]).write_vfunc(0x6D, ShouldBackgroundClone);
        }
    };

    struct SaveHook {


        static inline REL::Relocation<RE::BSEventNotifyControl(RE::SaveLoadManager*,const RE::BSSaveDataEvent*, RE::BSTEventSource<RE::BSSaveDataEvent>*)> originalFunction;
		static inline REL::Relocation<errno_t(RE::BSWin32SaveDataSystemUtility*, const char*, char*, bool, bool)> originalFunction2;

		static RE::BSEventNotifyControl ProcessEvent(RE::SaveLoadManager* a_this, const RE::BSSaveDataEvent* a_event, RE::BSTEventSource<RE::BSSaveDataEvent>* a_eventSource);
        static void PrepareFileSavePath(RE::BSWin32SaveDataSystemUtility* a_this,const char* a_fileName, char* a_dst, bool a_tmpSave, bool a_ignoreINI);

        static void Install() {
            originalFunction = REL::Relocation<std::uintptr_t>(RE::SaveLoadManager::VTABLE[0]).write_vfunc(0x1, ProcessEvent);
            originalFunction2 = REL::Relocation<std::uintptr_t>(RE::VTABLE_BSWin32SaveDataSystemUtility[0]).write_vfunc(0x2, PrepareFileSavePath);
        }
    };
    inline std::atomic<bool> listenSave = false;
    inline std::atomic<bool> listenSave2 = false;
    inline std::atomic<bool> listenLoad = true;
    inline std::string lastFile;
	inline std::atomic<std::string*> lastFile_ptr{ &lastFile };


    template <typename RefType>
    class MoveItemHooks {
    public:
        static void install(const bool is_actor = true) {
			REL::Relocation<std::uintptr_t> _vtbl{ RefType::VTABLE[0] };
			if (is_actor) {
			    pick_up_object_ = _vtbl.write_vfunc(0xCC, pickUpObject);
			}
			remove_item_ = _vtbl.write_vfunc(0x56, RemoveItem);
			add_object_to_container_ = _vtbl.write_vfunc(0x5A, addObjectToContainer);
        }

    private:
        static void pickUpObject(RefType* a_this,
                                   RE::TESObjectREFR* a_object,
                                   int32_t a_count,
                                   bool a_arg3,
                                   bool a_play_sound);
        static inline REL::Relocation<decltype(pickUpObject)> pick_up_object_;

        static RE::ObjectRefHandle* RemoveItem(RefType* a_this,
            RE::ObjectRefHandle& a_hidden_return_argument,
            RE::TESBoundObject* a_item,
            std::int32_t a_count,
            RE::ITEM_REMOVE_REASON a_reason,
            RE::ExtraDataList* a_extra_list,
            RE::TESObjectREFR* a_move_to_ref,
            const RE::NiPoint3* a_drop_loc,
            const RE::NiPoint3* a_rotate);
        static inline REL::Relocation<decltype(RemoveItem)> remove_item_;

        static void addObjectToContainer(RefType* a_this,
                                    RE::TESBoundObject* a_object, 
                                    RE::ExtraDataList* a_extraList, 
                                    std::int32_t a_count,
                                    RE::TESObjectREFR* a_fromRefr
                                );
        static inline REL::Relocation<decltype(addObjectToContainer)> add_object_to_container_;
    };

    inline void Install() {
        NpcSkinHook::Install();
        ReplaceTextureOnObjectsHook::Install();
		InventoryHoverHook::Install();
		MoveItemHooks<RE::PlayerCharacter>::install();
		MoveItemHooks<RE::TESObjectREFR>::install(false);
		MoveItemHooks<RE::Character>::install();
		SaveHook::Install();
    }
}


template <typename RefType>
void Hooks::MoveItemHooks<RefType>::pickUpObject(RefType* a_this, RE::TESObjectREFR* a_object, int32_t a_count,
    bool a_arg3, bool a_play_sound) {

    if (!a_this || !a_object || !a_object->GetBaseObject() || !a_object->GetBaseObject()->IsInventoryObject() || a_count <= 0) {
        return pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
	}
    Manager::GetSingleton()->OnItemPickup(a_this,a_object,a_count);
    pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
}

template <typename RefType>
RE::ObjectRefHandle* Hooks::MoveItemHooks<RefType>::RemoveItem(RefType* a_this, RE::ObjectRefHandle& a_hidden_return_argument,
                                                               RE::TESBoundObject* a_item, std::int32_t a_count, RE::ITEM_REMOVE_REASON a_reason, RE::ExtraDataList* a_extra_list,
                                                               RE::TESObjectREFR* a_move_to_ref, const RE::NiPoint3* a_drop_loc, const RE::NiPoint3* a_rotate)
{

    if (!a_this || !a_item || !a_item->IsInventoryObject() || a_count <= 0) {
		return remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
	}

	auto manager = Manager::GetSingleton();
	if (!a_move_to_ref) {
		if (a_reason == RE::ITEM_REMOVE_REASON::kDropping) {
		    manager->OnItemDrop(a_this,a_item,a_count);
        }
        else {
            manager->UpdateStackOnRemove(a_this,a_item,a_count);
        }
	}
	else {
		const auto inv_variants = manager->GetInventoryModels(a_this, a_item,a_count);
		manager->UpdateStackOnRemove(a_this, a_item, a_count);
		manager->UpdateStackOnAdd(a_move_to_ref, a_item, a_count, inv_variants);
	}

	return remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
}

template<typename RefType>
void Hooks::MoveItemHooks<RefType>::addObjectToContainer(RefType* a_this, RE::TESBoundObject* a_object, RE::ExtraDataList* a_extraList, std::int32_t a_count, RE::TESObjectREFR* a_fromRefr)
{
	if (a_fromRefr || !a_this || !a_object || !a_object->IsInventoryObject() || a_count <= 0) {
		return add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);
	}
	std::vector<const variant*> variants;
	for (int i = 0; i < a_count; ++i) {
		variants.push_back(nullptr);
	}
	Manager::GetSingleton()->UpdateStackOnAdd(a_this,a_object,a_count,variants);

	return add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);
}

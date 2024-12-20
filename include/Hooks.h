#pragma once
#include "SaveLoadManager.h"

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


    class PlayerHook {
    public:
        static void install();
    private:
        static void pickUpObject(RE::Actor* a_this,
                                   RE::TESObjectREFR* a_object,
                                   uint32_t a_count,
                                   bool a_arg3,
                                   bool a_play_sound);
        static inline REL::Relocation<decltype(pickUpObject)> pick_up_object_;

        static RE::ObjectRefHandle RemoveItem(RE::Actor* a_this,
            RE::TESBoundObject* a_item,
            std::int32_t a_count,
            RE::ITEM_REMOVE_REASON a_reason,
            RE::ExtraDataList* a_extra_list,
            RE::TESObjectREFR* a_move_to_ref,
            const RE::NiPoint3* a_drop_loc,
            const RE::NiPoint3* a_rotate);
        static inline REL::Relocation<decltype(RemoveItem)> remove_item_;
    };

    

    inline void Install() {
        NpcSkinHook::Install();
        ReplaceTextureOnObjectsHook::Install();
		InventoryHoverHook::Install();
		PlayerHook::install();
		SaveHook::Install();
    }
}
#pragma once
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <mutex>


inline std::mutex mtx;

inline size_t GetModuleSize(HMODULE module) {
    MODULEINFO moduleInfo;
    if (GetModuleInformation(GetCurrentProcess(), module, &moduleInfo, sizeof(moduleInfo))) {
        return moduleInfo.SizeOfImage;
    }
    return 0;
}

inline bool IsAddressInSkyrimExe(uintptr_t addr) {
    void* skyrimBase = GetModuleHandle(L"SkyrimSE.exe");
    if (!skyrimBase) return false;

    size_t skyrimSize = GetModuleSize(static_cast<HMODULE>(skyrimBase));
    uintptr_t baseAddr = reinterpret_cast<uintptr_t>(skyrimBase);

    return addr >= baseAddr && addr < (baseAddr + skyrimSize);
}

inline void PrintStackTrace() {
    void* skyrimSEBase = GetModuleHandle(L"SkyrimSE.exe");
    auto skyrimAddress = reinterpret_cast<uintptr_t>(skyrimSEBase);
    const int maxFrames = 64;
    void* stack[maxFrames];
    unsigned short frames = CaptureStackBackTrace(0, maxFrames, stack, NULL);

    for (unsigned short i = 0; i < frames; ++i) {
        auto currentAddress = reinterpret_cast<uintptr_t>(stack[i]);
        if (IsAddressInSkyrimExe(currentAddress)) {
            logger::trace("{:x}", 0x140000000 + currentAddress - skyrimAddress);
        }
    }
}

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


    class player_hook {
    public:
        static void install();
    private:
        static void pick_up_object(RE::Actor* a_this,
                                   RE::TESObjectREFR* a_object,
                                   uint32_t a_count,
                                   bool a_arg3,
                                   bool a_play_sound);
        static inline REL::Relocation<decltype(pick_up_object)> pick_up_object_;

        static RE::ObjectRefHandle remove_item(RE::Actor* a_this,
            RE::TESBoundObject* a_item,
            std::int32_t a_count,
            RE::ITEM_REMOVE_REASON a_reason,
            RE::ExtraDataList* a_extra_list,
            RE::TESObjectREFR* a_move_to_ref,
            const RE::NiPoint3* a_drop_loc,
            const RE::NiPoint3* a_rotate);
        static inline REL::Relocation<decltype(remove_item)> remove_item_;
    };

    

    inline void Install() {
        NpcSkinHook::Install();
        ReplaceTextureOnObjectsHook::Install();
		InventoryHoverHook::Install();
		player_hook::install();
    }
}
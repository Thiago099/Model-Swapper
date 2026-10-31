#pragma once
#include "HookBuilder.h"
#include "Manager.h"
#include "FilterContext.h"
#include <windows.h>
#include <dbghelp.h>
#include <iostream>
#include <psapi.h>
#include <mutex>
inline std::recursive_mutex modelLoadMutex;
size_t GetModuleSize(HMODULE module) {
    MODULEINFO moduleInfo;
    if (GetModuleInformation(GetCurrentProcess(), module, &moduleInfo, sizeof(moduleInfo))) {
        return moduleInfo.SizeOfImage;
    }
    return 0;
}

bool IsAddressInSkyrimExe(uintptr_t addr) {
    void* skyrimBase = GetModuleHandle(L"SkyrimSE.exe");
    if (!skyrimBase) return false;

    size_t skyrimSize = GetModuleSize(static_cast<HMODULE>(skyrimBase));
    uintptr_t baseAddr = reinterpret_cast<uintptr_t>(skyrimBase);

    return addr >= baseAddr && addr < (baseAddr + skyrimSize);
}

void PrintStackTrace() {
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
        static inline REL::Relocation<RE::NiAVObject*(RE::TESObjectREFR*, bool)> originalLoad3D;
        static bool ShouldBackgroundClone(RE::TESObjectREFR* ref) {
            // Keep the original hook behavior for configurations without scoped filters.
            if (ref && !Manager::GetSingleton()->HasScopedRules()) {
                if (auto base = ref->GetBaseObject()) {
                    auto manager = Manager::GetSingleton();
                    manager->Process(base, ref->GetFormID());
                }
            }
            return originalFunction(ref);
        }
        static RE::NiAVObject* Load3D(RE::TESObjectREFR* ref, bool backgroundLoading) {
            auto manager = Manager::GetSingleton();
            if (!ref || !manager->HasScopedRules()) {
                return originalLoad3D(ref, backgroundLoading);
            }

            std::lock_guard<std::recursive_mutex> lock(modelLoadMutex);
            ModelSwapScope scope(ref->GetFormID());
            std::optional<FormFilterContext> context;
            if (manager->HasContextRules()) {
                context = FormFilters::CaptureContext(ref);
            }
            if (auto base = ref->GetBaseObject()) {
                scope.Add(manager->Swap(base, ref->GetFormID(), context ? &*context : nullptr));
            }
            return originalLoad3D(ref, backgroundLoading);
        }
        static void Install() {
            originalFunction = REL::Relocation<std::uintptr_t>(RE::TESObjectREFR::VTABLE[0]).write_vfunc(0x6D, ShouldBackgroundClone);
            originalLoad3D = REL::Relocation<std::uintptr_t>(RE::TESObjectREFR::VTABLE[0]).write_vfunc(0x6A, Load3D);
        }
    };


     struct NpcSkinHook {
        static inline REL::Relocation<bool(RE::TESObjectREFR*)> originalFunction;
        static inline REL::Relocation<RE::NiAVObject*(RE::TESObjectREFR*, bool)> originalLoad3D;

        static bool ShouldBackgroundClone(RE::TESObjectREFR* ref) {

            if (ref && !Manager::GetSingleton()->HasScopedRules()) {
                if (auto obj = ref->GetBaseObject()) {
                    if (auto npc = obj->As<RE::TESNPC>()) {
                        
                        if (auto race = npc->race) {
                            if (auto raceSkin = race->skin) {
                                for (auto addon : raceSkin->armorAddons) {
                                    auto manager = Manager::GetSingleton();
                                    manager->Process(addon, ref->GetFormID());
                                }
                            }
                        }

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
        static RE::NiAVObject* Load3D(RE::TESObjectREFR* ref, bool backgroundLoading) {
            return LoadSkin3D(ref, backgroundLoading, originalLoad3D);
        }
        static RE::NiAVObject* LoadSkin3D(RE::TESObjectREFR* ref, bool backgroundLoading,
                                        const REL::Relocation<RE::NiAVObject*(RE::TESObjectREFR*, bool)>& original) {
            auto manager = Manager::GetSingleton();
            if (!ref || !manager->HasScopedRules()) {
                return original(ref, backgroundLoading);
            }

            std::lock_guard<std::recursive_mutex> lock(modelLoadMutex);
            ModelSwapScope scope(ref->GetFormID());
            std::optional<FormFilterContext> context;
            if (manager->HasContextRules()) {
                context = FormFilters::CaptureContext(ref);
            }
            if (auto obj = ref->GetBaseObject()) {
                if (auto npc = obj->As<RE::TESNPC>()) {
                    std::set<RE::TESObjectARMA*> addons;
                    auto swapSkin = [&](RE::TESObjectARMO* skin) {
                        if (!skin) {
                            return;
                        }
                        for (auto addon : skin->armorAddons) {
                            if (addon && addons.insert(addon).second) {
                                scope.Add(manager->Swap(addon, ref->GetFormID(), npc->GetFormID(), context ? &*context : nullptr));
                            }
                        }
                    };
                    if (auto race = npc->race) {
                        swapSkin(race->skin);
                    }
                    swapSkin(npc->skin);
                }
            }
            return original(ref, backgroundLoading);
        }
        static void Install() {
            originalFunction = REL::Relocation<std::uintptr_t>(RE::Character::VTABLE[0]).write_vfunc(0x6D, ShouldBackgroundClone);
            originalLoad3D = REL::Relocation<std::uintptr_t>(RE::Character::VTABLE[0]).write_vfunc(0x6A, Load3D);
        }
    };

    struct PlayerSkinHook {
        static inline REL::Relocation<RE::NiAVObject*(RE::TESObjectREFR*, bool)> originalLoad3D;
        static RE::NiAVObject* Load3D(RE::TESObjectREFR* ref, bool backgroundLoading) {
            return NpcSkinHook::LoadSkin3D(ref, backgroundLoading, originalLoad3D);
        }
        static void Install() {
            originalLoad3D = REL::Relocation<std::uintptr_t>(RE::PlayerCharacter::VTABLE[0]).write_vfunc(0x6A, Load3D);
        }
    };

    

    inline void Install() {
        NpcSkinHook::Install();
        PlayerSkinHook::Install();
        ReplaceTextureOnObjectsHook::Install();
    }
}

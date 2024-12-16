#pragma once
#include "HookBuilder.h"
#include "Manager.h"
#include <windows.h>
#include <dbghelp.h>
#include <iostream>
#include <psapi.h>
inline std::mutex mtx;
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
        static bool ShouldBackgroundClone(RE::TESObjectREFR* ref) {
            if (ref) {
                if (auto base = ref->GetBaseObject()) {
                    auto manager = Manager::GetSingleton();
                    manager->Process(base, ref->GetFormID());
                }
            }
            return originalFunction(ref);
        }
        static void Install() {
            originalFunction = REL::Relocation<std::uintptr_t>(RE::TESObjectREFR::VTABLE[0]).write_vfunc(0x6D, ShouldBackgroundClone);
        }
    };


     struct NpcSkinHook {
        static inline REL::Relocation<bool(RE::TESObjectREFR*)> originalFunction;

        static bool ShouldBackgroundClone(RE::TESObjectREFR* ref) {

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
        static void Install() {
            originalFunction = REL::Relocation<std::uintptr_t>(RE::Character::VTABLE[0]).write_vfunc(0x6D, ShouldBackgroundClone);
        }
    };

    

    inline void Install() {
        NpcSkinHook::Install();
        ReplaceTextureOnObjectsHook::Install();
    }
}
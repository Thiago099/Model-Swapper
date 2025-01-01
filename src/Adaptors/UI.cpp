#include "Adaptors/UI.h"
#include "Lib/SKSEMenuFramework.h"
#include "Application/Manager.h"
#include "Application/InventoryManager.h"
#include "Application/WorldStackManager.h"

void UI::Install() {

    if (!SKSEMenuFramework::IsInstalled()) {
        return;
    }
    SKSEMenuFramework::SetSection("Model Swapper");
    SKSEMenuFramework::AddSectionItem("Inventory Stacks", InventoryStacks::Render);
    SKSEMenuFramework::AddSectionItem("Queue", Queue::Render);
    SKSEMenuFramework::AddSectionItem("World Stacks", WorldStacks::Render);
}



void __stdcall UI::InventoryStacks::Render() {

    auto manager = InventoryManager::GetSingleton();
    for (auto &[key, value] : manager->GetAll()) {
        if (value.size() == 0) {
            continue;
        }

        bool found = false;

        for (auto [key2, value2] : value) {
        if (value2.size() > 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            continue;
        }

        if (auto form = RE::TESForm::LookupByID<RE::TESObjectREFR>(key)) {
            if (auto base = form->GetBaseObject()) {
                ImGui::Text(std::format("Container: {}", base->GetName()).c_str());
            } else {
                ImGui::Text(std::format("Container: {:x}", key).c_str());
            }
        } else {
            ImGui::Text(std::format("Container: {:x}", key).c_str());
        }
        for (auto [key2, value2] : value) {
            if (auto form = RE::TESForm::LookupByID(key2)) {
                ImGui::Text(std::format("    BaseForm: {}", form->GetName()).c_str());
            } else {
                ImGui::Text(std::format("    BaseForm: {:x}", key2).c_str());
            }
            for (auto &item : value) {
                for (auto variant : item.second) {
                    ImGui::Text(std::format("            Variant: {}", variant).c_str());
                }
            }

        }
	}

}

void __stdcall UI::Queue::Render() {
    auto manager = InventoryManager::GetSingleton();

    for (auto [key, value] : manager->GetQueue()) {
        ImGui::Text(std::format("Form: {:x}", key).c_str());
        for (auto item : value) {
            ImGui::Text(std::format("    Form: {}", item).c_str());
        }
    }
}


void __stdcall UI::WorldStacks::Render() {
    auto manager = WorldStackManager::GetSingleton();

    for (auto [key, value] : manager->GetAll()) {
        if (value.size() == 0) {
            continue;
        }
        if (auto form = RE::TESForm::LookupByID<RE::TESObjectREFR>(key)) {
            if (auto base = form->GetBaseObject()) {
                ImGui::Text(std::format("Container: {}", base->GetName()).c_str());
            } else {
                ImGui::Text(std::format("Container: {:x}", key).c_str());
            }
        } else {
            ImGui::Text(std::format("Container: {:x}", key).c_str());
        }
        for (auto item : value) {
            ImGui::Text(std::format("    Variant: {}", item).c_str());
        
        }
    }
}
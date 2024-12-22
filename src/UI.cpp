#include "UI.h"

void UI::Install() {

    if (!SKSEMenuFramework::IsInstalled()) {
        return;
    }
    SKSEMenuFramework::SetSection("Model Swapper");
    SKSEMenuFramework::AddSectionItem("Inventory Stacks", InventoryStacks::Render);
    SKSEMenuFramework::AddSectionItem("Queue", Queue::Render);
}



void __stdcall UI::InventoryStacks::Render() {

    auto manager = Manager::GetSingleton();
    for (auto &[key, value] : manager->inventory_stacks) {
        ImGui::Text(std::format("    Container: {:x}", key).c_str());
        for (auto [key2, value2] : value) {
            ImGui::Text(std::format("        BaseForm: {:x}", key2).c_str());
            for (auto &item : value) {
                for (auto variant : item.second) {
                    if (variant) {
                        ImGui::Text(std::format("            Model: {}", variant->model).c_str());
                    }
                }
            }

        }
	}

}

void __stdcall UI::Queue::Render() {
    auto manager = Manager::GetSingleton();
    for (auto [key, value] : manager->variants_queue) {
        ImGui::Text(std::format("Form: {:x}", key).c_str());
        for (auto item : value) {
            ImGui::Text(std::format("    Form: {}", item->model).c_str());
        }
    }
}

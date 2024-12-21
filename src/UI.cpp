#include "UI.h"

void UI::Install() {

    if (!SKSEMenuFramework::IsInstalled()) {
        return;
    }
    SKSEMenuFramework::SetSection("Model Swapper");
    SKSEMenuFramework::AddSectionItem("Model Swapper", Example1::Render);
}



void __stdcall UI::Example1::Render() {

    auto manager = Manager::GetSingleton();
    ImGui::Text("Inventory stacks");
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
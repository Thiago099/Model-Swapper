#pragma once
#include "SKSEMenuFramework.h"
#include "Manager.h"

namespace UI {
    void Install();
    namespace InventoryStacks {
        void __stdcall Render();
    }
    namespace Queue {
        void __stdcall Render();
    }

    namespace WorldStacks {
        void __stdcall Render();
    }
};
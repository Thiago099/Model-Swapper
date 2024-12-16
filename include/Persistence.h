#pragma once

#include "Manager.h"
#include "String.h"
#include "Config.h"
#include "Ini.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;



namespace Persistence {
    void Install();
}
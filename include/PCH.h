#pragma once

#include <spdlog/sinks/basic_file_sink.h>

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

namespace logger = SKSE::log;
using namespace std::literals;

using FormID = RE::FormID;
using RefID = RE::FormID;

constexpr RefID player_refid = 20;

constexpr std::uint32_t kDataKey = 'MDLS';


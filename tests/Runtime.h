#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <list>
#include <memory>
#include <optional>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace logger {
    template <class... Args> void trace(Args&&...) {}
    template <class... Args> void error(Args&&...) {}
    template <class... Args> void info(Args&&...) {}
}

// Only model/form behavior is simulated; selection and registration use the
// production headers and Manager.cpp.
namespace RE {
    using FormID = std::uint32_t;

    class BSFixedString {
        std::optional<std::string> value;
    public:
        BSFixedString() = default;
        BSFixedString(const char* text) { if (text) value = text; }
        BSFixedString& operator=(const char* text) {
            if (text) value = text;
            else value.reset();
            return *this;
        }
        const char* c_str() const { return value ? value->c_str() : nullptr; }
    };

    class TESModel {
        BSFixedString path;
    public:
        const char* GetModel() const { return path.c_str(); }
        void SetModel(const char* value) { path = value; }
        virtual ~TESModel() = default;
    };

    class TESForm {
    public:
        FormID formID = 0;
        virtual ~TESForm() = default;
        FormID GetFormID() const { return formID; }
        template <class T> T* As() { return dynamic_cast<T*>(this); }
    };

    class TESBoundObject : public TESForm, public TESModel {};

    namespace SEXES {
        constexpr std::size_t kMale = 0;
        constexpr std::size_t kFemale = 1;
    }

    class TESObjectARMA : public TESForm {
    public:
        TESModel bipedModels[2];
        TESModel bipedModel1stPersons[2];
    };

    class TESFile {
        std::uint8_t index;
        bool light;
    public:
        TESFile(std::uint8_t compileIndex, bool isLight) : index(compileIndex), light(isLight) {}
        std::uint8_t GetCompileIndex() const { return index; }
        bool IsLight() const { return light; }
    };

    class TESDataHandler {
    public:
        static TESDataHandler* GetSingleton() { static TESDataHandler value; return &value; }
        TESFile* LookupModByName(std::string_view plugin) {
            static TESFile main{1, false}, light{0xFE, true}, inactive{0xFF, false};
            if (plugin == "Example.esp") return &main;
            if (plugin == "Light.esl") return &light;
            if (plugin == "Inactive.esp") return &inactive;
            return nullptr;
        }
        FormID LookupFormID(FormID local, std::string_view plugin) {
            if (plugin == "Example.esp") return 0x01000000 | local;
            if (plugin == "Light.esl") return 0xFE001000 | local;
            return 0;
        }
    };
}

namespace REL {
    template <class T> class Relocation {
    public:
        explicit Relocation(std::uint64_t) {}
        template <class... Args> std::int64_t operator()(Args&&...) const { return 0; }
    };
}

#define RELOCATION_ID(se, ae) se

#pragma once

#include <filesystem>
#include <functional>

#include "SimpleIni.h"

namespace fs = std::filesystem;

namespace Ini {
    inline std::string CommentString(const std::string& originalString) {
        std::stringstream ss(originalString);
        std::string currentLine;
        std::string modifiedString;
        while (std::getline(ss, currentLine, '\n')) {
            modifiedString += "; " + currentLine;
        }
        return modifiedString;
    }
    class IniWriter {
    private:
        std::string current_section;
        std::map<std::string, std::string> sections;
        std::list<std::string> sorted_keys;

    public:
        void SetSection(const std::string& section) {
            if (const auto it = sections.find(section); it == sections.end()) {
                sections[section] = "";
                sorted_keys.push_back(section);
            }
            current_section = section;
        }
        template <class E, class T>
        void Add(T key, E value) {
            sections[current_section] += std::format("{} = {}\n", key, value);
        }
        void Comment(const std::string& value) { sections[current_section] += CommentString(value) + "\n\n"; }
        std::string str() {
            std::string result = "";
            for (const auto& section : sorted_keys) {
                result += std::format("\n[{}]\n\n{}", section, sections[section]);
            }
            logger::trace("{}", result);
            return result;
        }
        void Write(const std::string& fileName) {
            const fs::path appPath = std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / fileName;
            fs::create_directories(appPath.parent_path());

            logger::trace("write file start {}", appPath.string().c_str());
            std::ofstream file(appPath, std::ios::out | std::ios::trunc);

            if (!file) {
                logger::error("Failed to open file.");
                return;
            }

            file << str();

            logger::trace("write file end {}", appPath.string().c_str());
        }
    };

    class IniReader {
    private:
        CSimpleIniA _ini;
        const char* _section;
        bool opened = false;

        const char* GetValue(const char* key) const {
            if (!opened) return "";
            return _ini.GetValue(_section, key);
        }

    public:
        IniReader(const char* filaname) {
            _ini.SetUnicode();
            opened = _ini.LoadFile(("Data/SKSE/Plugins/" + std::string(filaname)).c_str()) >= 0;
        }

        void GetKeyPair(const char* section, std::function<void(const char*, const char*)> const& fn) const;
        void SetSection(const char* section) { _section = section; }
        const char* GetString(const char* key, const char* def = "") const { return _ini.GetValue(_section, key, def); }
        float GetFloat(const char* key, const float def = 0.0f) const {
            return static_cast<float>(_ini.GetDoubleValue(_section, key, def));
        }
        int GetInt(const char* key, const int def = 0) const {
            return static_cast<int>(_ini.GetLongValue(_section, key, def));
        }
        bool GetBool(const char* key, const bool def = false) const { return _ini.GetBoolValue(_section, key, def); }
    };
}
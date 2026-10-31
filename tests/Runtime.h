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

    enum class FormType { None, Location, Region, Keyword, Cell, WorldSpace, BoundObject, ArmorAddon, Reference };

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
        static inline std::map<FormID, TESForm*> registeredForms;
        FormID formID = 0;
        FormType formType = FormType::None;
        virtual ~TESForm() = default;
        FormID GetFormID() const { return formID; }
        FormType GetFormType() const { return formType; }
        template <class T> T* As() { return dynamic_cast<T*>(this); }
        template <class T> const T* As() const { return dynamic_cast<const T*>(this); }
        static TESForm* LookupByID(FormID id) {
            const auto found = registeredForms.find(id);
            return found == registeredForms.end() ? nullptr : found->second;
        }
        template <class T> static T* LookupByID(FormID id) { return dynamic_cast<T*>(LookupByID(id)); }
        static void Register(TESForm& form, FormID id) {
            form.formID = id;
            registeredForms[id] = &form;
        }
    };

    class BGSKeyword : public TESForm {
    public:
        BGSKeyword() { formType = FormType::Keyword; }
    };

    class BGSKeywordForm {
    public:
        std::vector<BGSKeyword*> keywords;
        virtual ~BGSKeywordForm() = default;
        bool HasKeyword(const BGSKeyword* keyword) const {
            return keyword && std::find(keywords.begin(), keywords.end(), keyword) != keywords.end();
        }
        bool HasKeywordID(FormID id) const {
            return std::any_of(keywords.begin(), keywords.end(), [id](const auto* keyword) {
                return keyword && keyword->GetFormID() == id;
            });
        }
        const std::vector<BGSKeyword*>& GetKeywords() const { return keywords; }
    };

    class TESBoundObject : public TESForm, public TESModel, public BGSKeywordForm {
    public:
        TESBoundObject() { formType = FormType::BoundObject; }
    };

    class BGSLocation : public TESForm, public BGSKeywordForm {
    public:
        BGSLocation* parentLoc = nullptr;
        BGSLocation() { formType = FormType::Location; }
        bool IsParent(const BGSLocation* possibleParent) const {
            for (const auto* current = parentLoc; current; current = current->parentLoc) {
                if (current == possibleParent) return true;
            }
            return false;
        }
    };

    class TESRegion : public TESForm {
    public:
        TESRegion() { formType = FormType::Region; }
    };

    using TESRegionList = std::vector<TESRegion*>;

    class TESObjectCELL : public TESForm {
    public:
        TESRegionList* regionList = nullptr;
        TESObjectCELL() { formType = FormType::Cell; }
        TESRegionList* GetRegionList(bool) const { return regionList; }
    };

    class TESWorldSpace : public TESForm {
    public:
        TESWorldSpace* parentWorld = nullptr;
        TESWorldSpace() { formType = FormType::WorldSpace; }
    };

    class TESObjectREFR : public TESForm {
    public:
        TESBoundObject* base = nullptr;
        TESObjectCELL* parentCell = nullptr;
        TESObjectCELL* saveParentCell = nullptr;
        TESWorldSpace* worldspace = nullptr;
        BGSLocation* location = nullptr;
        std::vector<BGSKeyword*> referenceKeywords;
        TESObjectREFR() { formType = FormType::Reference; }
        TESBoundObject* GetBaseObject() const { return base; }
        TESObjectCELL* GetParentCell() const { return parentCell; }
        TESObjectCELL* GetSaveParentCell() const { return saveParentCell; }
        TESWorldSpace* GetWorldspace() const { return worldspace; }
        BGSLocation* GetCurrentLocation() const { return location; }
        bool HasKeyword(const BGSKeyword* keyword) const {
            return (base && base->HasKeyword(keyword)) ||
                (keyword && std::find(referenceKeywords.begin(), referenceKeywords.end(), keyword) != referenceKeywords.end());
        }
    };

    namespace SEXES {
        constexpr std::size_t kMale = 0;
        constexpr std::size_t kFemale = 1;
    }

    class TESObjectARMA : public TESForm {
    public:
        TESObjectARMA() { formType = FormType::ArmorAddon; }
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

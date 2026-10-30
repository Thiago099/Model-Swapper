#pragma once
#include "Str.h"
#include "Wrapper.h"
#include <memory>
#include <set>


class Manager {
    inline static Manager* singleton = nullptr;
    models sources;
    bool hasScopedRules = false;
public:
    static inline Manager* GetSingleton() {
        if (!singleton) {
            singleton = new Manager();
        }
        return singleton;
    }
    void Register(std::string key, variants value);
    bool HasScopedRules() const { return hasScopedRules; }
    std::unique_ptr<AVObject> Swap(RE::TESBoundObject* base, RE::FormID id);
    std::unique_ptr<AVObject> Swap(RE::TESObjectARMA* base, RE::FormID id, RE::FormID baseFormID);
    void Process(RE::TESBoundObject* base, RE::FormID id);
    void Process(RE::TESObjectARMA* base, RE::FormID id);
};

class ModelSwapScope {
    inline static thread_local ModelSwapScope* active = nullptr;
    ModelSwapScope* parent = nullptr;
    ModelSwapScope* owner = nullptr;
    RE::FormID referenceID = 0;
    std::vector<std::unique_ptr<AVObject>> wrappers;
    std::vector<std::unique_ptr<AVObject>> suspended;
public:
    explicit ModelSwapScope(RE::FormID id = 0) : parent(active), owner(this), referenceID(id) {
        if (parent && id != 0 && parent->referenceID == id) {
            // Continuations for the same reference share the outer scope.
            owner = parent;
            return;
        }
        if (parent) {
            // A nested load must select from the baseline, even when it shares
            // the outer reference's base form or skin armor addons.
            for (const auto& wrapper : parent->wrappers) {
                suspended.push_back(wrapper->Snapshot());
            }
            for (auto it = parent->wrappers.rbegin(); it != parent->wrappers.rend(); ++it) {
                (*it)->Reset();
            }
        }
        active = this;
    }
    ModelSwapScope(const ModelSwapScope&) = delete;
    ModelSwapScope& operator=(const ModelSwapScope&) = delete;

    ~ModelSwapScope() {
        if (owner != this) {
            return;
        }
        for (auto it = wrappers.rbegin(); it != wrappers.rend(); ++it) {
            (*it)->Reset();
        }
        for (auto it = suspended.rbegin(); it != suspended.rend(); ++it) {
            (*it)->Reset();
        }
        active = parent;
    }

    void Add(std::unique_ptr<AVObject> wrapper) {
        if (wrapper) {
            owner->wrappers.push_back(std::move(wrapper));
        }
    }
};

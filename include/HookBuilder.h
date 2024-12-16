#pragma once
#include <type_traits>

enum WriteKind { kCall, kBranch };

class GenericHookItem {
public:
    virtual void Install(SKSE::Trampoline& tranpoline) = 0;
    virtual const size_t GetTrampolineSize() const = 0;
};

template <class T, std::size_t N, std::size_t TS, WriteKind Kind>
class HookItem : public GenericHookItem {
    int _ae_id;
    int _ae_offset;
    int _se_id;
    int _se_offset;

public:
    HookItem(int se_id, int se_offset, int ae_id, int ae_offset) {
        _se_id = se_id;
        _ae_id = ae_id;
        _se_offset = se_offset;
        _ae_offset = ae_offset;
    }
    const size_t GetTrampolineSize() const { return TS; }
    void Install(SKSE::Trampoline& tranpoline) {
        if (REL::Module::IsAE()) {
            if (_ae_offset == 0x0) {
                logger::error("Missing AE offset");
                return;
            }
            if (_ae_id == 0) {
                logger::error("Missing AE id");
                return;
            }
        } else {
            if (_se_offset == 0x0) {
                logger::error("Missing SE offset");
                return;
            }
            if (_se_id == 0) {
                logger::error("Missing SE id");
                return;
            }
        }
        const REL::Relocation<std::uintptr_t> function{REL::RelocationID(_se_id, _ae_id)};
        if constexpr (Kind == WriteKind::kCall) {
            T::originalFunction =
                tranpoline.write_call<N>(function.address() + REL::Relocate(_se_offset, _ae_offset), T::thunk);
        } else {
            T::originalFunction =
                tranpoline.write_branch<N>(function.address() + REL::Relocate(_se_offset, _ae_offset), T::thunk);
        }
    };
};

class HookBuilder {
private:
    std::vector<std::unique_ptr<GenericHookItem>> items;

    
    
    inline static HookBuilder* singleton = nullptr;
    inline static bool firstCreation = true;

public:

    static HookBuilder* GetSingleton() {

        if (!singleton && firstCreation) {
            firstCreation = false;
            singleton = new HookBuilder();
        }

        return singleton;
    }

    ~HookBuilder() { 
        items.clear();
    }
    template <class T, std::size_t N, std::size_t TS>
    void AddCall(int se_id, int se_offset, int ae_id, int ae_offset) {
        if constexpr (N != 5 && N != 6) {
            static_assert(false && N, "invalid call size");
        }

        auto newItem = std::make_unique<HookItem<T, N, TS, WriteKind::kCall>>(se_id, se_offset, ae_id, ae_offset);
        items.push_back(std::move(newItem));
    }
    template <class T, std::size_t N, std::size_t TS>
    void AddBranch(int se_id, int se_offset, int ae_id, int ae_offset) {
        if constexpr (N != 5 && N != 6) {
            static_assert(false && N, "invalid call size");
        }

        auto newItem = std::make_unique<HookItem<T, N, TS, WriteKind::kBranch>>(se_id, se_offset, ae_id, ae_offset);
        items.push_back(std::move(newItem));
    }

    void Install() {

        if (items.size() == 0) {
            return;
        }

        auto& trampoline = SKSE::GetTrampoline();
        trampoline.create(std::accumulate(items.begin(), items.end(), static_cast<size_t>(0),
                                          [](const size_t sum, const std::unique_ptr<GenericHookItem>& cls) {
                                              return sum + cls->GetTrampolineSize();
                                          }));
        for (const auto& item : items) {
            item->Install(trampoline);
        }

        delete this;
    }
};
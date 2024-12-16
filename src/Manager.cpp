#include "Manager.h"

void Manager::Register(std::string key, variants value) {
    key = Str::processString(key);

    auto it = sources.find(key);
    if (it != sources.end()) {
        it->second.insert(it->second.end(), value.begin(), value.end());
    } else {
        sources[key] = value;
    }

    for (auto item : value) {
        sources[item->key] = value;
    }
}

void Manager::Process(RE::TESBoundObject* base, RE::FormID id) {
    auto manager = Manager::GetSingleton();
    auto wrapper = new AVModel(base);
    Process(wrapper, id);
    delete wrapper;
}

void Manager::Process(RE::TESObjectARMA* base, RE::FormID id) {
    auto manager = Manager::GetSingleton();
    auto wrapper = new AVObjectARMA(base);
    Process(wrapper, id);
    delete wrapper;
}

void Manager::Process(AVObject* arma, RE::FormID id) {
    arma->Match(sources, id);
}

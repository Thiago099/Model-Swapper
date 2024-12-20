#include "Plugin.h"

void OnMessage(SKSE::MessagingInterface::Message* message) {
    auto manager = Manager::GetSingleton();
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
    }
    else if (message->type == SKSE::MessagingInterface::kSaveGame) {
        Hooks::listenSave2.store(false);
        Hooks::listenSave.store(true);
    }
    else if (message->type == SKSE::MessagingInterface::kPreLoadGame) {
        Hooks::listenSave.store(false);
        Hooks::listenSave2.store(false);
		manager->PreLoadGame();
    } 
    else if (message->type == SKSE::MessagingInterface::kNewGame) {
    	manager->CleanData();
    } 
    else if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
        //auto form = RE::TESForm::LookupByID<RE::TESObjectMISC>(0x5ACE4);
        //auto player = RE::PlayerCharacter::GetSingleton();

        //auto ptr = player->PlaceObjectAtMe(form, false);

        //auto ref = ptr.get();

        //ref->extraList.RemoveByType(RE::ExtraDataType::kModelSwap);
        //const auto xText = RE::BSExtraData::Create<RE::ExtraModelSwap>();
        //ref->extraList.Add(xText);
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    SetupLog();
    logger::info("Plugin loaded");
    Hooks::Install();
    Persistence::Install();
    // TODO: Clear serialization folder for non-existing/deleted save game files.
    return true;
}

#include "Plugin.h"
#include "HookBuilder.h"

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
    }
    if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
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
    HookBuilder::GetSingleton()->Install();
    Persistence::Install();
    return true;
}

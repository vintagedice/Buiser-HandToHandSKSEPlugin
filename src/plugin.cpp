#include "animhandler.hpp"
#include "h2hlevel.hpp"
#include "hithandler.hpp"
#include "logger.hpp"

namespace {
    static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message) {
        static bool ssmOk = false;
        switch (message->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            bhh_events::HitEventHandler::Register();
            ssmOk = h2h_level::StartingSkillManager::GetSingleton()->LoadForms();
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
            bhh_events::AnimHandler::Register();
            if (ssmOk) {
                h2h_level::StartingSkillManager::GetSingleton()->HandleExistingCharacter();
            }
            break;
        case SKSE::MessagingInterface::kNewGame:
            if (ssmOk) {
                h2h_level::StartingSkillManager::GetSingleton()->RegisterForRaceMenuDone();
            }
            break;
        }
    }
}
SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    bhh_logger::SetupLog();
    auto* plugin = SKSE::PluginDeclaration::GetSingleton();
    h2h_level::LoadSettingsINI();
    logger::info("Registering {}, Version {}, for load.", plugin->GetName(), plugin->GetVersion());
    SKSE::GetMessagingInterface()->RegisterListener("SKSE", SKSEMessageHandler);
    return true;
}

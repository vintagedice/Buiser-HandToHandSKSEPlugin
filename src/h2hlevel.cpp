#include "h2hlevel.hpp"

#include <SimpleIni.h>

#include "RE/Skyrim.h"
#include "formutil.hpp"
#include "logger.hpp"
#include "scriputil.hpp"

using h2h_level::Settings;
using h2h_level::StartingSkillManager;

void loadSettingVal(const char* section, CSimpleIniA& ini, h2h_level::SettingVal& sv) {
    sv.value = static_cast<float>(ini.GetDoubleValue(section, sv.name, sv.regular));
    if (sv.value < sv.min || sv.value > sv.max) {
        sv.value = sv.regular;
    }
    logger::info("Setting {}.{} set to {}", section, sv.name, sv.value);
}

void h2h_level::LoadSettingsINI() {
    CSimpleIniA ini;
    ini.SetUnicode();
    auto err = ini.LoadFile(R"(.\Data\SKSE\Plugins\BruiserHandToHandSKSEPlugin.ini)");
    if (SI_OK != err) {
        logger::warn("Failed to load settings ini file with error code {}. Defaulting to hardcoded values.", err);
        return;
    }
    auto constexpr xpSection = "SkillXP";
    loadSettingVal(xpSection, ini, Settings.SkillUseMult);
    loadSettingVal(xpSection, ini, Settings.SkillUseOffset);
    loadSettingVal(xpSection, ini, Settings.SkillImproveMult);
    loadSettingVal(xpSection, ini, Settings.SkillImproveOffset);
    loadSettingVal(xpSection, ini, Settings.DamageXPDampen);
    logger::info("Finished loading XP settings from ini.");
}

float h2h_level::nextSkillLevelXP(float currentLevel, float xpSkillCurve) {
    return Settings.SkillImproveMult.value * powf(currentLevel, xpSkillCurve) + Settings.SkillImproveOffset.value;
}

float h2h_level::calcSkillXpGain(float damage) {
    return Settings.SkillUseMult.value * powf(damage, Settings.DamageXPDampen.value) + Settings.SkillUseOffset.value;
}

void h2h_level::GivePlayerXP(float xpGained) {
    static std::mutex xpFuncMutex;
    static auto papyrusVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    static auto player = RE::PlayerCharacter::GetSingleton();

    std::lock_guard<std::mutex> guard(xpFuncMutex);
    LOGTRACE("Processing player level xp of {}.", xpGained);

    // Dispatch out the function to get current experience.
    float currentXP = -1.0;
    auto getXPCallback =
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>(new script_util::FloatCallbackFunctor(currentXP));
    std::unique_ptr<RE::BSScript::IFunctionArguments> args1(RE::MakeFunctionArguments());
    if (!papyrusVM->DispatchStaticCall("Game", "GetPlayerExperience", args1.get(), getXPCallback)) {
        logger::error("Error in dispatch call.");
        return;
    }
    // Wait for the result of the current XP. We're going to use it to then set the new XP.
    dynamic_cast<script_util::FloatCallbackFunctor*>(getXPCallback.get())->WaitForCallback();
    if (currentXP < 0.0) {
        logger::error("Failed to obtain current player Level XP.");
    }
    float newXP = currentXP + xpGained;
    LOGTRACE("Player XP: {}, new XP {}", currentXP, newXP);

    // Create dispatch not to actually update XP
    auto setXPCallback =
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>(new script_util::WaitingCallbackFunctor{});
    std::unique_ptr<RE::BSScript::IFunctionArguments> args3(RE::MakeFunctionArguments(std::move(newXP)));
    if (!papyrusVM->DispatchStaticCall("Game", "SetPlayerExperience", args3.get(), setXPCallback)) {
        logger::error("Error in dispatch call.");
        return;
    }
    dynamic_cast<script_util::WaitingCallbackFunctor*>(setXPCallback.get())->WaitForCallback();
    LOGTRACE("XP Gain Finished");
}

StartingSkillManager* StartingSkillManager::GetSingleton() {
    static StartingSkillManager singleton{};
    return std::addressof(singleton);
}

bool StartingSkillManager::LoadForms() {
    if (!initFormFromEditorId(glob.skillLevelId, glob.skillLevel)) return false;
    if (!initGSFromEditorId(gamesetting.skillStartId, gamesetting.skillStart)) return false;
    logger::info("Starting Skill Manager loaded.");
    return true;
}

bool StartingSkillManager::RegisterForRaceMenuDone() {
    newCharacter = true;
    auto ui = RE::UI::GetSingleton();
    if (ui == nullptr) {
        logger::error("Failed to get UI event source holder when trying to register race menu complete event.");
        return false;
    }
    ui->AddEventSink(this);
    logger::info("Skill manager ready to set skill values after racemenu close");
    return true;
}

RE::BSEventNotifyControl StartingSkillManager::ProcessEvent(const RE::MenuOpenCloseEvent* event,
                                                            RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
    if (event->opening || event->menuName != "RaceSex Menu") {
        return RE::BSEventNotifyControl::kContinue;
    }
    if (glob.skillLevel->value != 0) {
        logger::info("Skill level already set, no starting skill management needed.");
        return RE::BSEventNotifyControl::kStop;
    }

    setStartingSkillValue();
    newCharacter = false;
    return RE::BSEventNotifyControl::kStop;
}

void StartingSkillManager::HandleExistingCharacter() {
    LOGTRACE("Handling skill initialization for existing save.");

    if (glob.skillLevel->value != 0.f) {
        logger::info("Skill level already set, no starting skill management needed.");
        return;
    }
    if (newCharacter) {
        logger::info("Race Menu close event will handle setting starting skills. Ignoring post game load.");
        return;
    }
    auto player = RE::PlayerCharacter::GetSingleton();
    if (player == nullptr) {
        logger::error("Unable to get player reference after character creation.");
        return;
    }
    auto const& gsd = player->GetGameStatsData();
    if (gsd.byCharGenFlag & RE::PlayerCharacter::ByCharGenFlag::kHandsBound) {
        logger::info(
            "Loaded into game preexisting save that hasn't yet finished race menu selection. Will set starting skills "
            "post race menu finish.");
        RegisterForRaceMenuDone();
        return;
    }

    setStartingSkillValue();
}

void StartingSkillManager::setStartingSkillValue() {
    LOGTRACE("Performing Hand To Hand Skill value initialization.");
    auto const player = RE::PlayerCharacter::GetSingleton();
    if (player == nullptr) {
        logger::error("Unable to get player reference during skill value initialization.");
        return;
    }
    auto const playerRace = player->GetRace();
    if (playerRace == nullptr) {
        logger::error("Unable to get player race info.");
        return;
    }
    float oneHandedBonus = 0.f;
    float twoHandedBonus = 0.f;
    for (auto const& boost : playerRace->data.skillBoosts) {
        if (boost.skill == RE::ActorValue::kOneHanded) {
            oneHandedBonus = boost.bonus;
        } else if (boost.skill == RE::ActorValue::kTwoHanded) {
            twoHandedBonus = boost.bonus;
        }
    }
    float startingH2H =
        std::ceil(static_cast<float>(gamesetting.skillStart->GetSInt()) + (oneHandedBonus + twoHandedBonus) / 2.f);
    logger::info("Seting Hand to Hand starting skill level to {}", startingH2H);
    glob.skillLevel->value = startingH2H;
    return;
}
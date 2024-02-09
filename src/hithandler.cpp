#pragma once
#include "hithandler.hpp"

#include "formutil.hpp"
#include "h2hlevel.hpp"
#include "logger.hpp"
#include "scriputil.hpp"

using bhh_events::HitEventHandler;

HitEventHandler* HitEventHandler::GetSingleton() {
    static HitEventHandler singleton{};
    return std::addressof(singleton);
}

bool HitEventHandler::Register() {
    auto handler = HitEventHandler::GetSingleton();

    // Grab Game Settings
    if (!initGSFromEditorId(handler->gamesetting.xpPerRankGSId, handler->gamesetting.xpPerSkillRank)) return false;
    if (!initGSFromEditorId(handler->gamesetting.xpSkillCurveId, handler->gamesetting.xpSkillCurve)) return false;

    // Grab Global Vars
    if (!initFormFromEditorId(handler->glob.skillLevelId, handler->glob.skillLevel)) return false;
    if (!initFormFromEditorId(handler->glob.skillExpId, handler->glob.skillExp)) return false;
    if (!initFormFromEditorId(handler->glob.skillRatioId, handler->glob.skillRatio)) return false;
    if (!initFormFromEditorId(handler->glob.skillShowLevelUpId, handler->glob.skillShowLevelUp)) return false;
    if (!initFormFromEditorId(handler->glob.enablePlayerXPId, handler->glob.enablePlayerXP)) return false;
    if (!initFormFromEditorId(handler->glob.skillXPModId, handler->glob.skillXPMod)) return false;
    if (!initFormFromEditorId(handler->glob.enableBeastFormXPId, handler->glob.enableBeastFormXP)) return false;

    // Get Unarmed weapon keyword
    if (!initFormFromEditorId(handler->keyword.unarmedWeapKeywordId, handler->keyword.unarmedKeyword)) return false;

    // Finally actually register listener now that we have what we need
    RE::ScriptEventSourceHolder* eventHolder = RE::ScriptEventSourceHolder::GetSingleton();
    eventHolder->AddEventSink(handler);
    logger::info("HitEvent event registered.");
    return true;
}

// Checks if player is in beast form and if so are we allowed to collect XP in beast form
bool HitEventHandler::AllowedForm() const {
    static auto MenuControls = RE::MenuControls::GetSingleton();
    return !MenuControls->InBeastForm() || glob.enableBeastFormXP->value != 0.0f;
}

RE::BSEventNotifyControl HitEventHandler::ProcessEvent(const RE::TESHitEvent* event,
                                                       RE::BSTEventSource<RE::TESHitEvent>*) {
    if (glob.skillLevel->value >= h2h_level::Settings.SkillMaxLevel) {
        LOGTRACE("Character at max level, no more xp hit processing.");
        return RE::BSEventNotifyControl::kContinue;
    }
    if (glob.skillXPMod->value <= 0) {
        LOGTRACE("Skill modify multiplier set to 0. No more XP hit processing.");
        return RE::BSEventNotifyControl::kContinue;
    }
    if (!event) {
        logger::error("Hit Event Source Not Found!");
        return RE::BSEventNotifyControl::kContinue;
    }
    if (!event->cause) {
        logger::error("Hit Event Attacker Not Found!");
        return RE::BSEventNotifyControl::kContinue;
    }
    if (!event->target) {
        logger::error("Hit Event Target Not Found!");
        return RE::BSEventNotifyControl::kContinue;
    }
    auto defender = event->target->As<RE::Actor>();
    if (!event->cause->IsPlayerRef() || defender == nullptr) {
        LOGTRACE("Ignoring hit from either non player source or non actor target.");
        return RE::BSEventNotifyControl::kContinue;
    }
    LOGTRACE("Player hit Event recieved: attacker {}, target {}", event->cause->GetDisplayFullName(),
             event->target->GetDisplayFullName());

    if (!AllowedForm()) {
        LOGTRACE("Player in beast form, and beast form exp is off, disabling.");
        return RE::BSEventNotifyControl::kContinue;
    }
    auto attackingWeapon = RE::TESForm::LookupByID<RE::TESObjectWEAP>(event->source);
    if (attackingWeapon == nullptr) {
        LOGTRACE("Hit from a non melee: 0x{:x}. Ignoring.", event->source);
        return RE::BSEventNotifyControl::kContinue;
    }
    if (!attackingWeapon->HasKeyword(keyword.unarmedKeyword)) {
        LOGTRACE("Hit from a non unarmed: {}. Ignoring.", attackingWeapon->GetFullName());
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::AIProcess* defenderProcess = defender->GetActorRuntimeData().currentProcess;
    if (!defenderProcess || !defenderProcess->high ||
        defender->AsActorState()->GetLifeState() == RE::ACTOR_LIFE_STATE::kDead || !defender->Get3D()) {
        LOGTRACE("Defender is dead or not valid.");
        return RE::BSEventNotifyControl::kContinue;
    }
    // We have everything we need from this hit, return now. Process the hit xp in a different thread.
    std::thread(&HitEventHandler::ApplyHandToHandXP, this, defender, attackingWeapon).detach();
    return RE::BSEventNotifyControl::kContinue;
}

void HitEventHandler::ApplyHandToHandXP(RE::Actor* defender, RE::TESObjectWEAP* weapon) const {
    static auto player = RE::PlayerCharacter::GetSingleton();
    static std::mutex xpFuncMutex;
    std::lock_guard<std::mutex> guard(xpFuncMutex);
    if (glob.skillLevel->value >= h2h_level::Settings.SkillMaxLevel) {
        return;
    }
    LOGTRACE("Processing hand to hand xp from hit.");
    // Calculate assumed base damage of a current unarmed hit.
    auto damage = player->CalcUnarmedDamage();
    LOGTRACE("Unarmed base damage: {}", damage);
    RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModAttackDamage, player, weapon, defender,
                                        &damage);
    LOGTRACE("Unarmed perk modded damage: {}", damage);

    // Get any skill gain improvement effects.
    auto skillImprove = 1.0f;
    RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModSkillUse, player, &skillImprove);
    if (skillImprove < 0.0f) {
        logger::error(
            "Skill improve set to negative? Just using 1. There may be a bad perk somewhere since this isn't supposed "
            "to be less than 0.");
        skillImprove = 1.0f;
    }
    LOGTRACE("Skill improve mult: {}", skillImprove);
    LOGTRACE("Calculating skill xp with skillimprove = {}, skillMod = {}", skillImprove, glob.skillXPMod->value);
    float xpGain = skillImprove * h2h_level::calcSkillXpGain(damage) * glob.skillXPMod->value;
    if (xpGain <= 0) {
        logger::info("XP gain was less than or equal to 0. No H2H exp added.");
        return;
    }
    float xpNeeded = h2h_level::nextSkillLevelXP(glob.skillLevel->value, gamesetting.xpSkillCurve->GetFloat());
    LOGTRACE("XP for next level at {}, with skill curve {}, is {}", glob.skillLevel->value,
             gamesetting.xpSkillCurve->GetFloat(), xpNeeded);
    logger::info("XP Gain is {}", xpGain);

    auto newExp = glob.skillExp->value + xpGain;
    float playerLevelXP = 0.0f;
    while (newExp >= xpNeeded && glob.skillLevel->value < h2h_level::Settings.SkillMaxLevel) {
        // Temp set this to one to show that it's been filled.
        glob.skillRatio->value = 1.0;
        // Skill Level Up
        float newLevel = glob.skillLevel->value + 1.0f;
        LOGTRACE("New Skill level {}", newLevel);
        glob.skillShowLevelUp->value = newLevel;
        glob.skillLevel->value = newLevel;
        newExp -= xpNeeded;
        xpNeeded = h2h_level::nextSkillLevelXP(newLevel, gamesetting.xpSkillCurve->GetFloat());
        LOGTRACE("New XP need for next level {}", xpNeeded);
        if (glob.enablePlayerXP->value != 0) {
            playerLevelXP += newLevel * gamesetting.xpPerSkillRank->GetFloat();
        }
    }
    if (playerLevelXP > 0.0) {
        LOGTRACE("Dispatching player xp func");
        std::thread(h2h_level::GivePlayerXP, playerLevelXP).detach();
    }
    if (glob.skillLevel->value >= h2h_level::Settings.SkillMaxLevel) {
        glob.skillExp->value = 0.0f;
        glob.skillRatio->value = 0.0f;
        return;
    }
    glob.skillExp->value = newExp;
    glob.skillRatio->value = newExp / xpNeeded;
}
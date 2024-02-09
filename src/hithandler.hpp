#pragma once
#include "RE/Skyrim.h"

namespace bhh_events {
    class HitEventHandler : public RE::BSTEventSink<RE::TESHitEvent> {
    public:
        static HitEventHandler* GetSingleton();
        static bool Register();

        RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event,
                                              RE::BSTEventSource<RE::TESHitEvent>* a_eventSource) override;

    private:
        // Game settings
        struct {
            static constexpr auto xpPerRankGSId = "fXPPerSkillRank";
            static constexpr auto xpSkillCurveId = "fSkillUseCurve";
            RE::Setting *xpPerSkillRank, *xpSkillCurve;
        } gamesetting;

        // Global Vars
        struct {
            static constexpr auto skillLevelId = "BHH_HandtoHandLevel";
            static constexpr auto skillExpId = "BHH_HandtoHandExp";
            static constexpr auto skillRatioId = "BHH_HandtoHandRatio";
            static constexpr auto skillShowLevelUpId = "BHH_HandtoHandShowLevelup";
            static constexpr auto skillXPModId = "BHH_H2HXPMod";
            static constexpr auto enablePlayerXPId = "BHH_EnablePlayerXPGain";
            static constexpr auto enableBeastFormXPId = "BHH_EnableBeastFormXP";
            RE::TESGlobal *skillLevel, *skillExp, *skillRatio, *skillShowLevelUp, *enablePlayerXP, *skillXPMod,
                *enableBeastFormXP;
        } glob;

        struct {
            static constexpr auto unarmedWeapKeywordId = "BHH_WeapTypeUnarmed";
            RE::BGSKeyword* unarmedKeyword;
        } keyword;

        HitEventHandler() = default;
        ~HitEventHandler() = default;
        void ApplyHandToHandXP(RE::Actor* defender, RE::TESObjectWEAP* weapon) const;
        bool AllowedForm() const;
    };
}

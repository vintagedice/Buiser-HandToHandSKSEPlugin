#pragma once

#include "RE/Skyrim.h"

/*
 * Mimic a real skill with our own skill settings for the level up formula
 * TODO: load from an ini?
 */
namespace h2h_level {

    struct SettingVal {
        const char* name;
        const float min, max, regular;
        float value;
        SettingVal(const char* nameGiven, float minGiven, float maxGiven, float regGiven)
            : name(nameGiven), min(minGiven), max(maxGiven), regular(regGiven), value(regGiven) {}
    };

    /*
     * Settings with an in game equivalent.
     */
    struct {
        SettingVal SkillUseMult{"SkillUseMult", 0.0f, 100.f, 6.6f};
        SettingVal SkillUseOffset{"SkillUseOffset", 0.0f, 100.f, 1.0f};
        SettingVal SkillImproveMult{"SkillImproveMult", 0.0f, 100.f, 2.0f};
        SettingVal SkillImproveOffset{"SkillImproveOffset", 0.0f, 100.f, 0.0f};
        /*
         * Normal melee skills go off the base damage of the weapon dealt to target.
         * Normally the player can get higher tier weapons to keep leveling up, but they can't change their hands.
         * As such we calculate the damage as the unarmed damage with all perks applied, but dampen the effect a bit by
         * exponenentiating it to this value.
         */
        SettingVal DamageXPDampen{"DamageXPDampen", 0.0f, 2.f, 0.91f};

        // Max Hand To Hand Level
        const float SkillMaxLevel = 100.0f;
    } Settings;

    void LoadSettingsINI();

    // Formula used by the game to calculate amount of skill points needed for the next level
    float nextSkillLevelXP(float currentLevel, float xpSkillCurve);
    // Formula used by gain to calculate how much skill XP to give for this attack
    float calcSkillXpGain(float damage);
    // Give player level experience.
    void GivePlayerXP(float xpGained);

    class StartingSkillManager : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    public:
        static StartingSkillManager* GetSingleton();

        bool LoadForms();

        // Sets the HandToHand skill to a starting value based on race bonuses.
        // Used when an existing character that doesn't need race menu to finish is selected.
        void HandleExistingCharacter();

        bool RegisterForRaceMenuDone();

        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
                                              RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

    private:
        StartingSkillManager() = default;
        void setStartingSkillValue();

        struct {
            static constexpr auto skillLevelId = "BHH_HandtoHandLevel";
            RE::TESGlobal* skillLevel{nullptr};
        } glob;

        struct {
            static constexpr auto skillStartId = "iAVDSkillStart";
            RE::Setting* skillStart;
        } gamesetting;

        std::atomic<bool> newCharacter = false;
    };
}
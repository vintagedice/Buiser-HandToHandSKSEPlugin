#pragma once
#include "RE/Skyrim.h"

namespace bhh_events {

    class AnimHandler : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
    public:
        static AnimHandler* GetSingleton();
        static bool Register();

        RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event,
                                              RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource) override;

    private:
        AnimHandler() = default;
        virtual ~AnimHandler() = default;
        struct {
            static constexpr auto enableH2HBlockId = "BHH_EnableH2HBlock";
            static constexpr auto rotateAttackId = "BHH_RotateAttacks";
            RE::TESGlobal *enableH2HBlock, *rotateAttack;
        } glob;
        struct {
            static constexpr auto unarmedWeapKeywordId = "BHH_WeapTypeUnarmed";
            RE::BGSKeyword* unarmedKeyword;
        } keyword;
        bool isToggleOn() const;
        void applyToggle(std::string const& attackEvent, bool isPower);
    };
}
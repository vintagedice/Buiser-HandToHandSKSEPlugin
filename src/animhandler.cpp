#include "animhandler.hpp"

#include "formutil.hpp"
#include "logger.hpp"

using bhh_events::AnimHandler;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::steady_clock;

static RE::PlayerCharacter* player;

struct {
    // This appears to be the animation tag used by all attack start animations.
    // Does not play on repeated attack strings
    static constexpr auto attackStart = "PowerAttack_Start_end";

    // This appears to only occur once! even in combo attacks.
    // This is a suitable since its seems like the best event to catch exactly one attack input.
    static constexpr auto preHitFrame = "preHitFrame";

    // These one show up when a player spams a light attack
    static constexpr auto attackFollow = "AttackWinStart";
    static constexpr auto attackFollowLeft = "AttackWinStartLeft";

    // This sems to be the only power attack unique anim tag
    static constexpr auto powerAttackEnd = "PowerAttackStop";

    // Happens at end of all attacks
    static constexpr auto attackStop = "attackStop";
} animTags;
struct {
    // These appear to the the Attack Event data associated with the different possible hand to hand attacks
    static constexpr auto rightAttack = "AttackStartH2HRight";
    static constexpr auto rightPowerAttack = "attackPowerStartForwardH2HRightHand";
    static constexpr auto leftAttack = "AttackStartH2HLeft";
    static constexpr auto leftPowerAttack = "attackPowerStartForwardH2HLeftHand";
    static constexpr auto comboPowerAttack = "attackPowerStartH2HCombo";
} events;

AnimHandler* AnimHandler::GetSingleton() {
    static AnimHandler singleton{};
    return std::addressof(singleton);
}

bool AnimHandler::Register() {
    player = RE::PlayerCharacter::GetSingleton();
    if (player == nullptr) {
        logger::error("Failed to load player pointer");
        return false;
    }
    auto handler = GetSingleton();
    // Get toggle behaviour globals
    if (!initFormFromEditorId(handler->glob.enableH2HBlockId, handler->glob.enableH2HBlock)) return false;
    if (!initFormFromEditorId(handler->glob.rotateAttackId, handler->glob.rotateAttack)) return false;
    // Get Unarmed weapon keyword
    if (!initFormFromEditorId(handler->keyword.unarmedWeapKeywordId, handler->keyword.unarmedKeyword)) return false;

    if (!player->AddAnimationGraphEventSink(handler)) {
        logger::error("Failed to register AnimationGraphEvent event");
        return false;
    }
    logger::info("AnimationGraphEvent event Registered");
    return true;
}

// Check if hand to hand is in both hands. Null weapon is normal hand to hand.
bool H2HEquiped(RE::BGSKeyword* const unarmedKeyword) {
    auto weapForm = player->GetEquippedObject(false);
    if (weapForm != nullptr) {
        auto weap = weapForm->As<RE::TESObjectWEAP>();
        if (weap != nullptr && !weap->HasKeyword(unarmedKeyword)) {
            LOGTRACE("right unarmed check fail");
            return false;
        }
    }
    weapForm = player->GetEquippedObject(true);
    if (weapForm != nullptr) {
        auto weap = weapForm->As<RE::TESObjectWEAP>();
        if (weap != nullptr && !weap->HasKeyword(unarmedKeyword)) {
            LOGTRACE("right unarmed check fail");
            return false;
        }
    }
    return true;
}

bool AnimHandler::isToggleOn() const {
    return glob.enableH2HBlock->value == 1.0f &&
           (glob.rotateAttack->value == 1.0f || glob.rotateAttack->value == -1.0f);
}

RE::BSEventNotifyControl AnimHandler::ProcessEvent(const RE::BSAnimationGraphEvent* event,
                                                   RE::BSTEventSource<RE::BSAnimationGraphEvent>*) {
    static std::chrono::time_point<steady_clock> lastToggleTime;
    static const auto minWait = milliseconds(400);
    if (!isToggleOn() || !H2HEquiped(keyword.unarmedKeyword)) {
        return RE::BSEventNotifyControl::kContinue;
    }
    auto now = steady_clock::now();
    if (duration_cast<milliseconds>(now - lastToggleTime) < minWait) {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto playerProcess = player->GetActorRuntimeData().currentProcess;
    if (playerProcess == nullptr) {
        logger::error("null player process data");
        return RE::BSEventNotifyControl::kContinue;
    }
    auto hiProcess = playerProcess->high;
    if (hiProcess == nullptr) {
        logger::error("null high process data for player");
        return RE::BSEventNotifyControl::kContinue;
    }
    auto attackData = hiProcess->attackData;
    if (attackData == nullptr) {
        // Non attack event. Ignore
        return RE::BSEventNotifyControl::kContinue;
    }
    bool isPower = static_cast<bool>(attackData->data.flags & RE::AttackData::AttackFlag::kPowerAttack);
    std::string const tag{event->tag.data()}, attackEvent{attackData->event.c_str()};

    if (attackEvent != events.comboPowerAttack &&     // Dont toggle on power combo
        (tag.rfind(animTags.attackFollow, 0) == 0 ||  // Toggle on combo hits or...
         tag.rfind(animTags.attackStop, 0) == 0)) {   // toggle on attack stoping all together
        LOGTRACE("animEventTag {}, attackEvent {}, attack isPower {}", tag, attackEvent, isPower);
        applyToggle(attackEvent, isPower);
        lastToggleTime = now;
    }
    return RE::BSEventNotifyControl::kContinue;
}

void AnimHandler::applyToggle(std::string const& attackEvent, bool isPower) {
    LOGTRACE("Applying toggle");
    if (isPower) {
        if (attackEvent == events.rightPowerAttack) {
            glob.rotateAttack->value = -1.0f;
            return;
        }
        if (attackEvent == events.leftPowerAttack) {
            glob.rotateAttack->value = 1.0f;
            return;
        }
    } else {
        if (attackEvent == events.rightAttack) {
            glob.rotateAttack->value = -1.0f;
            return;
        }
        if (attackEvent == events.leftAttack) {
            glob.rotateAttack->value = 1.0f;
            return;
        }
    }
    LOGTRACE("No case reached?");
    LOGTRACE("Event {}, Atatck Toggle: {}", attackEvent, glob.rotateAttack->value);
}

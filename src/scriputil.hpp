#pragma once
#include "RE/Skyrim.h"

namespace script_util {
    /*
     * WaitingCallbackFunctor allows use to wait for the results of a dispatched VM call.
     * All classes inheriting must call the parent WaitingCallbackFunctor::operator() method in their overrides or they
     * will never release the condition variable to those wating.
     * DANGER - Wrap in the RE::BSTSmartPointer and don't manually manage these struct.
     */
    class WaitingCallbackFunctor : public RE::BSScript::IStackCallbackFunctor {
    public:
        WaitingCallbackFunctor() = default;
        virtual ~WaitingCallbackFunctor() = default;

        void operator()(RE::BSScript::Variable) override;
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override{};

        virtual void WaitForCallback();

    protected:
        std::mutex mtx;
        std::condition_variable cv;
        bool ready = false;
    };

    // FloatCallbackFunctor and fills it with the result of callback
    class FloatCallbackFunctor : public WaitingCallbackFunctor {
    public:
        explicit FloatCallbackFunctor(float& floatStore) : callbackVal(floatStore) {}
        virtual ~FloatCallbackFunctor() = default;

        void operator()(RE::BSScript::Variable result) override;

    private:
        float& callbackVal;
    };
}
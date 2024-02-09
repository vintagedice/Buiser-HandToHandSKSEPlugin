#include "logger.hpp"
#include "scriputil.hpp"

using script_util::FloatCallbackFunctor;
using script_util::WaitingCallbackFunctor;

void WaitingCallbackFunctor::operator()(RE::BSScript::Variable) {
    ready = true;
    cv.notify_all();
}

void WaitingCallbackFunctor::WaitForCallback() {
    LOGTRACE("Entering callback wait, ready state: {}", ready);
    std::unique_lock<std::mutex> lck(mtx);
    while (!ready) cv.wait(lck);
    LOGTRACE("Done callback wait");
}

void FloatCallbackFunctor::operator()(RE::BSScript::Variable result) {
    if (result.IsFloat()) {
        LOGTRACE("Return float is {}", result.GetFloat());
        callbackVal = result.GetFloat();
    } else {
        logger::error("VM callback didn't return a float like expected. Got {}", result.GetType().TypeAsString());
    }
    WaitingCallbackFunctor::operator()(result);
}
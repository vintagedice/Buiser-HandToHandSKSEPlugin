#pragma once
namespace bhh_logger {
    void SetupLog();
}

// Don't even compile the calls to trace for release versions.
#if _DEBUG
    #define LOGTRACE(...) logger::trace(__VA_ARGS__)
#else
    #define LOGTRACE(...)
#endif
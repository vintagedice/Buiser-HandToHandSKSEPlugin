#pragma once

#include <cmath>
#include <mutex>
#include <thread>
#include <typeinfo>

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

/// Leave at bottom and in order these Windows Headers
#include <ShlObj_core.h>
#include <Windows.h>
// This must be included after the others
#include <Psapi.h>
#undef cdecl  // Workaround for Clang 14 CMake configure error.

// Compatible declarations with other sample projects.
#define DLLEXPORT __declspec(dllexport)

using namespace std::literals;
using namespace REL::literals;
namespace logger = SKSE::log;

namespace util {
    using SKSE::stl::report_and_fail;
}

#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include "core/mem.hpp"
#include "core/types.hpp"
#include "dump/interfaces.hpp"
#include "dump/netvars.hpp"
#include "dump/offsets.hpp"

namespace hd::app {

enum class ExitCode : DWORD {
    Ok = 0,
    DumpDir = 1,
    NoModules = 2,
    Signatures = 3,
    NoVClient = 4,
    NoClassList = 5,
    NoNetvars = 6,
    Write = 7,
    MenuGate = 8,
    Unhandled = 99,
};

[[nodiscard]] DWORD ToDword(ExitCode code) noexcept;

[[nodiscard]] DWORD RunGuarded(HMODULE self) noexcept;

}

// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include <cstdint>

enum class LogLevel : uint8_t
{
    Info,
    Step,       // section header, so a long log can be skimmed
    Command,    // exactly what went to the car, for reproducing by hand
    Output,
    Pass,
    Warning,
    Error
};

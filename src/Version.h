// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

// The one place the version lives; it changes only when the owner asks. Macros, not constexpr, because App.rc's file
// version must come from here too and the resource compiler reads only the preprocessor.
#define NBTAA_VERSION_MAJOR 0
#define NBTAA_VERSION_MINOR 1
#define NBTAA_VERSION_PATCH 1
#define NBTAA_VERSION_TEXT "0.1.1"

#ifndef RC_INVOKED
constexpr const char* kAppVersion = NBTAA_VERSION_TEXT;
#endif

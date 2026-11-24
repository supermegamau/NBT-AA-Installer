// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include <cstdint>
#include <string>

// Every NBT Evo ships with the same root login (NBT-AAD's install guide), so there is nothing for the user to type.
constexpr const char* kCarUser = "root";
constexpr const char* kCarPassword = "ts&SK412";

// The car's SSH port never changes; only TurboCode's proxy on this PC forwards it from another port.
constexpr const char* kProxyHost = "127.0.0.1";
constexpr uint16_t kProxyPort = 6969;  // TurboCode's forward, as in NBT-AAD's install guide
constexpr uint16_t kSshPort = 22;

struct CarConnection
{
    std::string host;

    uint16_t Port() const { return host == kProxyHost ? kProxyPort : kSshPort; }
};

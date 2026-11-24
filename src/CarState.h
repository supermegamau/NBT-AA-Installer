// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include <cstdint>
#include <string>
#include <vector>

// What Connect found on the car; the window shows it and every later run starts by reading it again.
struct CarState
{
    bool                     valid = false;
    std::string              node;            // hu-jacinto or hu-omap
    bool                     packaged = false;  // NBTApps package: its own folders, settings in the data dir
    std::string              appDir;
    std::string              dataDir;
    std::string              envPath;
    bool                     daemonPresent = false;
    std::string              daemonVersion;   // NBT-AAD-VERSION from the binary on the car; builds before v1.9.68 have none
    uint64_t                 daemonSize = 0;
    std::string              daemonSha256;    // matched against GitHub's releases when the binary carries no version
    std::string              pluginVersion;   // from the plugin's RAM log, empty after a reboot until it runs
    bool                     running = false;
    bool                     envExists = false;
    std::string              envText;
    std::vector<std::string> dataFiles;       // the iDrive checkboxes are files here
    bool                     daemonBackup = false;
    bool                     pluginBackup = false;
    bool                     pluginComplete = false;  // all 5 files: false after a send that stopped half way, which must not overwrite .old
    std::string              cksumTool;       // empty = upload checked by size only
    std::string              restartTool;     // OnOffDSICommander; empty = ask for a reboot instead
};

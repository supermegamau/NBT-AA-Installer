// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include "SettingsCatalog.h"
#include "SshSession.h"

#include <wx/app.h>

#include <memory>

class InstallerApp : public wxApp
{
public:
    bool OnInit() override;
    int OnExit() override;

private:
    std::unique_ptr<SshLibrary> mSshLibrary;  // winsock and libssh2 must outlive every session
    SettingsCatalog             mCatalog;     // the window's tabs point into it for the whole run
};

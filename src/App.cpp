// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include "App.h"

#include "MainFrame.h"

#include <wx/msgdlg.h>
#include <wx/stdpaths.h>

#include <filesystem>

wxIMPLEMENT_APP(InstallerApp);

bool InstallerApp::OnInit()
{
    if (!wxApp::OnInit())
    {
        return false;
    }
    mSshLibrary = std::make_unique<SshLibrary>();
    if (!mSshLibrary->Ready())
    {
        wxMessageBox("Windows networking or libssh2 could not start.", "NBT-AA-Installer", wxOK | wxICON_ERROR);
        return false;
    }
    std::string error;
    if (!mCatalog.Load(std::filesystem::path(wxStandardPaths::Get().GetExecutablePath().ToStdWstring()).parent_path() / "data" / "settings.json", error))
    {
        wxMessageBox(wxString::FromUTF8("The settings list could not be read, so the tabs can't be built:\n\n" + error + "\n\nIt must sit next to the program as data\\settings.json."), "NBT-AA-Installer", wxOK | wxICON_ERROR);
        return false;
    }
    (new MainFrame(mCatalog))->Show();
    return true;
}

int InstallerApp::OnExit()
{
    mSshLibrary.reset();
    return wxApp::OnExit();
}

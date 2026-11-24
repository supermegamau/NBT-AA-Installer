// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include "GitHubReleases.h"

#include <wx/panel.h>

#include <functional>
#include <string>
#include <vector>

class wxButton;
class wxHyperlinkCtrl;
class wxListBox;
class wxStaticText;
class wxTextCtrl;

// "From GitHub": the last 20 releases, stable ones marked, and its own Send to car.
class GitHubPanel : public wxPanel
{
public:
    GitHubPanel(wxWindow* parent, std::function<void()> refresh, std::function<void()> send);

    void SetReleases(const std::vector<ReleaseInfo>& releases);
    void SetMessage(const std::string& text);
    void SetCarDaemon(const std::string& version, uint64_t size, const std::string& sha256);
    const ReleaseInfo* Selected() const;
    void SetBusy(bool busy);

private:
    void Redraw();
    void OnPick();

    std::vector<ReleaseInfo> mReleases;
    std::string              mCarVersion;
    uint64_t                 mCarSize = 0;
    std::string              mCarSha256;
    wxListBox*               mList = nullptr;
    wxStaticText*            mInfo = nullptr;
    wxHyperlinkCtrl*         mNotes = nullptr;
    wxButton*                mRefresh = nullptr;
    wxButton*                mSend = nullptr;
};

// "From this PC": a folder, and its own Send to car.
class LocalPanel : public wxPanel
{
public:
    LocalPanel(wxWindow* parent, std::function<void()> send);

    void SetPath(const std::string& path);
    std::string Path() const;
    void SetBusy(bool busy);

private:
    void ChooseFolder();
    void Inspect();

    wxTextCtrl*   mPath = nullptr;
    wxStaticText* mInfo = nullptr;
    wxButton*     mFolder = nullptr;
    wxButton*     mSend = nullptr;
};

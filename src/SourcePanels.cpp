// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include "SourcePanels.h"

#include "PackageSource.h"

#include <wx/button.h>
#include <wx/dirdlg.h>
#include <wx/hyperlink.h>
#include <wx/listbox.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <format>

GitHubPanel::GitHubPanel(wxWindow* parent, std::function<void()> refresh, std::function<void()> send) : wxPanel(parent)
{
    mList = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(150)));
    mList->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) { OnPick(); });
    mInfo = new wxStaticText(this, wxID_ANY, "Reading the release list...");
    mNotes = new wxHyperlinkCtrl(this, wxID_ANY, "release notes", "https://github.com/");
    mNotes->Hide();
    mRefresh = new wxButton(this, wxID_ANY, "Refresh");
    mSend = new wxButton(this, wxID_ANY, "Send to car");
    mRefresh->Bind(wxEVT_BUTTON, [refresh](wxCommandEvent&) { refresh(); });
    mSend->Bind(wxEVT_BUTTON, [send](wxCommandEvent&) { send(); });
    mSend->Disable();

    auto* bottom = new wxBoxSizer(wxHORIZONTAL);
    bottom->Add(mInfo, wxSizerFlags().CenterVertical());
    bottom->Add(mNotes, wxSizerFlags().CenterVertical().Border(wxLEFT, FromDIP(8)));
    bottom->AddStretchSpacer();
    bottom->Add(mRefresh, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(6)));
    bottom->Add(mSend, wxSizerFlags().CenterVertical());
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(mList, wxSizerFlags(1).Expand());
    sizer->Add(bottom, wxSizerFlags().Expand().Border(wxTOP, FromDIP(6)));
    SetSizer(sizer);
}

void GitHubPanel::SetReleases(const std::vector<ReleaseInfo>& releases)
{
    mReleases = releases;
    Redraw();
    // The newest stable release is the usual pick; the user can still choose any other.
    const auto stable = std::find_if(mReleases.begin(), mReleases.end(), [](const ReleaseInfo& release) { return release.stable; });
    if (stable != mReleases.end())
    {
        mList->SetSelection(static_cast<int>(stable - mReleases.begin()));
    }
    OnPick();
}

void GitHubPanel::SetMessage(const std::string& text)
{
    mInfo->SetLabel(wxString::FromUTF8(text));
    Layout();
}

void GitHubPanel::SetCarDaemon(const std::string& version, uint64_t size, const std::string& sha256)
{
    mCarVersion = version;
    mCarSize = size;
    mCarSha256 = sha256;
    Redraw();
}

const ReleaseInfo* GitHubPanel::Selected() const
{
    const int selected = mList->GetSelection();
    return selected >= 0 && static_cast<size_t>(selected) < mReleases.size() ? &mReleases[static_cast<size_t>(selected)] : nullptr;
}

void GitHubPanel::SetBusy(bool busy)
{
    mRefresh->Enable(!busy);
    mSend->Enable(!busy && Selected() != nullptr);
    Refresh();  // inside a static box, re-enabled buttons otherwise keep their grey look until hovered
}

void GitHubPanel::Redraw()
{
    const int selected = mList->GetSelection();
    mList->Clear();
    for (const ReleaseInfo& release : mReleases)
    {
        const bool byVersion = !mCarVersion.empty() && (mCarVersion == release.tag || (NbtVersion::Find(mCarVersion).valid && NbtVersion::Find(mCarVersion).Compare(NbtVersion::Find(release.tag)) == 0));
        const bool onCar = byVersion || GitHubReleases::Match({ release }, mCarSize, mCarSha256) != nullptr;
        mList->Append(wxString::FromUTF8(std::format("{}{}   {}{}", release.name, release.stable ? " [stable]" : "", release.Date(), onCar ? "   [on car]" : "")));
    }
    if (selected >= 0 && static_cast<size_t>(selected) < mReleases.size())
    {
        mList->SetSelection(selected);
    }
}

void GitHubPanel::OnPick()
{
    const ReleaseInfo* release = Selected();
    mSend->Enable(release != nullptr && mRefresh->IsEnabled());
    mNotes->Show(release != nullptr);
    if (release)
    {
        mInfo->SetLabel(wxString::FromUTF8(release->tag));
        mNotes->SetURL(wxString::FromUTF8(release->page));
    }
    Layout();
}

LocalPanel::LocalPanel(wxWindow* parent, std::function<void()> send) : wxPanel(parent)
{
    mPath = new wxTextCtrl(this, wxID_ANY);
    mPath->SetHint("A folder with nbt-aad and the 5 plugin files");
    mPath->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) { Inspect(); event.Skip(); });
    mFolder = new wxButton(this, wxID_ANY, "Folder...");
    mSend = new wxButton(this, wxID_ANY, "Send to car");
    mInfo = new wxStaticText(this, wxID_ANY, wxEmptyString);
    mFolder->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ChooseFolder(); });
    mSend->Bind(wxEVT_BUTTON, [send](wxCommandEvent&) { send(); });
    auto* hint = new wxStaticText(this, wxID_ANY, "Your own build, or files downloaded by hand. The files may sit in subfolders or inside a Releases.zip.");
    hint->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    hint->Wrap(FromDIP(420));

    auto* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(mPath, wxSizerFlags(1).CenterVertical().Border(wxRIGHT, FromDIP(6)));
    row->Add(mFolder, wxSizerFlags().CenterVertical());
    auto* bottom = new wxBoxSizer(wxHORIZONTAL);
    bottom->Add(mInfo, wxSizerFlags(1).CenterVertical());
    bottom->Add(mSend, wxSizerFlags().CenterVertical());
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(row, wxSizerFlags().Expand());
    sizer->Add(hint, wxSizerFlags().Border(wxTOP, FromDIP(6)));
    sizer->AddStretchSpacer();
    sizer->Add(bottom, wxSizerFlags().Expand().Border(wxTOP, FromDIP(6)));
    SetSizer(sizer);
}

void LocalPanel::SetPath(const std::string& path)
{
    mPath->ChangeValue(wxString::FromUTF8(path));
    Inspect();
}

std::string LocalPanel::Path() const
{
    return mPath->GetValue().Trim().Trim(false).utf8_string();
}

void LocalPanel::SetBusy(bool busy)
{
    for (wxWindow* control : { static_cast<wxWindow*>(mPath), static_cast<wxWindow*>(mFolder), static_cast<wxWindow*>(mSend) })
    {
        control->Enable(!busy);
    }
    Refresh();  // inside a static box, re-enabled buttons otherwise keep their grey look until hovered
}

void LocalPanel::ChooseFolder()
{
    wxDirDialog dialog(this, "Folder with nbt-aad and the plugin files", mPath->GetValue(), wxDD_DIR_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK)
    {
        SetPath(dialog.GetPath().utf8_string());
    }
}

void LocalPanel::Inspect()
{
    if (Path().empty())
    {
        mInfo->SetLabel(wxEmptyString);
        return;
    }
    // A quick look so the user sees the version before sending; Send to car collects again and logs it all.
    PackageFiles files;
    RunLog quiet;
    std::string error;
    std::string missing;
    const std::atomic<bool> never{ false };
    const bool ok = LocalSource(std::filesystem::path(wxString::FromUTF8(Path()).ToStdWstring())).Collect(files, quiet, never, error) && files.Complete(missing);
    mInfo->SetLabel(wxString::FromUTF8(ok ? "✔ all 6 files, " + (files.Version().empty() ? std::string("version unknown") : files.Version()) : "⚠ " + (error.empty() ? missing : error)));
    mInfo->SetForegroundColour(ok ? wxColour(0x16, 0xA3, 0x4A) : wxColour(0xD9, 0x77, 0x06));
    mInfo->Wrap(FromDIP(330));
    Layout();
}

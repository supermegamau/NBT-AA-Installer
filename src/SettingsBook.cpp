// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include "SettingsBook.h"

#include <wx/checkbox.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/scrolwin.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/stc/stc.h>

#include <algorithm>
#include <format>
#include <numeric>

namespace
{
    enum Marker : int32_t
    {
        kErrorMarker = 0,
        kWarningMarker = 1,
        kInfoMarker = 2
    };

    const char* Symbol(LogLevel level)
    {
        return level == LogLevel::Error ? "⛔" : level == LogLevel::Warning ? "⚠" : "ⓘ";
    }
}

SettingsBook::SettingsBook(wxWindow* parent, const SettingsCatalog& catalog) : wxPanel(parent), mCatalog(catalog), mEnv(catalog)
{
    mCounter = new wxStaticText(this, wxID_ANY, wxEmptyString);
    mCounter->SetFont(mCounter->GetFont().Bold());
    mAdvanced = new wxCheckBox(this, wxID_ANY, "Show advanced settings");
    mAdvanced->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { Redraw(false); });
    auto* note = new wxStaticText(this, wxID_ANY, wxString::Format("The car passes only the first %u settings in the file to Android Auto, and cuts lines over %u characters. Settings left on Default are not written to the file.", static_cast<uint32_t>(catalog.MaxSettings()), static_cast<uint32_t>(catalog.MaxLineLength())));
    note->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    mStatus = new wxStaticText(this, wxID_ANY, wxEmptyString);
    mStatus->SetForegroundColour(wxColour(0xD9, 0x77, 0x06));

    auto* book = new wxNotebook(this, wxID_ANY);
    for (const TabDef& tab : catalog.Tabs())
    {
        mPages.push_back(BuildTab(book, tab));
    }
    book->AddPage(BuildFileTab(book), "File");
    mEditTimer.Bind(wxEVT_TIMER, [this](wxTimerEvent&) { OnFileEdited(); });

    auto* top = new wxBoxSizer(wxHORIZONTAL);
    top->Add(mCounter, wxSizerFlags().CenterVertical());
    top->AddStretchSpacer();
    top->Add(mAdvanced, wxSizerFlags().CenterVertical());
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(top, wxSizerFlags().Expand());
    sizer->Add(note, wxSizerFlags().Border(wxTOP | wxBOTTOM, FromDIP(4)));
    sizer->Add(book, wxSizerFlags(1).Expand());
    sizer->Add(mStatus, wxSizerFlags().Border(wxTOP, FromDIP(4)));
    SetSizer(sizer);
    Load(std::string(), {});
}

wxScrolledWindow* SettingsBook::BuildTab(wxNotebook* book, const TabDef& tab)
{
    auto* page = new wxScrolledWindow(book, wxID_ANY);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    if (tab.locked)
    {
        mUnlockBox = new wxCheckBox(page, wxID_ANY, "Let these settings override iDrive's Android Auto options");
        mUnlockBox->SetFont(mUnlockBox->GetFont().Bold());
        mUnlockBox->Bind(wxEVT_CHECKBOX, &SettingsBook::OnUnlock, this);
        mUnlockNote = new wxStaticText(page, wxID_ANY, wxEmptyString);
        sizer->Add(mUnlockBox, wxSizerFlags().Border(wxALL, FromDIP(8)));
        sizer->Add(mUnlockNote, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(8)));
    }
    for (const SettingDef& setting : mCatalog.Settings())
    {
        if (setting.tab == tab.id)
        {
            mControls.push_back(new SettingControl(page, setting, [this](const std::string& key, const std::optional<std::string>& value) { return Apply(key, value); }));
            sizer->Add(mControls.back(), wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(4)));
        }
    }
    page->SetSizer(sizer);
    page->SetScrollRate(0, FromDIP(20));
    book->AddPage(page, wxString::FromUTF8(tab.label));
    return page;
}

wxPanel* SettingsBook::BuildFileTab(wxNotebook* book)
{
    auto* page = new wxPanel(book);
    mFile = new wxStyledTextCtrl(page, wxID_ANY);
    mFile->StyleSetFont(wxSTC_STYLE_DEFAULT, wxFont(wxFontInfo(10).FaceName("Courier New").Family(wxFONTFAMILY_TELETYPE)));
    mFile->StyleClearAll();
    mFile->SetLexer(wxSTC_LEX_PROPERTIES);  // NAME=value files: comments, names and values get their own colours
    mFile->StyleSetForeground(wxSTC_PROPS_COMMENT, wxColour(0x6B, 0x72, 0x80));
    mFile->StyleSetForeground(wxSTC_PROPS_KEY, wxColour(0x1D, 0x4E, 0xD8));
    mFile->StyleSetForeground(wxSTC_PROPS_ASSIGNMENT, wxColour(0x6B, 0x72, 0x80));
    mFile->SetMarginType(0, wxSTC_MARGIN_NUMBER);
    mFile->SetMarginWidth(0, mFile->TextWidth(wxSTC_STYLE_LINENUMBER, "_999"));
    mFile->SetMarginType(1, wxSTC_MARGIN_SYMBOL);
    mFile->SetMarginWidth(1, FromDIP(16));
    mFile->MarkerDefine(kErrorMarker, wxSTC_MARK_CIRCLE, wxColour(0xDC, 0x26, 0x26), wxColour(0xDC, 0x26, 0x26));
    mFile->MarkerDefine(kWarningMarker, wxSTC_MARK_CIRCLE, wxColour(0xD9, 0x77, 0x06), wxColour(0xD9, 0x77, 0x06));
    mFile->MarkerDefine(kInfoMarker, wxSTC_MARK_CIRCLE, wxColour(0x3B, 0x82, 0xF6), wxColour(0x3B, 0x82, 0xF6));
    mFile->SetEdgeMode(wxSTC_EDGE_LINE);
    mFile->SetEdgeColumn(mCatalog.MaxLineLength());  // the car's cut-off, visible while typing
    mFile->SetEOLMode(wxSTC_EOL_LF);
    mFile->Bind(wxEVT_STC_CHANGE, [this](wxStyledTextEvent&)
    {
        if (!mSettingText)
        {
            mEditTimer.StartOnce(kEditDelayMs);
        }
    });
    mFindings = new wxListBox(page, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(110)));
    mFindings->Bind(wxEVT_LISTBOX, &SettingsBook::OnFindingPicked, this);
    auto* hint = new wxStaticText(page, wxID_ANY, "Type anything here: lines the tool doesn't know are kept as they are. Problems are marked, never fixed for you.");
    hint->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(hint, wxSizerFlags().Border(wxALL, FromDIP(4)));
    sizer->Add(mFile, wxSizerFlags(1).Expand());
    sizer->Add(mFindings, wxSizerFlags().Expand().Border(wxTOP, FromDIP(4)));
    page->SetSizer(sizer);
    return page;
}

void SettingsBook::Load(const std::string& carText, const std::vector<std::string>& dataFiles)
{
    mEditTimer.Stop();
    mEnv.SetText(carText);
    mLoaded = mEnv.Text();  // normalised, so CRLF or a missing last newline doesn't count as a change of ours
    mDataFiles = dataFiles;
    mUnlocked = !ActiveOverrides().empty();
    mUnlockBox->SetValue(mUnlocked);
    mUnlockNote->SetLabel(mUnlocked ? "The car's file already overrides iDrive options, so this tab is open. Untick to turn those lines off (kept as #~off comments)." : "Locked: these settings would override the Android Auto options in iDrive's Settings page.");
    Report(std::string());
    Redraw(true);
    mFile->EmptyUndoBuffer();
}

void SettingsBook::Flush()
{
    if (mEditTimer.IsRunning())
    {
        mEditTimer.Stop();
        OnFileEdited();
    }
}

std::vector<std::string> SettingsBook::ActiveOverrides() const
{
    std::vector<std::string> keys;
    for (const TabDef& tab : mCatalog.Tabs())
    {
        for (const SettingDef& setting : mCatalog.Settings())
        {
            if (tab.locked && setting.tab == tab.id && mEnv.Value(setting.key))
            {
                keys.push_back(setting.key);
            }
        }
    }
    return keys;
}

void SettingsBook::ShowAdvanced(bool show)
{
    mAdvanced->SetValue(show);
    Redraw(false);
}

bool SettingsBook::AdvancedShown() const
{
    return mAdvanced->IsChecked();
}

bool SettingsBook::Apply(const std::string& key, const std::optional<std::string>& value)
{
    Flush();
    bool done = true;
    if (value && key.size() + 1 + value->size() > mCatalog.MaxLineLength())
    {
        Report(std::format("{}: the line would be longer than {} characters, which the car cuts in two.", key, mCatalog.MaxLineLength()));
        done = false;
    }
    else if (value && !mEnv.CanAdd(key))
    {
        Report(std::format("Already {} settings the car reads: set another one back to Default first.", mCatalog.MaxSettings()));
        done = false;
    }
    else
    {
        value ? mEnv.Set(key, *value) : mEnv.Remove(key);
        Report(std::string());
    }
    CallAfter([this, done]() { Redraw(done); });  // not inside the control's own event: its choice list is rebuilt
    return done;
}

void SettingsBook::OnUnlock(wxCommandEvent&)
{
    Flush();
    std::vector<std::string> refused;
    if (mUnlockBox->IsChecked())
    {
        const auto locked = std::find_if(mCatalog.Tabs().begin(), mCatalog.Tabs().end(), [](const TabDef& tab) { return tab.locked; });
        if (wxMessageBox(wxString::FromUTF8(locked->unlockWarning + "\n\nUnlock them?"), "iDrive overrides", wxOK | wxCANCEL | wxICON_WARNING, this) != wxOK)
        {
            mUnlockBox->SetValue(false);
            return;
        }
        mUnlocked = true;
        for (const SettingDef& setting : mCatalog.Settings())
        {
            if (setting.tab == locked->id && mEnv.HasOff(setting.key) && !mEnv.TurnOn(setting.key))
            {
                refused.push_back(setting.key);
            }
        }
        Report(refused.empty() ? std::string() : "Still turned off (an active line for it already exists, or it would be past the 15th setting): " + std::accumulate(refused.begin(), refused.end(), std::string(), [](const std::string& list, const std::string& key) { return list.empty() ? key : list + ", " + key; }));
    }
    else
    {
        mUnlocked = false;
        for (const std::string& key : ActiveOverrides())
        {
            if (!mEnv.TurnOff(key))
            {
                refused.push_back(key);
            }
        }
        Report(refused.empty() ? std::string() : "Removed instead of turned off (the #~off line would pass 159 characters): set them again by hand if needed.");
        for (const std::string& key : refused)
        {
            mEnv.Remove(key);
        }
    }
    mUnlockNote->SetLabel(mUnlocked ? "These settings now win over the Android Auto options in iDrive." : "Locked: lines for these settings are kept as #~off comments and come back when you tick the box again.");
    Redraw(true);
}

void SettingsBook::OnFileEdited()
{
    mEnv.SetText(mFile->GetText().utf8_string());
    Redraw(false);
}

void SettingsBook::OnFindingPicked(wxCommandEvent&)
{
    const int32_t picked = mFindings->GetSelection();
    if (picked >= 0 && static_cast<size_t>(picked) < mFindingLines.size())
    {
        mFile->GotoLine(mFindingLines[static_cast<size_t>(picked)]);
        mFile->SetFocus();
    }
}

void SettingsBook::Redraw(bool fileText)
{
    if (fileText)
    {
        mSettingText = true;
        mFile->SetText(wxString::FromUTF8(mEnv.Text()));
        mSettingText = false;
    }
    const auto locked = std::find_if(mCatalog.Tabs().begin(), mCatalog.Tabs().end(), [](const TabDef& tab) { return tab.locked; });
    for (SettingControl* control : mControls)
    {
        const SettingDef& setting = control->Setting();
        control->ShowValue(mEnv.Value(setting.key), mEnv.HasOff(setting.key), !setting.flagFile.empty() && std::find(mDataFiles.begin(), mDataFiles.end(), setting.flagFile) != mDataFiles.end());
        control->Enable(locked == mCatalog.Tabs().end() || setting.tab != locked->id || mUnlocked);
        control->Show(!setting.advanced || mAdvanced->IsChecked() || mEnv.Value(setting.key).has_value());  // a setting in the file is always shown
    }
    for (wxScrolledWindow* page : mPages)
    {
        page->FitInside();
        page->Layout();
    }
    const uint8_t used = mEnv.SlotsUsed();
    mCounter->SetLabel(wxString::Format("Settings the car reads: %u / %u", static_cast<uint32_t>(used), static_cast<uint32_t>(mCatalog.MaxSettings())));
    mCounter->SetForegroundColour(used > mCatalog.MaxSettings() ? wxColour(0xDC, 0x26, 0x26) : wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    RedrawFileMarkers();
    Layout();
}

void SettingsBook::RedrawFileMarkers()
{
    mFile->MarkerDeleteAll(-1);
    mFindings->Clear();
    mFindingLines.clear();
    for (const EnvFinding& finding : mEnv.Findings())
    {
        mFile->MarkerAdd(finding.line, finding.level == LogLevel::Error ? kErrorMarker : finding.level == LogLevel::Warning ? kWarningMarker : kInfoMarker);
        mFindings->Append(wxString::FromUTF8(std::format("{} line {}: {}", Symbol(finding.level), finding.line + 1, finding.text)));
        mFindingLines.push_back(finding.line);
    }
}

void SettingsBook::Report(const std::string& text)
{
    mStatus->SetLabel(wxString::FromUTF8(text));
    if (!text.empty())
    {
        wxBell();
    }
}

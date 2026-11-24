// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include "EnvFile.h"
#include "SettingControl.h"
#include "SettingsCatalog.h"

#include <wx/panel.h>
#include <wx/timer.h>

#include <optional>
#include <string>
#include <vector>

class wxCheckBox;
class wxListBox;
class wxNotebook;
class wxScrolledWindow;
class wxStaticText;
class wxStyledTextCtrl;

// The settings tabs and the File tab: all views of one EnvFile. The file text is what counts.
class SettingsBook : public wxPanel
{
public:
    SettingsBook(wxWindow* parent, const SettingsCatalog& catalog);

    void Load(const std::string& carText, const std::vector<std::string>& dataFiles);  // what Connect read
    void Flush();                                                                       // take in File tab typing that is still waiting
    std::string Text() const { return mEnv.Text(); }
    bool Changed() const { return mEnv.Text() != mLoaded; }
    const EnvFile& Env() const { return mEnv; }
    bool OverridesUnlocked() const { return mUnlocked; }
    std::vector<std::string> ActiveOverrides() const;
    void ShowAdvanced(bool show);
    bool AdvancedShown() const;

private:
    wxScrolledWindow* BuildTab(wxNotebook* book, const TabDef& tab);
    wxPanel* BuildFileTab(wxNotebook* book);
    bool Apply(const std::string& key, const std::optional<std::string>& value);
    void OnUnlock(wxCommandEvent& event);
    void OnFileEdited();
    void OnFindingPicked(wxCommandEvent& event);
    void Redraw(bool fileText);
    void RedrawFileMarkers();
    void Report(const std::string& text);

    static constexpr int32_t kEditDelayMs = 400;  // re-read the File tab once typing pauses, not on every key

    const SettingsCatalog&       mCatalog;
    EnvFile                      mEnv;
    std::string                  mLoaded;
    std::vector<std::string>     mDataFiles;       // flag files: which iDrive boxes are ticked
    std::vector<SettingControl*> mControls;
    std::vector<wxScrolledWindow*> mPages;
    wxStaticText*                mCounter = nullptr;
    wxStaticText*                mStatus = nullptr;
    wxCheckBox*                  mAdvanced = nullptr;
    wxCheckBox*                  mUnlockBox = nullptr;
    wxStaticText*                mUnlockNote = nullptr;
    wxStyledTextCtrl*            mFile = nullptr;
    wxListBox*                   mFindings = nullptr;
    std::vector<uint16_t>        mFindingLines;
    wxTimer                      mEditTimer;
    bool                         mUnlocked = false;
    bool                         mSettingText = false;  // our own SetText must not count as the user typing
};

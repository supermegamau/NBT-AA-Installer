// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include "SettingsCatalog.h"

#include <wx/panel.h>

#include <functional>
#include <optional>
#include <string>

class wxChoice;
class wxStaticText;
class wxTextCtrl;

// One setting's row: label, a control for its type, a ⚠ / ⓘ marker and the description.
class SettingControl : public wxPanel
{
public:
    using Apply = std::function<bool(const std::string& key, const std::optional<std::string>& value)>;  // nullopt = Default; false = refused

    SettingControl(wxWindow* parent, const SettingDef& setting, Apply apply);

    const SettingDef& Setting() const { return mSetting; }
    void ShowValue(const std::optional<std::string>& value, bool turnedOff, bool idriveTicked);  // not Show(): that is wxWindow's

private:
    void OnChoice(wxCommandEvent& event);
    void OnTextDone();
    void Mark(const std::string& symbol, const std::string& tip);
    std::string DefaultLabel(bool idriveTicked) const;
    bool IsChoice() const;

    const SettingDef& mSetting;
    Apply             mApply;
    wxChoice*         mChoice = nullptr;   // bool and select
    wxTextCtrl*       mText = nullptr;     // numbers and text
    wxStaticText*     mMarker = nullptr;
    std::string       mShown;              // the text field's value as last shown, to spot a real edit
    bool              mHasRaw = false;     // the choice list ends with the file's invalid value, kept until the user picks
};

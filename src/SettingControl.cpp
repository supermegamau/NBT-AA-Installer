// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include "SettingControl.h"

#include <wx/choice.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace
{
    constexpr int32_t kLabelWidth = 470;  // name plus (NBT_...) for most settings; longer names are cut with "..."
    constexpr int32_t kControlWidth = 320;
}

SettingControl::SettingControl(wxWindow* parent, const SettingDef& setting, Apply apply) : wxPanel(parent), mSetting(setting), mApply(std::move(apply))
{
    // The name, then the variable as it appears in nbt-aad.env, so the File tab and the tabs can be matched up.
    auto* name = new wxStaticText(this, wxID_ANY, wxString::FromUTF8(setting.label), wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
    name->SetFont(name->GetFont().Bold());
    auto* key = new wxStaticText(this, wxID_ANY, wxString::FromUTF8("(" + setting.key + ")"));
    key->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    auto* label = new wxBoxSizer(wxHORIZONTAL);
    label->Add(name, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(4)));
    label->Add(key, wxSizerFlags().CenterVertical());
    label->SetMinSize(FromDIP(wxSize(kLabelWidth, -1)));
    if (IsChoice())
    {
        mChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(kControlWidth), -1));
        mChoice->Bind(wxEVT_CHOICE, &SettingControl::OnChoice, this);
    }
    else
    {
        mText = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(kControlWidth), -1), wxTE_PROCESS_ENTER);
        mText->SetHint(wxString::FromUTF8(DefaultLabel(false)));
        mText->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { OnTextDone(); });
        mText->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) { OnTextDone(); event.Skip(); });
    }
    mMarker = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(24), -1));
    // The explanation only as a tooltip, so a tab shows many settings at once.
    const wxString tip = wxString::FromUTF8(setting.description.empty() ? setting.key : setting.description);
    for (wxWindow* window : { static_cast<wxWindow*>(this), static_cast<wxWindow*>(name), static_cast<wxWindow*>(key), mChoice ? static_cast<wxWindow*>(mChoice) : static_cast<wxWindow*>(mText) })
    {
        window->SetToolTip(tip);
    }

    auto* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(label, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(8)));
    row->Add(mChoice ? static_cast<wxWindow*>(mChoice) : static_cast<wxWindow*>(mText), wxSizerFlags().CenterVertical());
    row->Add(mMarker, wxSizerFlags().CenterVertical().Border(wxLEFT, FromDIP(6)));
    if (!setting.unit.empty())
    {
        row->Add(new wxStaticText(this, wxID_ANY, wxString::FromUTF8(setting.unit)), wxSizerFlags().CenterVertical());
    }
    SetSizer(row);
}

void SettingControl::ShowValue(const std::optional<std::string>& value, bool turnedOff, bool idriveTicked)
{
    const std::string problem = value ? mSetting.Problem(*value) : std::string();
    if (mChoice)
    {
        mChoice->Clear();
        mChoice->Append(wxString::FromUTF8(DefaultLabel(idriveTicked)));
        if (mSetting.type == SettingType::Bool)
        {
            mChoice->Append(wxString::FromUTF8(mSetting.onLabel));
            mChoice->Append(wxString::FromUTF8(mSetting.offLabel));
        }
        for (const SettingChoice& choice : mSetting.values)
        {
            mChoice->Append(wxString::FromUTF8(choice.label));
        }
        int32_t selected = 0;
        if (value && mSetting.type == SettingType::Bool && problem.empty())
        {
            selected = SettingDef::IsOnWord(*value) ? 1 : 2;
        }
        for (size_t i = 0; value && mSetting.type == SettingType::Select && i < mSetting.values.size(); ++i)
        {
            selected = mSetting.values[i].value == *value ? static_cast<int32_t>(i) + 1 : selected;
        }
        mHasRaw = value && !problem.empty();
        if (mHasRaw)
        {
            selected = static_cast<int32_t>(mChoice->Append(wxString::FromUTF8("⚠ " + *value)));  // shown as written: the file is only changed when the user picks
        }
        mChoice->SetSelection(selected);
    }
    else
    {
        mShown = value.value_or(std::string());
        mText->ChangeValue(wxString::FromUTF8(mShown));
        mText->SetHint(wxString::FromUTF8(DefaultLabel(idriveTicked)));
    }
    Mark(!problem.empty() ? "⚠" : turnedOff ? "⏸" : std::string(), !problem.empty() ? "The file says \"" + *value + "\": " + problem + ".\nThe file is only changed when you use this control." : turnedOff ? std::string("Turned off (#~off in the file): ticking the iDrive overrides box brings it back.") : std::string());
}

void SettingControl::OnChoice(wxCommandEvent&)
{
    const int32_t index = mChoice->GetSelection();
    const bool isRaw = mHasRaw && index == static_cast<int32_t>(mChoice->GetCount()) - 1;
    if (isRaw)
    {
        return;  // re-picking the file's own value changes nothing
    }
    std::optional<std::string> value;
    if (index > 0 && mSetting.type == SettingType::Bool)
    {
        value = index == 1 ? "1" : "0";
    }
    else if (index > 0)
    {
        value = mSetting.values[static_cast<size_t>(index) - 1].value;
    }
    mApply(mSetting.key, value);  // the book redraws every control from the file afterwards, refused or not
}

void SettingControl::OnTextDone()
{
    const std::string typed = mText->GetValue().Trim().Trim(false).utf8_string();
    if (typed == mShown)
    {
        return;
    }
    const std::string problem = typed.empty() ? std::string() : mSetting.Problem(typed);
    if (!problem.empty())
    {
        Mark("⚠", "\"" + typed + "\" is not saved: " + problem + ".");
        return;
    }
    mApply(mSetting.key, typed.empty() ? std::nullopt : std::optional<std::string>(typed));
}

void SettingControl::Mark(const std::string& symbol, const std::string& tip)
{
    mMarker->SetLabel(wxString::FromUTF8(symbol));
    mMarker->SetForegroundColour(symbol == "⚠" ? wxColour(0xD9, 0x77, 0x06) : wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
    mMarker->SetToolTip(wxString::FromUTF8(tip));
}

std::string SettingControl::DefaultLabel(bool idriveTicked) const
{
    return "Default: " + mSetting.unset + (mSetting.flagFile.empty() ? std::string() : idriveTicked ? " · iDrive: ticked" : " · iDrive: not ticked");
}

bool SettingControl::IsChoice() const
{
    return mSetting.type == SettingType::Bool || mSetting.type == SettingType::Select;
}

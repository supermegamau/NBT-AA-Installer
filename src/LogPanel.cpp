// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include "LogPanel.h"

#include <wx/button.h>
#include <wx/clipbrd.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>

LogPanel::LogPanel(wxWindow* parent) : wxPanel(parent)
{
    mText = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxTE_DONTWRAP);
    mText->SetFont(wxFont(wxFontInfo(9).FaceName("Courier New").Family(wxFONTFAMILY_TELETYPE)));  // fixed width keeps ls -l columns aligned
    mText->Bind(wxEVT_KEY_DOWN, &LogPanel::OnKeyDown, this);

    auto* copy = new wxButton(this, wxID_ANY, "Copy");
    auto* clear = new wxButton(this, wxID_ANY, "Clear");
    copy->Bind(wxEVT_BUTTON, &LogPanel::OnCopy, this);
    clear->Bind(wxEVT_BUTTON, &LogPanel::OnClear, this);

    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    buttons->AddStretchSpacer();
    buttons->Add(copy, wxSizerFlags().Border(wxRIGHT));
    buttons->Add(clear);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(mText, wxSizerFlags(1).Expand());
    sizer->Add(buttons, wxSizerFlags().Expand().Border(wxTOP));
    SetSizer(sizer);
}

void LogPanel::Append(LogLevel level, const std::string& line)
{
    CallAfter([this, level, text = ToWx(line)]() { AppendNow(level, text); });
}

void LogPanel::AppendNow(LogLevel level, const wxString& line)
{
    // The font goes in with every line: a style with only a weight makes RichEdit fall back to the UI font.
    mText->SetDefaultStyle(wxTextAttr(Colour(level), wxNullColour, level == LogLevel::Step || level == LogLevel::Error ? mText->GetFont().Bold() : mText->GetFont()));
    mText->AppendText(line + "\n");
}

wxColour LogPanel::Colour(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Step:    return wxColour(0x3B, 0x82, 0xF6);
    case LogLevel::Command: return wxColour(0x8B, 0x5C, 0xF6);
    case LogLevel::Output:  return wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
    case LogLevel::Pass:    return wxColour(0x16, 0xA3, 0x4A);
    case LogLevel::Warning: return wxColour(0xD9, 0x77, 0x06);
    case LogLevel::Error:   return wxColour(0xDC, 0x26, 0x26);
    default:                return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    }
}

void LogPanel::OnCopy(wxCommandEvent&)
{
    if (wxTheClipboard->Open())
    {
        wxTheClipboard->SetData(new wxTextDataObject(mText->GetValue()));
        wxTheClipboard->Close();
    }
}

void LogPanel::OnClear(wxCommandEvent&)
{
    mText->Clear();  // the window only: the log file keeps everything
}

void LogPanel::OnKeyDown(wxKeyEvent& event)
{
    // A read-only RichEdit ignores Ctrl+A, so select all here; every other key keeps its normal handling.
    if (event.GetModifiers() == wxMOD_CONTROL && event.GetKeyCode() == 'A')
    {
        mText->SelectAll();
        return;
    }
    event.Skip();
}

wxString LogPanel::ToWx(const std::string& text)
{
    // car output is not always UTF-8, and FromUTF8 gives up on the whole line rather than one character
    const wxString utf8 = wxString::FromUTF8(text);
    return utf8.empty() && !text.empty() ? wxString::From8BitData(text.c_str(), text.size()) : utf8;
}

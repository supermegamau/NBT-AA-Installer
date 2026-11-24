// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include "LogLevel.h"

#include <wx/panel.h>

#include <string>

class wxTextCtrl;

// Live log view, meant to sit full width at the bottom of a window. Shared with NBT-AA-Installer.
class LogPanel : public wxPanel
{
public:
    explicit LogPanel(wxWindow* parent);

    void Append(LogLevel level, const std::string& line);  // any thread

private:
    void AppendNow(LogLevel level, const wxString& line);
    void OnCopy(wxCommandEvent& event);
    void OnClear(wxCommandEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    static wxColour Colour(LogLevel level);
    static wxString ToWx(const std::string& text);

    wxTextCtrl* mText = nullptr;
};

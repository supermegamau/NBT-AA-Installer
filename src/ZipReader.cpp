// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include "ZipReader.h"

#include <wx/mstream.h>
#include <wx/zipstrm.h>

#include <memory>

bool ZipReader::Read(const std::string& zipBytes, const Filter& wanted, std::vector<ZipEntry>& entries, std::string& error)
{
    wxMemoryInputStream memory(zipBytes.data(), zipBytes.size());
    wxZipInputStream zip(memory);
    if (!zip.IsOk())
    {
        error = "not a zip file";
        return false;
    }
    for (std::unique_ptr<wxZipEntry> entry(zip.GetNextEntry()); entry; entry.reset(zip.GetNextEntry()))
    {
        const std::string path = entry->GetName(wxPATH_UNIX).utf8_string();
        if (entry->IsDir() || !wanted(path.substr(path.find_last_of('/') + 1)))
        {
            continue;
        }
        std::string data;
        char buffer[65536];
        while (zip.Read(buffer, sizeof(buffer)).LastRead() > 0)
        {
            data.append(buffer, zip.LastRead());
        }
        if (zip.GetLastError() != wxSTREAM_EOF && zip.GetLastError() != wxSTREAM_NO_ERROR)
        {
            error = "cannot unpack " + path + " (damaged zip?)";
            return false;
        }
        entries.push_back(ZipEntry{ path, data });
    }
    return true;
}

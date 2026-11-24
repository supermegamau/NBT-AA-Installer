// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include "DirListing.h"

#include <algorithm>
#include <sstream>

DirListing::DirListing(const std::string& lsOutput)
{
    std::istringstream lines(lsOutput);
    std::string line;
    while (std::getline(lines, line))
    {
        // "-rwxr-xr-x  1 root  root  2184512 Nov 19 18:00 nbt-aad": mode, links, owner, group, size, 3 date fields, name
        std::istringstream fields(line);
        std::string mode, links, owner, group, date1, date2, date3;
        DirEntry entry;
        if (!(fields >> mode >> links >> owner >> group >> entry.size >> date1 >> date2 >> date3) || mode.size() < 10)
        {
            continue;  // "total 12", error lines and anything unexpected
        }
        std::getline(fields >> std::ws, entry.name);
        entry.name = entry.name.substr(0, entry.name.find(" -> "));  // a symlink keeps its own name
        if (entry.name == "." || entry.name == "..")
        {
            continue;  // QNX's ls -l lists these too; they would count as extra files
        }
        entry.directory = mode[0] == 'd';
        entry.executable = mode[3] == 'x' || mode[6] == 'x' || mode[9] == 'x';
        mEntries.push_back(entry);
    }
}

std::optional<DirEntry> DirListing::Find(const std::string& name) const
{
    const auto found = std::find_if(mEntries.begin(), mEntries.end(), [&name](const DirEntry& entry) { return entry.name == name; });
    return found != mEntries.end() ? std::optional<DirEntry>(*found) : std::nullopt;
}

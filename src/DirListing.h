// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct DirEntry
{
    std::string name;
    uint64_t    size = 0;
    bool        executable = false;
    bool        directory = false;
};

// Parses `ls -l` output from the car.
class DirListing
{
public:
    explicit DirListing(const std::string& lsOutput);

    std::optional<DirEntry> Find(const std::string& name) const;
    const std::vector<DirEntry>& Entries() const { return mEntries; }

private:
    std::vector<DirEntry> mEntries;
};

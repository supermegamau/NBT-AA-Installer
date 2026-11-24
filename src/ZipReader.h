// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include <functional>
#include <string>
#include <vector>

struct ZipEntry
{
    std::string path;  // inside the zip, '/' separated
    std::string data;
};

// Reads files out of a zip in memory with wxWidgets' zip stream (zlib is already inside wx).
class ZipReader
{
public:
    using Filter = std::function<bool(const std::string& fileName)>;  // decides from the bare file name, so huge zips only cost what we keep

    static bool Read(const std::string& zipBytes, const Filter& wanted, std::vector<ZipEntry>& entries, std::string& error);
};

// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct PackageFile
{
    std::string name;
    std::string data;
    std::string origin;  // where it was found, for the log and for "two different copies" errors
};

// "v1.9.112-cluster" -> 1.9.112, for comparing a release with what the car runs.
struct NbtVersion
{
    uint16_t major = 0;
    uint16_t minor = 0;
    uint16_t patch = 0;
    bool     valid = false;

    static NbtVersion Find(const std::string& text);
    int32_t Compare(const NbtVersion& other) const;  // <0 older, 0 same, >0 newer
};

// The six files NBT-AAD consists of, held in memory (about 1.3 MB) from collection until upload.
class PackageFiles
{
public:
    static constexpr const char* kDaemonName = "nbt-aad";
    static constexpr const char* kPluginNames[] =
    {
        "libNbtAad.so",
        "NbtAad.manifest",
        "NbtAad.xml",
        "NbtAad_ALL_Texts.zip",
        "NbtAad_ALL_Images.zip"
    };

    static bool IsWanted(const std::string& name);
    bool Add(const std::string& name, const std::string& data, const std::string& origin, std::string& error);
    bool Complete(std::string& missing) const;
    const PackageFile* Get(const std::string& name) const;
    const std::vector<PackageFile>& Files() const { return mFiles; }
    std::string Version() const;

    static std::string VersionIn(const std::string& binary);  // the daemon carries "NBT-AAD-VERSION=<build>"
    static std::string Sha256(const std::string& data);
    static uint32_t Cksum(const std::string& data);           // POSIX cksum, to compare with the car's own cksum

private:
    std::vector<PackageFile> mFiles;
};

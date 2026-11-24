// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include <windows.h>
#include <bcrypt.h>

#include "PackageFiles.h"

#include <algorithm>
#include <format>
#include <regex>

NbtVersion NbtVersion::Find(const std::string& text)
{
    std::smatch match;
    static const std::regex kPattern("v([0-9]+)\\.([0-9]+)\\.([0-9]+)");
    if (!std::regex_search(text, match, kPattern))
    {
        return NbtVersion{};
    }
    return NbtVersion{ static_cast<uint16_t>(std::stoul(match[1].str())), static_cast<uint16_t>(std::stoul(match[2].str())), static_cast<uint16_t>(std::stoul(match[3].str())), true };
}

int32_t NbtVersion::Compare(const NbtVersion& other) const
{
    return major != other.major ? major - other.major : minor != other.minor ? minor - other.minor : patch - other.patch;
}

bool PackageFiles::IsWanted(const std::string& name)
{
    return name == kDaemonName || std::any_of(std::begin(kPluginNames), std::end(kPluginNames), [&name](const char* plugin) { return name == plugin; });
}

bool PackageFiles::Add(const std::string& name, const std::string& data, const std::string& origin, std::string& error)
{
    const auto found = std::find_if(mFiles.begin(), mFiles.end(), [&name](const PackageFile& file) { return file.name == name; });
    if (found == mFiles.end())
    {
        mFiles.push_back(PackageFile{ name, data, origin });
        return true;
    }
    if (found->data == data)
    {
        return true;  // the same file twice (loose and inside Releases.zip) is no question
    }
    error = std::format("two different copies of {}: {} and {}. Pick a folder or zip with only one.", name, found->origin, origin);
    return false;
}

bool PackageFiles::Complete(std::string& missing) const
{
    missing.clear();
    for (const char* name : { kDaemonName, kPluginNames[0], kPluginNames[1], kPluginNames[2], kPluginNames[3], kPluginNames[4] })
    {
        const PackageFile* file = Get(name);
        missing += !file ? std::string(missing.empty() ? "" : ", ") + name + " missing" : file->data.empty() ? std::string(missing.empty() ? "" : ", ") + name + " is empty" : std::string();
    }
    return missing.empty();
}

const PackageFile* PackageFiles::Get(const std::string& name) const
{
    const auto found = std::find_if(mFiles.begin(), mFiles.end(), [&name](const PackageFile& file) { return file.name == name; });
    return found != mFiles.end() ? &*found : nullptr;
}

std::string PackageFiles::Version() const
{
    const PackageFile* daemon = Get(kDaemonName);
    return daemon ? VersionIn(daemon->data) : std::string();
}

std::string PackageFiles::VersionIn(const std::string& binary)
{
    static const std::string kMark = "NBT-AAD-VERSION=";
    const size_t at = binary.find(kMark);
    if (at == std::string::npos)
    {
        return std::string();
    }
    const size_t start = at + kMark.size();
    const auto end = std::find_if(binary.begin() + static_cast<ptrdiff_t>(start), binary.end(), [](char c) { return c < 0x21 || c > 0x7E; });
    return binary.substr(start, static_cast<size_t>(end - binary.begin()) - start);
}

std::string PackageFiles::Sha256(const std::string& data)
{
    uint8_t hash[32] = {};
    if (BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())), static_cast<ULONG>(data.size()), hash, sizeof(hash)) != 0)
    {
        return std::string();
    }
    std::string text;
    for (uint8_t byte : hash)
    {
        text += std::format("{:02x}", byte);
    }
    return text;
}

uint32_t PackageFiles::Cksum(const std::string& data)
{
    // POSIX cksum: CRC-32 (polynomial 0x04C11DB7, MSB first) over the bytes, then over the length's bytes, inverted.
    uint32_t crc = 0;
    const auto feed = [&crc](uint8_t byte)
    {
        crc ^= static_cast<uint32_t>(byte) << 24;
        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            crc = crc & 0x80000000u ? (crc << 1) ^ 0x04C11DB7u : crc << 1;
        }
    };
    for (char c : data)
    {
        feed(static_cast<uint8_t>(c));
    }
    for (uint64_t length = data.size(); length; length >>= 8)
    {
        feed(static_cast<uint8_t>(length & 0xFF));
    }
    return ~crc;
}

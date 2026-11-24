// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include "PackageSource.h"

#include "HttpClient.h"
#include "ZipReader.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <sstream>

namespace
{
    constexpr const char* kReleasesZip = "Releases.zip";  // the plugin files, as NBT-AAD's releases and USB-stick guide ship them

    bool ReadFile(const std::filesystem::path& path, std::string& data)
    {
        std::ifstream file(path, std::ios::binary);
        std::stringstream text;
        text << file.rdbuf();
        data = text.str();
        return static_cast<bool>(file);
    }

    bool WriteFile(const std::filesystem::path& path, const std::string& data)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
        return static_cast<bool>(file);
    }

    // Tags can hold characters Windows forbids in folder names.
    std::string FolderName(std::string tag)
    {
        std::replace_if(tag.begin(), tag.end(), [](char c) { return std::string("<>:\"/\\|?*").find(c) != std::string::npos || c < 0x20; }, '_');
        return tag.empty() ? "untagged" : tag;
    }
}

bool PackageSource::AddFromZip(const std::string& zipBytes, const std::string& origin, PackageFiles& files, RunLog& log, std::string& error)
{
    std::vector<ZipEntry> entries;
    if (!ZipReader::Read(zipBytes, [](const std::string& name) { return PackageFiles::IsWanted(name) || name == kReleasesZip; }, entries, error))
    {
        error = origin + ": " + error;
        return false;
    }
    for (const ZipEntry& entry : entries)
    {
        const std::string name = entry.path.substr(entry.path.find_last_of('/') + 1);
        const std::string where = origin + " > " + entry.path;
        if (name == kReleasesZip && !AddFromZip(entry.data, where, files, log, error))
        {
            return false;
        }
        if (name != kReleasesZip && !files.Add(name, entry.data, where, error))
        {
            return false;
        }
    }
    log.Write(LogLevel::Info, std::format("  {}: {} file(s) taken", origin, entries.size()));
    return true;
}

std::string LocalSource::Describe() const
{
    return "From this PC: " + mPath.string();
}

bool LocalSource::Collect(PackageFiles& files, RunLog& log, const std::atomic<bool>&, std::string& error)
{
    std::error_code problem;
    if (!std::filesystem::is_directory(mPath, problem))
    {
        error = mPath.string() + " is not a folder: choose the folder that holds nbt-aad and the plugin files";
        return false;
    }
    return CollectFolder(files, log, error);
}

bool LocalSource::CollectFolder(PackageFiles& files, RunLog& log, std::string& error)
{
    std::error_code problem;
    for (auto entry = std::filesystem::recursive_directory_iterator(mPath, std::filesystem::directory_options::skip_permission_denied, problem); entry != std::filesystem::recursive_directory_iterator(); entry.increment(problem))
    {
        if (entry.depth() >= kMaxDepth)
        {
            entry.disable_recursion_pending();
        }
        const std::string name = entry->path().filename().string();
        if (!entry->is_regular_file(problem) || (!PackageFiles::IsWanted(name) && name != kReleasesZip))
        {
            continue;
        }
        std::string data;
        const std::string origin = std::filesystem::relative(entry->path(), mPath, problem).string();
        if (!ReadFile(entry->path(), data))
        {
            error = "cannot read " + entry->path().string();
            return false;
        }
        if (name == kReleasesZip ? !AddFromZip(data, origin, files, log, error) : !files.Add(name, data, origin, error))
        {
            return false;
        }
    }
    return true;
}

std::string GitHubSource::Describe() const
{
    return std::format("From GitHub: {} ({}, {}{})", mRelease.name, mRelease.tag, mRelease.Date(), mRelease.stable ? ", stable" : ", not marked stable");
}

bool GitHubSource::Collect(PackageFiles& files, RunLog& log, const std::atomic<bool>& cancel, std::string& error)
{
    log.Write(LogLevel::Info, "release notes: " + mRelease.page);
    std::string daemon;
    std::string plugin;
    if (!Fetch(mRelease.daemon, PackageFiles::kDaemonName, daemon, log, cancel, error) || !Fetch(mRelease.plugin, kReleasesZip, plugin, log, cancel, error))
    {
        return false;
    }
    Prune();
    return files.Add(PackageFiles::kDaemonName, daemon, mRelease.tag + "/" + PackageFiles::kDaemonName, error) && AddFromZip(plugin, mRelease.tag + "/" + kReleasesZip, files, log, error);
}

bool GitHubSource::Fetch(const ReleaseAsset& asset, const std::string& name, std::string& data, RunLog& log, const std::atomic<bool>& cancel, std::string& error)
{
    const std::filesystem::path folder = mDownloads / FolderName(mRelease.tag);
    const std::filesystem::path path = folder / name;
    if (ReadFile(path, data) && data.size() == asset.size && (asset.sha256.empty() || PackageFiles::Sha256(data) == asset.sha256))
    {
        log.Write(LogLevel::Info, std::format("  {}: reusing {} ({} bytes{})", name, path.string(), data.size(), asset.sha256.empty() ? "" : ", SHA-256 matches GitHub"));
        return true;
    }
    log.Write(LogLevel::Command, "GET " + asset.url);
    const auto start = std::chrono::steady_clock::now();
    const HttpResult answer = HttpClient::Get(asset.url, "application/octet-stream", cancel);
    const int64_t milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    if (!answer.ok)
    {
        error = std::format("download of {} failed: {}", name, answer.error);
        return false;
    }
    log.Write(LogLevel::Info, std::format("  {} bytes in {} ms", answer.body.size(), milliseconds));
    if (answer.body.size() != asset.size)
    {
        error = std::format("{}: got {} bytes, GitHub lists {}", name, answer.body.size(), asset.size);
        return false;
    }
    if (!asset.sha256.empty() && PackageFiles::Sha256(answer.body) != asset.sha256)
    {
        error = name + ": SHA-256 differs from the one GitHub lists, so the download is damaged";
        return false;
    }
    log.Write(LogLevel::Pass, asset.sha256.empty() ? name + ": size matches (this release lists no SHA-256)" : name + ": size and SHA-256 match GitHub");
    data = answer.body;
    std::error_code problem;
    std::filesystem::create_directories(folder, problem);
    if (!WriteFile(path, data))
    {
        log.Write(LogLevel::Warning, "cannot keep a copy in " + path.string() + ": it will be downloaded again next time");
    }
    return true;
}

void GitHubSource::Prune() const
{
    std::vector<std::filesystem::directory_entry> folders;
    std::error_code problem;
    for (const auto& entry : std::filesystem::directory_iterator(mDownloads, problem))
    {
        if (entry.is_directory(problem))
        {
            folders.push_back(entry);
        }
    }
    std::sort(folders.begin(), folders.end(), [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) { return a.last_write_time() > b.last_write_time(); });
    for (size_t i = kKeepReleases; i < folders.size(); ++i)
    {
        std::filesystem::remove_all(folders[i].path(), problem);
    }
}

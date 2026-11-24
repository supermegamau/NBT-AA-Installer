// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include "GitHubReleases.h"
#include "PackageFiles.h"
#include "RunLog.h"

#include <atomic>
#include <filesystem>
#include <string>

// Where the six files come from: the two install sections differ only here.
class PackageSource
{
public:
    virtual ~PackageSource() = default;

    virtual std::string Describe() const = 0;
    virtual bool Collect(PackageFiles& files, RunLog& log, const std::atomic<bool>& cancel, std::string& error) = 0;

protected:
    static bool AddFromZip(const std::string& zipBytes, const std::string& origin, PackageFiles& files, RunLog& log, std::string& error);
};

// From this PC: a folder, with the files at any depth and possibly inside a Releases.zip.
class LocalSource : public PackageSource
{
public:
    explicit LocalSource(std::filesystem::path path) : mPath(std::move(path))
    {
    }

    std::string Describe() const override;
    bool Collect(PackageFiles& files, RunLog& log, const std::atomic<bool>& cancel, std::string& error) override;

private:
    bool CollectFolder(PackageFiles& files, RunLog& log, std::string& error);

    static constexpr uint8_t kMaxDepth = 3;  // deep enough for build/out/Releases, shallow enough not to crawl a whole drive

    std::filesystem::path mPath;
};

// From GitHub: one release's nbt-aad and Releases.zip, downloaded once into downloads\<tag>\.
class GitHubSource : public PackageSource
{
public:
    GitHubSource(ReleaseInfo release, std::filesystem::path downloads) : mRelease(std::move(release)), mDownloads(std::move(downloads))
    {
    }

    std::string Describe() const override;
    bool Collect(PackageFiles& files, RunLog& log, const std::atomic<bool>& cancel, std::string& error) override;

private:
    bool Fetch(const ReleaseAsset& asset, const std::string& name, std::string& data, RunLog& log, const std::atomic<bool>& cancel, std::string& error);
    void Prune() const;

    static constexpr uint8_t kKeepReleases = 5;

    ReleaseInfo           mRelease;
    std::filesystem::path mDownloads;
};

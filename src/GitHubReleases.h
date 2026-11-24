// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include "RunLog.h"
#include "SettingsCatalog.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

struct ReleaseAsset
{
    std::string url;
    uint64_t    size = 0;
    std::string sha256;  // from GitHub's "digest"; older releases have none
};

struct ReleaseInfo
{
    std::string  tag;
    std::string  name;
    std::string  published;   // ISO 8601, sorts as text
    std::string  page;        // the release notes
    bool         prerelease = false;
    bool         stable = false;
    ReleaseAsset daemon;
    ReleaseAsset plugin;

    std::string Date() const { return published.substr(0, 10); }
};

// The last releases on NBT-AAD's GitHub page that carry both files the installer needs.
class GitHubReleases
{
public:
    bool Fetch(const ReleaseRules& rules, RunLog& log, const std::atomic<bool>& cancel, std::string& error);
    const std::vector<ReleaseInfo>& Releases() const { return mReleases; }  // the newest kShown
    const std::vector<ReleaseInfo>& All() const { return mAll; }            // every one fetched, to recognise an older build on the car
    static const ReleaseInfo* Match(const std::vector<ReleaseInfo>& releases, uint64_t size, const std::string& sha256);

    static constexpr uint8_t kShown = 20;

private:
    std::vector<ReleaseInfo> mReleases;
    std::vector<ReleaseInfo> mAll;
};

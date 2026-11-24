// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include "GitHubReleases.h"

#include "HttpClient.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <format>
#include <regex>

namespace
{
    ReleaseAsset FindAsset(const nlohmann::json& release, const std::string& name)
    {
        for (const nlohmann::json& asset : release.value("assets", nlohmann::json::array()))
        {
            if (asset.value("name", std::string()) == name)
            {
                const std::string digest = asset.contains("digest") && asset.at("digest").is_string() ? asset.at("digest").get<std::string>() : std::string();
                return ReleaseAsset{ asset.value("browser_download_url", std::string()), asset.value("size", uint64_t(0)), digest.starts_with("sha256:") ? digest.substr(7) : std::string() };
            }
        }
        return ReleaseAsset{};
    }

    bool IsStable(const ReleaseInfo& release, const ReleaseRules& rules)
    {
        // GitHub's pre-release flag alone misses most test builds, so the tag and name are checked too.
        return !release.prerelease && std::regex_search(release.tag, std::regex(rules.stableTag)) && std::none_of(rules.testingMarkers.begin(), rules.testingMarkers.end(), [&release](const std::string& marker) { return release.name.find(marker) != std::string::npos; });
    }
}

const ReleaseInfo* GitHubReleases::Match(const std::vector<ReleaseInfo>& releases, uint64_t size, const std::string& sha256)
{
    // The SHA-256 is certain; a release without one is matched by its exact size.
    const auto found = std::find_if(releases.begin(), releases.end(), [&](const ReleaseInfo& release) { return size > 0 && (release.daemon.sha256.empty() ? release.daemon.size == size : release.daemon.sha256 == sha256); });
    return found != releases.end() ? &*found : nullptr;
}

bool GitHubReleases::Fetch(const ReleaseRules& rules, RunLog& log, const std::atomic<bool>& cancel, std::string& error)
{
    const std::string url = std::format("https://api.github.com/repos/{}/releases?per_page=50", rules.repo);
    log.Write(LogLevel::Command, "GET " + url);
    const HttpResult answer = HttpClient::Get(url, "application/vnd.github+json", cancel);
    log.Write(LogLevel::Info, std::format("  {} ({} bytes, {} requests left this hour)", answer.ok ? std::string("OK") : answer.error, answer.body.size(), answer.rateLimitLeft.empty() ? std::string("?") : answer.rateLimitLeft));
    if (!answer.ok)
    {
        error = answer.status == 403 || answer.status == 429 ? "GitHub's limit of 60 requests an hour without a login is used up: try again later" : "cannot read the release list: " + answer.error;
        return false;
    }
    try
    {
        std::vector<ReleaseInfo> releases;
        for (const nlohmann::json& item : nlohmann::json::parse(answer.body))
        {
            ReleaseInfo release{ item.value("tag_name", std::string()), item.value("name", std::string()), item.value("published_at", std::string()), item.value("html_url", std::string()), item.value("prerelease", false), false, FindAsset(item, rules.daemonAsset), FindAsset(item, rules.pluginAsset) };
            release.name = release.name.empty() ? release.tag : release.name;
            release.stable = IsStable(release, rules);
            if (item.value("draft", false) || release.daemon.url.empty() || release.plugin.url.empty())
            {
                log.Write(LogLevel::Info, std::format("  skipped {}: {}", release.tag, item.value("draft", false) ? "draft" : "no " + (release.daemon.url.empty() ? rules.daemonAsset : rules.pluginAsset)));
                continue;
            }
            releases.push_back(release);
        }
        // The API sorts by creation date: an old release published late would sit in the middle.
        std::sort(releases.begin(), releases.end(), [](const ReleaseInfo& a, const ReleaseInfo& b) { return a.published > b.published; });
        mAll = releases;
        releases.resize(std::min<size_t>(releases.size(), kShown));
        mReleases = releases;
        return true;
    }
    catch (const std::exception& problem)
    {
        error = std::string("unexpected answer from GitHub: ") + problem.what();
        return false;
    }
}

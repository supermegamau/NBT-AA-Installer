// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include "EnvFile.h"

#include <algorithm>
#include <format>

namespace
{
    std::string TrimRight(std::string text)
    {
        text.erase(text.find_last_not_of(" \t\r") + 1);
        return text;
    }

    std::string Trim(const std::string& text)
    {
        const size_t first = text.find_first_not_of(" \t");
        return first == std::string::npos ? std::string() : TrimRight(text.substr(first));
    }

    std::string Masked(const EnvLine& line, const SettingsCatalog& catalog)
    {
        const SettingDef* setting = catalog.Find(Trim(line.Key()));
        return setting && setting->secret && (line.IsSetting() || line.IsOff()) ? line.Text().substr(0, line.Text().find('=') + 1) + "***" : line.Text();
    }
}

EnvLine::EnvLine(const std::string& text) : mText(TrimRight(text))
{
    const std::string body = mText.starts_with(kOffMarker) ? mText.substr(std::char_traits<char>::length(kOffMarker)) : mText;
    const size_t equals = body.find('=');
    mOff = mText.starts_with(kOffMarker) && equals != std::string::npos && !body.empty() && body[0] != '#';
    mSetting = !mText.empty() && mText[0] != '#' && equals != std::string::npos;  // the car skips only a '#' in column 0
    if (mOff || mSetting)
    {
        mKey = body.substr(0, equals);
        mValue = body.substr(equals + 1);
    }
}

void EnvFile::SetText(const std::string& text)
{
    mLines.clear();
    size_t start = 0;
    while (start < text.size())
    {
        const size_t end = text.find('\n', start);
        mLines.emplace_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
        start = end == std::string::npos ? text.size() : end + 1;
    }
}

std::string EnvFile::Text() const
{
    std::string text;
    for (const EnvLine& line : mLines)
    {
        text += line.Text() + "\n";  // LF only: the car trims a CR, but the tool never adds one
    }
    return text;
}

std::optional<std::string> EnvFile::Value(const std::string& key) const
{
    const int32_t found = FindActive(key);
    return found >= 0 ? std::optional<std::string>(mLines[found].Value()) : std::nullopt;
}

bool EnvFile::HasOff(const std::string& key) const
{
    return FindOff(key) >= 0;
}

bool EnvFile::CanAdd(const std::string& key) const
{
    const int32_t off = FindOff(key);
    return FindActive(key) >= 0 || mCatalog.IsAppOnly(key) || SlotAt(off >= 0 ? static_cast<size_t>(off) : InsertPosition(key)) <= mCatalog.MaxSettings();
}

void EnvFile::Set(const std::string& key, const std::string& value)
{
    const int32_t active = FindActive(key);
    const int32_t off = FindOff(key);
    const EnvLine line(key + "=" + value);
    if (active >= 0)
    {
        mLines[active] = line;
        for (size_t i = mLines.size(); i-- > static_cast<size_t>(active) + 1;)
        {
            if (mLines[i].IsSetting() && Trim(mLines[i].Key()) == key)
            {
                mLines.erase(mLines.begin() + static_cast<ptrdiff_t>(i));  // using the control settles a duplicate
            }
        }
        return;
    }
    if (off >= 0)
    {
        mLines[off] = line;  // back where it was turned off, so the order the user chose survives
        return;
    }
    mLines.insert(mLines.begin() + static_cast<ptrdiff_t>(InsertPosition(key)), line);
}

void EnvFile::Remove(const std::string& key)
{
    std::erase_if(mLines, [&key](const EnvLine& line) { return line.IsSetting() && Trim(line.Key()) == key; });
}

bool EnvFile::TurnOff(const std::string& key)
{
    bool fits = true;
    for (EnvLine& line : mLines)
    {
        if (line.IsSetting() && Trim(line.Key()) == key && line.Text().size() + std::char_traits<char>::length(EnvLine::kOffMarker) <= mCatalog.MaxLineLength())
        {
            line = EnvLine(EnvLine::kOffMarker + line.Text());
        }
        else if (line.IsSetting() && Trim(line.Key()) == key)
        {
            fits = false;  // a comment over 159 characters is split by the car like any other line
        }
    }
    return fits;
}

bool EnvFile::TurnOn(const std::string& key)
{
    const int32_t off = FindOff(key);
    if (off < 0 || FindActive(key) >= 0 || (!mCatalog.IsAppOnly(key) && SlotAt(static_cast<size_t>(off)) > mCatalog.MaxSettings()))
    {
        return false;
    }
    mLines[off] = EnvLine(mLines[off].Text().substr(std::char_traits<char>::length(EnvLine::kOffMarker)));
    return true;
}

uint8_t EnvFile::SlotsUsed() const
{
    uint16_t slot = 0;
    uint16_t lastDaemon = 0;
    for (const EnvLine& line : mLines)
    {
        slot += line.IsSetting() ? 1 : 0;
        lastDaemon = IsDaemonSetting(line) ? slot : lastDaemon;
    }
    return static_cast<uint8_t>(std::min<uint16_t>(lastDaemon, 255));
}

std::vector<std::string> EnvFile::IgnoredKeys() const
{
    std::vector<std::string> ignored;
    uint16_t slot = 0;
    for (const EnvLine& line : mLines)
    {
        slot += line.IsSetting() ? 1 : 0;
        if (IsDaemonSetting(line) && slot > mCatalog.MaxSettings())
        {
            ignored.push_back(Trim(line.Key()));
        }
    }
    return ignored;
}

std::vector<uint16_t> EnvFile::LongLines() const
{
    std::vector<uint16_t> numbers;
    for (size_t i = 0; i < mLines.size(); ++i)
    {
        if (mLines[i].Text().size() > mCatalog.MaxLineLength())
        {
            numbers.push_back(static_cast<uint16_t>(i + 1));
        }
    }
    return numbers;
}

std::vector<EnvFinding> EnvFile::Findings() const
{
    std::vector<EnvFinding> findings;
    std::vector<std::string> seen;
    uint16_t slot = 0;
    for (uint16_t i = 0; i < mLines.size(); ++i)
    {
        const EnvLine& line = mLines[i];
        const std::string key = Trim(line.Key());
        const SettingDef* setting = mCatalog.Find(key);
        slot += line.IsSetting() ? 1 : 0;
        if (line.Text().size() > mCatalog.MaxLineLength())
        {
            findings.push_back(EnvFinding{ i, LogLevel::Error, std::format("{} characters: the car splits lines over {}, so nothing is sent", line.Text().size(), mCatalog.MaxLineLength()) });
        }
        if (line.IsOff())
        {
            findings.push_back(EnvFinding{ i, LogLevel::Info, "turned off by NBT-AA-Installer: the car skips it" });
            continue;
        }
        if (!line.IsSetting())
        {
            continue;
        }
        if (key.starts_with('#'))
        {
            findings.push_back(EnvFinding{ i, LogLevel::Warning, "spaces before '#': the car counts this comment as a setting" });
            continue;
        }
        if (line.Key() != key)
        {
            findings.push_back(EnvFinding{ i, LogLevel::Warning, "spaces around the name: the daemon won't recognise it" });
        }
        if (line.Value().find(" #") != std::string::npos || line.Value().find("\t#") != std::string::npos)
        {
            findings.push_back(EnvFinding{ i, LogLevel::Warning, "comment after the value: it becomes part of the value" });
        }
        if (std::find(seen.begin(), seen.end(), key) != seen.end())
        {
            findings.push_back(EnvFinding{ i, LogLevel::Warning, "duplicate: the car's readers disagree on which line wins" });
        }
        if (IsDaemonSetting(line) && slot > mCatalog.MaxSettings())
        {
            findings.push_back(EnvFinding{ i, LogLevel::Warning, std::format("setting {}: past the {}th, ignored by the car", slot, mCatalog.MaxSettings()) });
        }
        if (!setting)
        {
            findings.push_back(EnvFinding{ i, LogLevel::Info, "not in the settings list: kept as typed" });
        }
        else if (!setting->Problem(line.Value()).empty())
        {
            findings.push_back(EnvFinding{ i, LogLevel::Warning, setting->Problem(line.Value()) });
        }
        seen.push_back(key);
    }
    return findings;
}

std::vector<std::string> EnvFile::Diff(const std::string& before, const std::string& after, const SettingsCatalog& catalog)
{
    EnvFile old(catalog);
    EnvFile now(catalog);
    old.SetText(before);
    now.SetText(after);
    const std::vector<EnvLine>& a = old.Lines();
    const std::vector<EnvLine>& b = now.Lines();
    // Longest common subsequence: the files are a few dozen lines, so the table stays tiny.
    std::vector<std::vector<uint16_t>> common(a.size() + 1, std::vector<uint16_t>(b.size() + 1, 0));
    for (size_t i = a.size(); i-- > 0;)
    {
        for (size_t j = b.size(); j-- > 0;)
        {
            common[i][j] = a[i].Text() == b[j].Text() ? static_cast<uint16_t>(common[i + 1][j + 1] + 1) : std::max(common[i + 1][j], common[i][j + 1]);
        }
    }
    std::vector<std::string> changes;
    size_t i = 0;
    size_t j = 0;
    while (i < a.size() || j < b.size())
    {
        if (i < a.size() && j < b.size() && a[i].Text() == b[j].Text())
        {
            ++i;
            ++j;
        }
        else if (j < b.size() && (i == a.size() || common[i][j + 1] >= common[i + 1][j]))
        {
            changes.push_back("+ " + Masked(b[j++], catalog));
        }
        else
        {
            changes.push_back("- " + Masked(a[i++], catalog));
        }
    }
    return changes;
}

int32_t EnvFile::FindActive(const std::string& key) const
{
    const auto found = std::find_if(mLines.begin(), mLines.end(), [&key](const EnvLine& line) { return line.IsSetting() && Trim(line.Key()) == key; });
    return found != mLines.end() ? static_cast<int32_t>(found - mLines.begin()) : -1;
}

int32_t EnvFile::FindOff(const std::string& key) const
{
    const auto found = std::find_if(mLines.begin(), mLines.end(), [&key](const EnvLine& line) { return line.IsOff() && Trim(line.Key()) == key; });
    return found != mLines.end() ? static_cast<int32_t>(found - mLines.begin()) : -1;
}

uint8_t EnvFile::SlotAt(size_t position) const
{
    const ptrdiff_t before = std::count_if(mLines.begin(), mLines.begin() + static_cast<ptrdiff_t>(std::min(position, mLines.size())), [](const EnvLine& line) { return line.IsSetting(); });
    return static_cast<uint8_t>(std::min<ptrdiff_t>(before + 1, 255));
}

size_t EnvFile::InsertPosition(const std::string& key) const
{
    if (mCatalog.IsAppOnly(key))
    {
        return mLines.size();  // the iDrive app reads the whole file, so the end keeps the 15 slots for the daemon
    }
    const auto lastDaemon = std::find_if(mLines.rbegin(), mLines.rend(), [this](const EnvLine& line) { return IsDaemonSetting(line); });
    if (lastDaemon != mLines.rend())
    {
        return static_cast<size_t>(mLines.rend() - lastDaemon);
    }
    const auto firstSetting = std::find_if(mLines.begin(), mLines.end(), [](const EnvLine& line) { return line.IsSetting(); });
    return static_cast<size_t>(firstSetting - mLines.begin());
}

bool EnvFile::IsDaemonSetting(const EnvLine& line) const
{
    return line.IsSetting() && !mCatalog.IsAppOnly(Trim(line.Key()));  // unknown keys count: the car can't tell them apart
}

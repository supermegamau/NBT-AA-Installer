// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include "LogLevel.h"
#include "SettingsCatalog.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// One line of nbt-aad.env, read the way the car reads it.
class EnvLine
{
public:
    explicit EnvLine(const std::string& text);

    const std::string& Text() const { return mText; }
    bool IsSetting() const { return mSetting; }
    bool IsOff() const { return mOff; }
    const std::string& Key() const { return mKey; }     // as written, spaces included: that's what the daemon sees
    const std::string& Value() const { return mValue; }

    static constexpr const char* kOffMarker = "#~off ";  // ours alone: the user's own comments are never touched

private:
    std::string mText;            // kept verbatim so unknown or invalid lines are never rewritten
    std::string mKey;
    std::string mValue;
    bool        mSetting = false;  // the car counts it toward the 15
    bool        mOff = false;      // a setting this tool turned off, to bring back later
};

struct EnvFinding
{
    uint16_t    line = 0;  // 0-based
    LogLevel    level = LogLevel::Info;
    std::string text;
};

// nbt-aad.env as text: the File tab shows it, the setting controls edit single lines of it.
class EnvFile
{
public:
    explicit EnvFile(const SettingsCatalog& catalog) : mCatalog(catalog)
    {
    }

    void SetText(const std::string& text);
    std::string Text() const;
    const std::vector<EnvLine>& Lines() const { return mLines; }

    std::optional<std::string> Value(const std::string& key) const;  // the first active line, as the daemon reads it
    bool HasOff(const std::string& key) const;
    bool CanAdd(const std::string& key) const;                         // false when a new line would be past the 15th
    void Set(const std::string& key, const std::string& value);
    void Remove(const std::string& key);
    bool TurnOff(const std::string& key);                              // false when a marked line would pass 159 characters
    bool TurnOn(const std::string& key);                               // false when it would break the 15 or 159 rule

    uint8_t SlotsUsed() const;                                         // the last daemon setting's place among the setting lines
    std::vector<std::string> IgnoredKeys() const;                      // daemon settings past the 15th
    std::vector<uint16_t> LongLines() const;                           // 1-based numbers, for the message
    std::vector<EnvFinding> Findings() const;

    static std::vector<std::string> Diff(const std::string& before, const std::string& after, const SettingsCatalog& catalog);

private:
    int32_t FindActive(const std::string& key) const;
    int32_t FindOff(const std::string& key) const;
    uint8_t SlotAt(size_t position) const;
    size_t InsertPosition(const std::string& key) const;
    bool IsDaemonSetting(const EnvLine& line) const;

    const SettingsCatalog& mCatalog;
    std::vector<EnvLine>   mLines;
};

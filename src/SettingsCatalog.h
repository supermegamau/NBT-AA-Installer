// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

enum class SettingType : uint8_t
{
    Bool,
    Select,
    Int,
    Decimal,
    Hex,
    Text
};

struct SettingChoice
{
    std::string value;
    std::string label;
};

// One NBT_* setting as data/settings.json describes it.
struct SettingDef
{
    std::string                key;
    std::string                tab;
    std::string                label;
    std::string                description;
    std::string                unset;          // what the car does with no line: the "Default" choice
    std::string                unit;
    std::string                onLabel = "On";
    std::string                offLabel = "Off";
    std::string                idriveOption;   // the iDrive checkbox this line overrides
    std::string                flagFile;       // that checkbox's file in the data dir
    SettingType                type = SettingType::Text;
    std::vector<SettingChoice> values;
    std::optional<double>      min;
    std::optional<double>      max;
    uint16_t                   maxLength = 0;
    bool                       appOnly = false;   // read by the iDrive app from the whole file: not one of the 15
    bool                       advanced = false;
    bool                       secret = false;    // masked in logs and summaries

    std::string Problem(const std::string& value) const;  // empty when the car accepts the value as written

    static bool IsOnWord(const std::string& value);
    static bool IsOffWord(const std::string& value);
};

struct TabDef
{
    std::string id;
    std::string label;
    bool        locked = false;      // the iDrive overrides: opened only after a warning
    std::string unlockWarning;
};

// The From GitHub source, kept in the JSON so a new repo or naming scheme needs no rebuild.
struct ReleaseRules
{
    std::string              repo = "Sq00ky/Supra-iDrive6-Android-Auto";
    std::string              daemonAsset = "nbt-aad";
    std::string              pluginAsset = "Releases.zip";
    std::string              stableTag = "^v[0-9]+\\.[0-9]+\\.[0-9]+";
    std::vector<std::string> testingMarkers{ "[TESTING]" };
};

// data/settings.json: the hand-maintained list of settings the tabs are built from.
class SettingsCatalog
{
public:
    bool Load(const std::filesystem::path& path, std::string& error);
    bool LoadText(const std::string& json, std::string& error);

    const std::vector<TabDef>& Tabs() const { return mTabs; }
    const std::vector<SettingDef>& Settings() const { return mSettings; }
    const ReleaseRules& Releases() const { return mReleases; }
    const SettingDef* Find(const std::string& key) const;
    bool IsAppOnly(const std::string& key) const;
    uint8_t MaxSettings() const { return mMaxSettings; }
    uint16_t MaxLineLength() const { return mMaxLineLength; }

private:
    std::vector<TabDef>     mTabs;
    std::vector<SettingDef> mSettings;
    ReleaseRules            mReleases;
    uint8_t                 mMaxSettings = 15;     // the iDrive app's envbuf: 16 slots, PATH takes one
    uint16_t                mMaxLineLength = 159;  // fgets into 160 bytes
};

// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include "SettingsCatalog.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <fstream>
#include <sstream>

namespace
{
    std::string Lower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    std::optional<double> ParseDecimal(const std::string& text)
    {
        double value = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        return error == std::errc() && end == text.data() + text.size() && !text.empty() ? std::optional<double>(value) : std::nullopt;
    }

    std::optional<double> ParseInt(const std::string& text)
    {
        int64_t value = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        return error == std::errc() && end == text.data() + text.size() && !text.empty() ? std::optional<double>(static_cast<double>(value)) : std::nullopt;
    }

    std::optional<double> ParseHex(const std::string& text)
    {
        const std::string digits = Lower(text).starts_with("0x") ? text.substr(2) : text;
        uint64_t value = 0;
        const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value, 16);
        return error == std::errc() && end == digits.data() + digits.size() && !digits.empty() ? std::optional<double>(static_cast<double>(value)) : std::nullopt;
    }

    // Ranges may be written as numbers or, for hex settings, as "0x..." strings.
    std::optional<double> Limit(const nlohmann::json& item, const char* name)
    {
        if (!item.contains(name))
        {
            return std::nullopt;
        }
        const nlohmann::json& limit = item.at(name);
        return limit.is_number() ? std::optional<double>(limit.get<double>()) : limit.is_string() ? ParseHex(limit.get<std::string>()) : std::nullopt;
    }

    struct TypeName
    {
        const char* name;
        SettingType type;
    };

    constexpr TypeName kTypeNames[] =
    {
        { "bool", SettingType::Bool },
        { "select", SettingType::Select },
        { "int", SettingType::Int },
        { "decimal", SettingType::Decimal },
        { "hex", SettingType::Hex },
        { "text", SettingType::Text }
    };

    SettingType TypeOf(const std::string& name)
    {
        const auto found = std::find_if(std::begin(kTypeNames), std::end(kTypeNames), [&name](const TypeName& entry) { return name == entry.name; });
        return found != std::end(kTypeNames) ? found->type : SettingType::Text;  // an unknown type still shows as a text field
    }

    std::string Number(double value)
    {
        return value == std::floor(value) ? std::format("{}", static_cast<int64_t>(value)) : std::format("{}", value);
    }
}

bool SettingDef::IsOnWord(const std::string& value)
{
    const std::string word = Lower(value);
    return word == "1" || word == "on" || word == "yes" || word == "true";
}

bool SettingDef::IsOffWord(const std::string& value)
{
    const std::string word = Lower(value);
    return word == "0" || word == "off" || word == "no" || word == "false";
}

std::string SettingDef::Problem(const std::string& value) const
{
    if (type == SettingType::Bool)
    {
        return IsOnWord(value) || IsOffWord(value) ? std::string() : "not an on/off value (1/on/yes/true or 0/off/no/false): the car uses its default";
    }
    if (type == SettingType::Select)
    {
        return std::any_of(values.begin(), values.end(), [&value](const SettingChoice& choice) { return choice.value == value; }) ? std::string() : "not one of the choices: the car uses its default";
    }
    if (type == SettingType::Text)
    {
        return maxLength && value.size() > maxLength ? std::format("longer than {} characters", maxLength) : std::string();
    }
    const std::optional<double> number = type == SettingType::Int ? ParseInt(value) : type == SettingType::Hex ? ParseHex(value) : ParseDecimal(value);
    if (!number)
    {
        return type == SettingType::Hex ? "not a hex number: the car uses its default" : "not a number: the car uses its default";
    }
    if ((min && *number < *min) || (max && *number > *max))
    {
        return std::format("outside {}..{}: the car limits it", min ? Number(*min) : std::string("-"), max ? Number(*max) : std::string("-"));
    }
    return std::string();
}

bool SettingsCatalog::Load(const std::filesystem::path& path, std::string& error)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        error = "cannot open " + path.string();
        return false;
    }
    std::stringstream text;
    text << file.rdbuf();
    return LoadText(text.str(), error);
}

bool SettingsCatalog::LoadText(const std::string& json, std::string& error)
{
    try
    {
        const nlohmann::json root = nlohmann::json::parse(json);
        mMaxSettings = static_cast<uint8_t>(root.value("maxSettings", 15));
        mMaxLineLength = static_cast<uint16_t>(root.value("maxLineLength", 159));
        if (root.contains("releases"))
        {
            const nlohmann::json& releases = root.at("releases");
            mReleases.repo = releases.value("repo", mReleases.repo);
            mReleases.daemonAsset = releases.value("daemonAsset", mReleases.daemonAsset);
            mReleases.pluginAsset = releases.value("pluginAsset", mReleases.pluginAsset);
            mReleases.stableTag = releases.value("stableTag", mReleases.stableTag);
            mReleases.testingMarkers = releases.value("testingMarkers", mReleases.testingMarkers);
        }
        mTabs.clear();
        for (const nlohmann::json& item : root.at("tabs"))
        {
            mTabs.push_back(TabDef{ item.at("id").get<std::string>(), item.value("label", item.at("id").get<std::string>()), item.value("locked", false), item.value("unlockWarning", std::string()) });
        }
        mSettings.clear();
        for (const nlohmann::json& item : root.at("settings"))
        {
            SettingDef setting;
            setting.key = item.at("key").get<std::string>();
            setting.tab = item.value("tab", std::string("advanced"));
            setting.label = item.value("label", setting.key);
            setting.description = item.value("description", std::string());
            setting.unset = item.value("unset", std::string("Default"));
            setting.unit = item.value("unit", std::string());
            setting.onLabel = item.value("onLabel", setting.onLabel);
            setting.offLabel = item.value("offLabel", setting.offLabel);
            setting.idriveOption = item.value("idriveOption", std::string());
            setting.flagFile = item.value("flagFile", std::string());
            setting.type = TypeOf(item.value("type", std::string("text")));
            setting.min = Limit(item, "min");
            setting.max = Limit(item, "max");
            setting.maxLength = static_cast<uint16_t>(item.value("maxLength", 0));
            setting.appOnly = item.value("appOnly", false);
            setting.advanced = item.value("advanced", false);
            setting.secret = item.value("secret", false);
            for (const nlohmann::json& choice : item.value("values", nlohmann::json::array()))
            {
                setting.values.push_back(SettingChoice{ choice.at("value").get<std::string>(), choice.value("label", choice.at("value").get<std::string>()) });
            }
            mSettings.push_back(setting);
        }
        return true;
    }
    catch (const nlohmann::json::exception& problem)
    {
        error = std::string("settings.json: ") + problem.what();
        return false;
    }
}

const SettingDef* SettingsCatalog::Find(const std::string& key) const
{
    const auto found = std::find_if(mSettings.begin(), mSettings.end(), [&key](const SettingDef& setting) { return setting.key == key; });
    return found != mSettings.end() ? &*found : nullptr;
}

bool SettingsCatalog::IsAppOnly(const std::string& key) const
{
    const SettingDef* setting = Find(key);
    return setting && setting->appOnly;
}

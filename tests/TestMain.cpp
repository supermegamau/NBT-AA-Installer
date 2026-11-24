// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
// Checks the nbt-aad.env rules and the checksums, which must be right before anything reaches a car.
#include "DirListing.h"
#include "EnvFile.h"
#include "PackageFiles.h"
#include "SettingsCatalog.h"

#include <cstdio>
#include <format>
#include <source_location>
#include <string>

namespace
{
    uint16_t gFailures = 0;
    uint16_t gChecks = 0;

    void Check(bool ok, const std::string& what, std::source_location where = std::source_location::current())
    {
        ++gChecks;
        if (!ok)
        {
            ++gFailures;
            std::printf("FAIL line %u: %s\n", static_cast<uint32_t>(where.line()), what.c_str());
        }
    }

    const char* kCatalogJson = R"({
      "maxSettings": 3,
      "maxLineLength": 40,
      "tabs": [ {"id": "common", "label": "Common"}, {"id": "idrive", "label": "iDrive", "locked": true} ],
      "settings": [
        {"key": "NBT_A", "tab": "common", "type": "bool"},
        {"key": "NBT_B", "tab": "common", "type": "int", "min": 1, "max": 60},
        {"key": "NBT_C", "tab": "common", "type": "select", "values": [{"value": "x"}, {"value": "y"}]},
        {"key": "NBT_D", "tab": "common", "type": "hex", "min": "0x0", "max": "0xFF"},
        {"key": "NBT_APP", "tab": "common", "type": "text", "appOnly": true},
        {"key": "NBT_PASS", "tab": "common", "type": "text", "secret": true},
        {"key": "NBT_GPS", "tab": "idrive", "type": "bool", "flagFile": "phone-gps"}
      ]
    })";

    void TestLines(const SettingsCatalog& catalog)
    {
        EnvFile env(catalog);
        env.SetText("# comment\r\nNBT_A=1\n  # x=y\n NBT_B=5\nNBT_C=z # note\nNBT_A=0\n");
        Check(env.Lines().size() == 6, "six lines, CRLF taken");
        Check(env.Value("NBT_A") == "1", "the first line wins for the daemon");
        Check(env.Value("NBT_B") == "5", "a leading space still finds the key");
        Check(env.Text().find('\r') == std::string::npos, "written back with LF only");
        const std::vector<EnvFinding> findings = env.Findings();
        const auto has = [&findings](uint16_t line, const std::string& part) { return std::any_of(findings.begin(), findings.end(), [&](const EnvFinding& f) { return f.line == line && f.text.find(part) != std::string::npos; }); };
        Check(has(2, "spaces before '#'"), "indented comment counted as a setting");
        Check(has(3, "spaces around the name"), "leading space on a name");
        Check(has(4, "comment after the value"), "inline comment");
        Check(has(4, "not one of the choices"), "invalid select value");
        Check(has(5, "duplicate"), "duplicate key");
        Check(has(5, "past the 3th"), "fourth setting ignored with a limit of 3");
    }

    void TestLimit(const SettingsCatalog& catalog)
    {
        EnvFile env(catalog);
        env.SetText("NBT_A=1\nNBT_B=2\nNBT_APP=x\n");
        Check(env.SlotsUsed() == 2, "app-only line at the end takes no daemon slot");
        Check(env.CanAdd("NBT_C"), "third daemon setting fits");
        env.Set("NBT_C", "x");
        Check(env.Text() == "NBT_A=1\nNBT_B=2\nNBT_C=x\nNBT_APP=x\n", "daemon setting goes after the last daemon line, before app-only ones");
        Check(!env.CanAdd("NBT_D"), "a fourth is refused");
        Check(env.CanAdd("NBT_B"), "changing an existing one is fine at the limit");
        Check(env.CanAdd("NBT_APP"), "app-only is never refused");
        env.Set("NBT_B", "7");
        Check(env.Value("NBT_B") == "7" && env.SlotsUsed() == 3, "rewritten in place");
        env.Remove("NBT_A");
        Check(!env.Value("NBT_A") && env.CanAdd("NBT_D"), "Default removes the line and frees a slot");
        env.SetText("NBT_A=1\nNBT_A=0\n");
        env.Set("NBT_A", "0");
        Check(env.Text() == "NBT_A=0\n", "using the control settles a duplicate");
        env.SetText("");
        env.Set("NBT_APP", "a");
        env.Set("NBT_A", "1");
        Check(env.Text() == "NBT_A=1\nNBT_APP=a\n", "first daemon setting goes before app-only lines");
    }

    void TestOff(const SettingsCatalog& catalog)
    {
        EnvFile env(catalog);
        env.SetText("NBT_A=1\nNBT_GPS=1\n");
        Check(env.TurnOff("NBT_GPS"), "turned off");
        Check(env.Text() == "NBT_A=1\n#~off NBT_GPS=1\n" && !env.Value("NBT_GPS") && env.HasOff("NBT_GPS"), "kept as #~off");
        Check(env.SlotsUsed() == 1, "an #~off line takes no slot");
        Check(env.TurnOn("NBT_GPS") && env.Value("NBT_GPS") == "1", "back where it was");
        env.SetText("NBT_GPS=0123456789012345678901234567890\n");
        Check(!env.TurnOff("NBT_GPS") && env.Value("NBT_GPS"), "refused when the marked line would pass the limit");
        env.SetText("#~off NBT_GPS=1\nNBT_GPS=0\n");
        Check(!env.TurnOn("NBT_GPS"), "no second active line");
        env.SetText("#~off NBT_GPS=1\n");
        env.Set("NBT_GPS", "0");
        Check(env.Text() == "NBT_GPS=0\n", "setting a turned-off key reuses its line");
    }

    void TestLongAndDiff(const SettingsCatalog& catalog)
    {
        EnvFile env(catalog);
        env.SetText("NBT_A=1\n# " + std::string(40, 'x') + "\n");
        Check(env.LongLines() == std::vector<uint16_t>{ 2 }, "long comment found, numbered from 1");
        const std::vector<std::string> diff = EnvFile::Diff("NBT_A=1\nNBT_PASS=old\n", "NBT_A=0\nNBT_PASS=new\n", catalog);
        Check(diff.size() == 4, "two lines changed");
        Check(std::find(diff.begin(), diff.end(), "- NBT_PASS=***") != diff.end() && std::find(diff.begin(), diff.end(), "+ NBT_PASS=***") != diff.end(), "secret values masked");
        Check(EnvFile::Diff("a\nb\n", "a\nb\n", catalog).empty(), "no diff for the same text");
    }

    void TestValues(const SettingsCatalog& catalog)
    {
        Check(catalog.Find("NBT_A")->Problem("yes").empty() && !catalog.Find("NBT_A")->Problem("maybe").empty(), "bool words");
        Check(catalog.Find("NBT_B")->Problem("60").empty() && !catalog.Find("NBT_B")->Problem("61").empty() && !catalog.Find("NBT_B")->Problem("1.5").empty(), "int range");
        Check(catalog.Find("NBT_D")->Problem("0x1f").empty() && catalog.Find("NBT_D")->Problem("FF").empty() && !catalog.Find("NBT_D")->Problem("0x100").empty(), "hex range");
        Check(catalog.IsAppOnly("NBT_APP") && !catalog.IsAppOnly("NBT_A") && !catalog.IsAppOnly("NBT_UNKNOWN"), "app-only lookup");
    }

    void TestListing()
    {
        // ls -l of a folder prints names; ls -l of one file prints the path as given (the bug that stopped the first send).
        const DirListing folder("total 22\ndrwxr-xr-x  2 root root 4096 Nov 18 21:13 .\n-rwxr-xr-x  1 root root 956398 Nov 12 18:31 nbt-aad\n");
        Check(folder.Find("nbt-aad") && folder.Find("nbt-aad")->size == 956398 && folder.Find("nbt-aad")->executable, "file in a folder listing");
        Check(!folder.Find("."), "dot entries skipped");
        const DirListing single("-rwxr-xr-x  1 root      root        1077501 Nov 20 22:44 /opt/aa/nbt-aad\n");
        Check(single.Find("/opt/aa/nbt-aad") && single.Find("/opt/aa/nbt-aad")->size == 1077501, "single file found by its full path");
    }

    void TestPackage()
    {
        Check(PackageFiles::Cksum("") == 4294967295u, "POSIX cksum of nothing");
        Check(PackageFiles::Cksum("123456789") == 930766865u, "POSIX cksum check value");
        Check(PackageFiles::Sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 of abc");
        Check(PackageFiles::VersionIn(std::string("\x7F" "ELF\0junkNBT-AAD-VERSION=v1.9.112-cluster\0more", 47)) == "v1.9.112-cluster", "version tag in a binary");
        Check(NbtVersion::Find("v1.9.111 - Gauge").Compare(NbtVersion::Find("v1.9.112")) < 0 && NbtVersion::Find("pr223").valid == false, "version compare");
        PackageFiles files;
        std::string error;
        Check(files.Add("nbt-aad", "a", "one", error) && files.Add("nbt-aad", "a", "two", error), "the same file twice is fine");
        Check(!files.Add("nbt-aad", "b", "three", error) && error.find("two different copies") != std::string::npos, "different copies refused");
        std::string missing;
        Check(!files.Complete(missing) && missing.find("libNbtAad.so missing") != std::string::npos, "missing plugin files named");
    }
}

int main()
{
    SettingsCatalog catalog;
    std::string error;
    Check(catalog.LoadText(kCatalogJson, error), "catalogue loads: " + error);
    TestLines(catalog);
    TestLimit(catalog);
    TestOff(catalog);
    TestLongAndDiff(catalog);
    TestValues(catalog);
    TestListing();
    TestPackage();
    SettingsCatalog real;
    Check(real.Load("data/settings.json", error) || real.Load("../data/settings.json", error), "data/settings.json loads: " + error);
    std::printf("%u checks, %u failed\n", static_cast<uint32_t>(gChecks), static_cast<uint32_t>(gFailures));
    return gFailures == 0 ? 0 : 1;
}

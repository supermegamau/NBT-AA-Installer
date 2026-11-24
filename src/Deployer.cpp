// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include "Deployer.h"

#include "DirListing.h"
#include "EnvFile.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <sstream>

namespace
{
    // Paths from NBT-AAD's nbt_paths.hpp: a hand install and an NBTApps package keep their files apart.
    constexpr const char* kHandAppDir = "/opt/aa";
    constexpr const char* kHandDataDir = "/var/opt/aa";
    constexpr const char* kPackageAppDir = "/var/opt/nbtapp/apps/com.github.sq00ky.android-auto";
    constexpr const char* kPackageDataDir = "/var/opt/nbtapp/data/com.github.sq00ky.android-auto";
    constexpr const char* kEnvName = "nbt-aad.env";

    // The car's library folders, as NBT-AAD's own launcher (mmr_launch.cpp) hands them to the car's programs.
    constexpr const char* kCarLibraryPath = "/proc/boot:/boot/lib:/lib:/usr/lib:/opt/os/lib:/opt/sys/lib:/opt/core/lib:/opt/car/lib";
    constexpr const char* kRestartOutput = "/dev/shmem/nbt-aa-installer-restart.out";  // RAM: gone at the next reboot
    constexpr const char* kCarLibraryFolders = "/proc/boot /boot/lib /lib /usr/lib /opt/os/lib /opt/sys/lib /opt/core/lib /opt/car/lib /opt/conn/lib";

    // NBT-AAD's RAM logs, wiped on reboot: worth having on the PC before anything else happens.
    constexpr const char* kCarLogs[] =
    {
        "/dev/shmem/nbt-aad.log",
        "/dev/shmem/nbt-aad-app.log",
        "/dev/shmem/nbt-aad-pf_aa.out"
    };

    std::string FirstLine(const std::string& text)
    {
        return text.substr(0, text.find_first_of("\r\n"));
    }

    std::string TextAfter(const std::string& line, const std::string& marker)
    {
        const size_t found = line.find(marker);
        return found == std::string::npos ? line : line.substr(found + marker.size());
    }

    // pidin prints one line per thread: the first column is the pid, and the program path names nbt-aad.
    std::vector<std::string> PidsFrom(const std::string& pidin)
    {
        std::vector<std::string> pids;
        std::istringstream lines(pidin);
        std::string line;
        while (std::getline(lines, line))
        {
            std::istringstream fields(line);
            std::string pid;
            if (line.find("nbt-aad") != std::string::npos && fields >> pid && std::all_of(pid.begin(), pid.end(), [](char c) { return c >= '0' && c <= '9'; }) && std::find(pids.begin(), pids.end(), pid) == pids.end())
            {
                pids.push_back(pid);
            }
        }
        return pids;
    }
}

Deployer::Deployer(CarConnection connection, RunLog& log, const SettingsCatalog& catalog, const std::atomic<bool>& cancel)
    : mConnection(std::move(connection))
    , mLog(log)
    , mCatalog(catalog)
    , mCancel(cancel)
{
}

bool Deployer::Connect(CarState& state)
{
    Begin(2);
    return Finish(Open() && Probe(state), "connect");
}

bool Deployer::Send(PackageSource& source, const EnvPlan& env, bool restart, CarState& state, const Ask& ask)
{
    Begin(10);
    mLog.Write(LogLevel::Info, source.Describe());
    Step("collect the files");
    PackageFiles files;
    std::string error;
    std::string missing;
    if (!source.Collect(files, mLog, mCancel, error) || !files.Complete(missing))
    {
        return Finish(Fail(error.empty() ? missing : error), "send");
    }
    for (const PackageFile& file : files.Files())
    {
        mLog.Write(LogLevel::Info, std::format("  {:<22} {:>9} bytes  SHA-256 {}  from {}", file.name, file.data.size(), PackageFiles::Sha256(file.data), file.origin));
    }
    Pass("all 6 files, version " + (files.Version().empty() ? std::string("unknown (no NBT-AAD-VERSION in nbt-aad)") : files.Version()));
    if (!Open() || !Probe(state) || Cancelled())
    {
        return Finish(false, "send");
    }
    const std::string envBefore = state.envText;
    const bool envExisted = state.envExists;
    if (!Remount() || !Stage(files, state) || !StopDaemon())
    {
        ClearStaged(state);
        mLog.Write(LogLevel::Error, "stopped before anything was replaced: the car keeps the version it had");
        PullLogsAfterFailure();
        return Finish(false, "send");
    }
    // From the swap on, any failure puts the previous version back: the car is never left half updated.
    if (!Swap(state) || !WriteEnv(env, state, ask) || !Firewall(state))
    {
        PullLogsAfterFailure();
        Restore(state, envBefore, envExisted);
        return Finish(false, "send");
    }
    Step("restart");
    restart ? Restart(state) : mLog.Write(LogLevel::Info, "not restarting, as chosen: the new version starts after the next iDrive restart or reboot");
    state.daemonVersion = files.Version();
    state.daemonSize = files.Get(PackageFiles::kDaemonName)->data.size();
    state.daemonSha256 = PackageFiles::Sha256(files.Get(PackageFiles::kDaemonName)->data);
    state.daemonPresent = true;
    state.daemonBackup = true;
    state.pluginBackup = true;
    state.pluginComplete = true;
    return Finish(true, "send");
}

bool Deployer::WriteEnvOnly(const EnvPlan& env, CarState& state, const Ask& ask)
{
    Begin(4);
    return Finish(Open() && Probe(state) && Remount() && WriteEnv(env, state, ask), "write .env");
}

bool Deployer::RollBack(bool restart, CarState& state)
{
    Begin(6);
    if (!Open() || !Probe(state))
    {
        return Finish(false, "roll back");
    }
    if (!state.daemonBackup && !state.pluginBackup)
    {
        return Finish(Fail("no nbt-aad.old and no " + std::string(kOldPluginDir) + " on the car: nothing to roll back to"), "roll back");
    }
    if (!Remount() || !StopDaemon())
    {
        return Finish(false, "roll back");
    }
    Step("put the .old files back (the current ones become .new)");
    const std::string app = state.appDir;
    const std::string script = (state.daemonBackup ? std::format("rm -f {0}/nbt-aad.new; mv {0}/nbt-aad {0}/nbt-aad.new; mv {0}/nbt-aad.old {0}/nbt-aad && chmod 755 {0}/nbt-aad", app) : std::string("true")) + " && " + (state.pluginBackup ? std::format("rm -rf /opt/aa/NbtAad.new; mv {0} /opt/aa/NbtAad.new; mv {1} {0}", kPluginDir, kOldPluginDir) : std::string("true"));
    const ExecResult swapped = mShell->Run(script);
    if (!swapped.sent || swapped.exitCode != 0)
    {
        return Finish(Fail("the swap did not finish: check " + app + " and " + kPluginDir + " by hand"), "roll back");
    }
    Pass("previous files are back; the replaced ones are in nbt-aad.new and /opt/aa/NbtAad.new");
    Step("restart");
    restart ? Restart(state) : mLog.Write(LogLevel::Info, "not restarting, as chosen");
    return Finish(true, "roll back");
}

bool Deployer::PullLogs(const std::filesystem::path& folder)
{
    Begin(2);
    if (!Open())
    {
        return Finish(false, "pull car logs");
    }
    Step("copy NBT-AAD's RAM logs to " + folder.string());
    std::error_code problem;
    std::filesystem::create_directories(folder, problem);
    uint8_t copied = 0;
    for (const char* path : kCarLogs)
    {
        const ExecResult log = mShell->Run(std::string("cat ") + path, false);
        if (log.sent && log.exitCode == 0)
        {
            std::ofstream(folder / std::filesystem::path(path).filename(), std::ios::binary).write(log.output.data(), static_cast<std::streamsize>(log.output.size()));
            ++copied;
        }
    }
    copied ? Pass(std::format("{} log(s) saved in {}", copied, folder.string())) : Warn("no NBT-AAD logs on the car: they are in RAM and a reboot clears them");
    return Finish(true, "pull car logs");
}

bool Deployer::Open()
{
    Step(std::format("connect to {}:{} as {}", mConnection.host, mConnection.Port(), kCarUser));
    mLog.AddSecret(kCarPassword);
    mSession = std::make_unique<SshSession>();
    std::string error;
    if (!mSession->Connect(mConnection.host, mConnection.Port(), error))
    {
        return Fail(error);
    }
    mLog.Write(LogLevel::Info, "host key " + mSession->HostKeyType() + " " + mSession->HostKeyFingerprint());
    mLog.Write(LogLevel::Info, mSession->NegotiatedMethods());
    const auto start = std::chrono::steady_clock::now();
    if (!mSession->Login(kCarUser, kCarPassword, error))
    {
        return Fail(error);
    }
    // Some head units wait seconds here (the server looking up this PC's name); the time shows whether that's normal.
    Pass(std::format("logged in ({}, {} ms)", mSession->AuthUsed(), std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()));
    mShell = std::make_unique<CarShell>(*mSession, mLog);
    mShell->DetectNode();
    return true;
}

bool Deployer::Probe(CarState& state)
{
    Step("read what is on the car");
    state = CarState{};
    state.node = mShell->Node();
    const std::optional<DirEntry> packaged = DirListing(mShell->Run(std::string("ls -l ") + kPackageAppDir).output).Find("nbt-aad");
    state.packaged = packaged && packaged->executable;
    state.appDir = state.packaged ? kPackageAppDir : kHandAppDir;
    state.dataDir = state.packaged ? kPackageDataDir : kHandDataDir;
    state.envPath = (state.packaged ? state.dataDir : state.appDir) + "/" + kEnvName;
    mLog.Write(LogLevel::Info, state.packaged ? "install type: NBTApps package" : "install type: hand install (/opt/aa)");

    const ExecResult daemon = mShell->Run("cat " + state.appDir + "/nbt-aad", false);
    state.daemonPresent = daemon.sent && daemon.exitCode == 0 && !daemon.output.empty();
    state.daemonVersion = state.daemonPresent ? PackageFiles::VersionIn(daemon.output) : std::string();
    state.daemonSize = state.daemonPresent ? daemon.output.size() : 0;
    state.daemonSha256 = state.daemonPresent ? PackageFiles::Sha256(daemon.output) : std::string();
    mLog.Write(LogLevel::Info, !state.daemonPresent ? std::string("no nbt-aad on the car yet: Send to car installs it") : std::format("nbt-aad on the car: {}, {} bytes, SHA-256 {}", state.daemonVersion.empty() ? std::string("no version tag (builds before v1.9.68 have none; matched against the GitHub releases instead)") : state.daemonVersion, state.daemonSize, state.daemonSha256));
    const ExecResult plugin = mShell->Run("grep create /dev/shmem/nbt-aad-app.log");
    state.pluginVersion = plugin.exitCode == 0 ? TextAfter(FirstLine(plugin.output), "create(name=NbtAad) ") : std::string();
    state.running = mShell->Run("pidin", false).output.find("nbt-aad") != std::string::npos;
    mLog.Write(LogLevel::Info, state.running ? "Android Auto (nbt-aad) is running" : "nbt-aad is not running");

    const ExecResult env = mShell->Run("cat " + state.envPath, false);
    state.envExists = env.sent && env.exitCode == 0;
    state.envText = state.envExists ? env.output : std::string();
    EnvFile envFile(mCatalog);
    envFile.SetText(state.envText);
    for (const EnvLine& line : envFile.Lines())
    {
        const SettingDef* setting = mCatalog.Find(line.Key());
        if (setting && setting->secret)
        {
            mLog.AddSecret(line.Value());  // before the file reaches the log
        }
    }
    mLog.Write(LogLevel::Info, state.envExists ? std::format("{} ({} lines):", state.envPath, envFile.Lines().size()) : state.envPath + " does not exist: every setting is at its default");
    if (state.envExists && !state.envText.empty())
    {
        mLog.Write(LogLevel::Output, state.envText);
    }

    for (const DirEntry& entry : DirListing(mShell->Run("ls -l " + state.dataDir).output).Entries())
    {
        state.dataFiles.push_back(entry.name);
    }
    const ExecResult backups = mShell->Run(std::format("[ -e {}/nbt-aad.old ] && echo daemon; [ -d {} ] && echo plugin; true", state.appDir, kOldPluginDir));
    state.daemonBackup = backups.output.find("daemon") != std::string::npos;
    state.pluginBackup = backups.output.find("plugin") != std::string::npos;
    const DirListing pluginFiles(mShell->Run(std::string("ls -l ") + kPluginDir).output);
    state.pluginComplete = std::all_of(std::begin(PackageFiles::kPluginNames), std::end(PackageFiles::kPluginNames), [&pluginFiles](const char* name) { return pluginFiles.Find(name) && pluginFiles.Find(name)->size > 0; });
    mLog.Write(state.pluginComplete ? LogLevel::Info : LogLevel::Warning, state.pluginComplete ? std::string("plugin folder complete (5 files)") : std::string("plugin folder incomplete: iDrive shows no Android Auto after a restart until it is fixed (Send to car or Roll back)"));
    state.cksumTool = mShell->FindTool("cksum");
    state.restartTool = mShell->FindTool("OnOffDSICommander");
    mLog.Write(LogLevel::Info, state.cksumTool.empty() ? "no cksum on the car: uploads are checked by size" : "uploads are checked with " + state.cksumTool);
    mLog.Write(LogLevel::Info, state.restartTool.empty() ? "no OnOffDSICommander found: a restart means holding the power button for 30 s" : "restart with " + state.restartTool + " appreset");
    state.valid = true;
    Pass("car read");
    return true;
}

bool Deployer::Remount()
{
    Step("make the file system writable");
    // As in NBT-AAD's install guide; a reboot puts it back to read-only.
    const ExecResult result = mShell->Node() == "hu-omap" ? mShell->RunHere("mount -o remount rw /fs/sda0") : mShell->RunHere("mount -o remount rw /net/hu-omap/fs/sda0");
    return result.sent && result.exitCode == 0 ? (Pass("writable until the next reboot"), true) : Fail("remount failed: nothing was changed");
}

bool Deployer::StopDaemon()
{
    Step("stop Android Auto if it is running");
    std::vector<std::string> pids = DaemonPids();
    if (pids.empty())
    {
        Pass("not running");
        return true;
    }
    for (uint8_t attempt = 0; attempt < 6 && !pids.empty(); ++attempt)
    {
        std::string script = attempt < 5 ? "kill" : "kill -9";  // the golden rule: never replace a running program
        for (const std::string& pid : pids)
        {
            script += " " + pid;
        }
        mShell->Run(attempt == 0 || attempt == 5 ? script + "; sleep 1" : std::string("sleep 1"));
        pids = DaemonPids();
    }
    return pids.empty() ? (Pass("stopped"), true) : Fail("nbt-aad keeps running: stop it with Force Stop in iDrive and try again");
}

std::vector<std::string> Deployer::DaemonPids()
{
    return PidsFrom(mShell->Run("pidin", false).output);
}

bool Deployer::Stage(const PackageFiles& files, const CarState& state)
{
    // Next to the current files, so the car keeps its working version until every new file has checked out.
    Step("upload the new files next to the current ones and check them");
    const ExecResult cleared = mShell->Run(std::format("mkdir -p {0} /opt/aa && rm -f {0}/nbt-aad.staged && rm -rf {1} && mkdir -p {1}", state.appDir, kStagedPluginDir));
    if (!cleared.sent || cleared.exitCode != 0)
    {
        return Fail("could not prepare the staging folder");
    }
    for (const PackageFile& file : files.Files())
    {
        if (Cancelled())
        {
            return false;
        }
        const bool daemon = file.name == PackageFiles::kDaemonName;
        const std::string path = daemon ? state.appDir + "/nbt-aad.staged" : std::string(kStagedPluginDir) + "/" + file.name;
        const ExecResult sent = mShell->Upload(path, file.data);
        const ExecResult mode = mShell->Run(std::format("chmod {} {}", daemon ? "755" : "644", path));
        if (!sent.sent || sent.exitCode != 0 || mode.exitCode != 0 || !Verify(path, file.data, state))
        {
            return Fail("upload of " + file.name + " failed");
        }
    }
    Pass("all 6 files uploaded and checked");
    return true;
}

void Deployer::ClearStaged(const CarState& state)
{
    mShell->Run(std::format("rm -f {}/nbt-aad.staged; rm -rf {}", state.appDir, kStagedPluginDir));
}

bool Deployer::Swap(const CarState& state)
{
    Step("swap: the current files become .old, the new ones take their place");
    const bool unfinished = !state.pluginComplete && (state.daemonBackup || state.pluginBackup);
    if (unfinished)
    {
        // An earlier send stopped half way: the .old files are the last working version, so they stay as they are.
        Warn("the plugin folder is incomplete (an earlier send did not finish): the .old files from before it are kept");
    }
    const std::string script = unfinished ? std::format("rm -f {0}/nbt-aad && rm -rf {1} && mv {0}/nbt-aad.staged {0}/nbt-aad && mv {2} {1}", state.appDir, kPluginDir, kStagedPluginDir) : std::format("rm -f {0}/nbt-aad.old && rm -rf {3} && if [ -e {0}/nbt-aad ]; then mv {0}/nbt-aad {0}/nbt-aad.old; fi && if [ -d {1} ]; then mv {1} {3}; fi && mv {0}/nbt-aad.staged {0}/nbt-aad && mv {2} {1}", state.appDir, kPluginDir, kStagedPluginDir, kOldPluginDir);
    const ExecResult result = mShell->Run(script);
    return result.sent && result.exitCode == 0 ? (Pass(std::format("new files in place; the previous version is in {}/nbt-aad.old and {}", state.appDir, kOldPluginDir)), true) : Fail("the swap did not finish");
}

void Deployer::Restore(CarState& state, const std::string& envBefore, bool envExisted)
{
    mSteps = static_cast<uint8_t>(mStep + 2);  // restoring adds its own step, and a fresh read of the car after it
    Step("something failed after the swap: put the previous version back from .old");
    if (!state.daemonPresent && !state.daemonBackup && !state.pluginBackup)
    {
        Warn("this was the first install, so there is no previous version: the new files stay");
    }
    const ExecResult files = mShell->Run(std::format("if [ -e {0}/nbt-aad.old ]; then rm -f {0}/nbt-aad && mv {0}/nbt-aad.old {0}/nbt-aad; fi && if [ -d {2} ]; then rm -rf {1} && mv {2} {1}; fi; rm -f {0}/nbt-aad.staged; rm -rf {3}", state.appDir, kPluginDir, kOldPluginDir, kStagedPluginDir));
    bool restored = files.sent && files.exitCode == 0;
    if (state.envText != envBefore || state.envExists != envExisted)
    {
        const ExecResult env = envExisted ? mShell->Upload(state.envPath + ".new", envBefore) : mShell->Run("rm -f " + state.envPath);
        const ExecResult renamed = envExisted ? mShell->Run(std::format("mv -f {0}.new {0}", state.envPath)) : env;
        restored = restored && env.exitCode == 0 && renamed.exitCode == 0;
    }
    restored ? Pass("the previous version and settings are back: the car is as it was before this send") : mLog.Write(LogLevel::Error, std::format("the restore did not finish: check {} and {} by hand, or use Roll back", state.appDir, kPluginDir));
    Probe(state);
}

bool Deployer::Verify(const std::string& path, const std::string& data, const CarState& state)
{
    const std::optional<DirEntry> entry = DirListing(mShell->Run("ls -l " + path).output).Find(path);  // ls -l of one file prints the path as given
    if (!entry || entry->size != data.size())
    {
        mLog.Write(LogLevel::Error, std::format("  size on the car {} instead of {}", entry ? std::to_string(entry->size) : std::string("unknown"), data.size()));
        return false;
    }
    if (state.cksumTool.empty())
    {
        return true;
    }
    const std::string expected = std::to_string(PackageFiles::Cksum(data));
    const std::string carSum = FirstLine(mShell->Run(state.cksumTool + " " + path).output);
    if (!carSum.starts_with(expected + " "))
    {
        mLog.Write(LogLevel::Error, "  cksum on the car differs from this PC's " + expected);
        return false;
    }
    return true;
}

bool Deployer::WriteEnv(const EnvPlan& env, CarState& state, const Ask& ask)
{
    Step("settings file (nbt-aad.env)");
    if (!env.write)
    {
        Pass("unchanged, not written");
        return true;
    }
    if (state.envText != env.loaded)
    {
        std::string diff;
        for (const std::string& line : EnvFile::Diff(env.loaded, state.envText, mCatalog))
        {
            diff += line + "\n";
        }
        mLog.Write(LogLevel::Warning, "the file on the car changed since Connect:\n" + diff);
        if (!ask("nbt-aad.env on the car changed since you pressed Connect (another tool or the NBTApps phone app?):\n\n" + diff + "\nReplace it with the settings in the tabs?"))
        {
            Warn("kept the car's file, as chosen: press Connect to load it into the tabs");
            return true;
        }
    }
    for (const std::string& line : EnvFile::Diff(state.envText, env.text, mCatalog))
    {
        mLog.Write(LogLevel::Info, "  " + line);
    }
    const std::string folder = state.envPath.substr(0, state.envPath.find_last_of('/'));
    const ExecResult sent = mShell->Run("mkdir -p " + folder);
    const ExecResult written = mShell->Upload(state.envPath + ".new", env.text);
    // Through a new file and a rename, as pf_aa does, so the iDrive app never reads half a file.
    const ExecResult renamed = mShell->Run(std::format("mv -f {0}.new {0}", state.envPath));
    if (!sent.sent || written.exitCode != 0 || renamed.exitCode != 0)
    {
        return Fail("could not write " + state.envPath);
    }
    state.envText = env.text;
    state.envExists = true;
    Pass("written: applies at the next Start of Android Auto in iDrive");
    return true;
}

bool Deployer::Firewall(CarState& state)
{
    Step("firewall rule for Android Auto (nbt-aad --pf-install, then --pf-check)");
    const ExecResult install = mShell->Run(state.appDir + "/nbt-aad --pf-install");
    const ExecResult check = mShell->Run(state.appDir + "/nbt-aad --pf-check");
    const ExecResult env = mShell->Run("cat " + state.envPath, false);  // --pf-install may add NBT_KEEP_PF=1
    state.envText = env.exitCode == 0 ? env.output : state.envText;
    state.envExists = state.envExists || env.exitCode == 0;
    // The new nbt-aad runs here for the first time: if it can't even do this, it is not fit to stay on the car.
    // pf_aa --check: 0 = rules in pf.conf and loaded, 9 = in pf.conf but loaded only at the next reboot (normal right
    // after --pf-install), 7 = rules, pf.conf syntax or NBT_KEEP_PF wrong. Anything else is an error of its own.
    if (install.exitCode != 0 || (check.exitCode != 0 && check.exitCode != kPfRulesNotLoadedYet))
    {
        return Fail("the firewall step failed with the new nbt-aad: see its output above");
    }
    Pass(check.exitCode == 0 ? "firewall rule in place and active" : "firewall rule in place; the car loads it at the next reboot");
    return true;
}

void Deployer::Restart(const CarState& state)
{
    if (state.restartTool.empty())
    {
        Warn("restart by holding the power button for 30 s: OnOffDSICommander was not found");
        return;
    }
    // An exec channel has no login environment: OnOffDSICommander then can't find its libraries (seen on the car), so
    // first with the library path the car's own launcher uses, then with the login profile the install guide's shell has.
    for (const std::string& environment : { std::format("LD_LIBRARY_PATH={}${{LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}}", kCarLibraryPath), std::string(". /etc/profile >/dev/null 2>&1;") })
    {
        // appreset restarts iDrive and never returns, so it runs in the background; an "exit" line within 3 s means it ended
        // early, and anything but "exit 0" means it failed (a missing library ends it at once).
        const ExecResult started = mShell->Run(std::format("( {0} {1} appreset < /dev/null; echo \"exit $?\" ) > {2} 2>&1 & sleep 3; cat {2}", environment, state.restartTool, kRestartOutput));
        const size_t exitLine = started.output.find("exit ");
        if (started.sent && started.exitCode == 0 && (exitLine == std::string::npos || started.output.compare(exitLine, 6, "exit 0") == 0))
        {
            Pass("iDrive restarts now: the screen goes dark for about a minute");
            return;
        }
    }
    mShell->Run(std::string("for d in ") + kCarLibraryFolders + "; do ls $d 2>/dev/null | grep -i servicebroker | sed \"s|^|$d/|\"; done");  // where the missing library lives, for the next fix
    Warn("restart command failed: hold the power button for 30 s instead (the new version is already in place)");
}

void Deployer::PullLogsAfterFailure()
{
    const std::filesystem::path folder = mLog.FilePath().parent_path() / ("car-" + mLog.FilePath().stem().string());
    std::error_code problem;
    std::filesystem::create_directories(folder, problem);
    for (const char* path : kCarLogs)
    {
        const ExecResult log = mShell->Run(std::string("cat ") + path, false);
        if (log.sent && log.exitCode == 0)
        {
            std::ofstream(folder / std::filesystem::path(path).filename(), std::ios::binary).write(log.output.data(), static_cast<std::streamsize>(log.output.size()));
        }
    }
    mLog.Write(LogLevel::Info, "car logs saved in " + folder.string());
}

bool Deployer::Cancelled()
{
    return mCancel ? Fail("cancelled") : false;
}

void Deployer::Begin(uint8_t steps)
{
    mStep = 0;
    mSteps = steps;
    mWarnings = 0;
}

void Deployer::Step(const std::string& title)
{
    mLog.Write(LogLevel::Step, std::format("{}/{} {}", ++mStep, mSteps, title));
}

void Deployer::Pass(const std::string& text)
{
    mLog.Write(LogLevel::Pass, text);
}

void Deployer::Warn(const std::string& text)
{
    ++mWarnings;
    mLog.Write(LogLevel::Warning, text);
}

bool Deployer::Fail(const std::string& text)
{
    mLog.Write(LogLevel::Error, text);
    return false;
}

bool Deployer::Finish(bool ok, const std::string& what)
{
    mLog.Write(ok ? (mWarnings ? LogLevel::Warning : LogLevel::Pass) : LogLevel::Error, std::format("{} {}{}. Log file: {}", what, ok ? "finished" : "failed", ok && mWarnings ? std::format(" with {} warning(s)", mWarnings) : std::string(), mLog.FilePath().string()));
    if (mSession)
    {
        mSession->Close();
    }
    return ok;
}

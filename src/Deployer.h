// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include "CarLogin.h"
#include "CarShell.h"
#include "CarState.h"
#include "PackageSource.h"
#include "RunLog.h"
#include "SettingsCatalog.h"
#include "SshSession.h"

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

// The settings file as the tabs left it, and as Connect read it, so a change on the car since then is noticed.
struct EnvPlan
{
    bool        write = false;
    std::string text;
    std::string loaded;
};

// Everything that talks to the car beyond Find car: Connect, Send to car, Write .env only, Roll back, Pull car logs.
class Deployer
{
public:
    using Ask = std::function<bool(const std::string& question)>;  // asked on the UI thread; the worker waits

    Deployer(CarConnection connection, RunLog& log, const SettingsCatalog& catalog, const std::atomic<bool>& cancel);

    bool Connect(CarState& state);
    bool Send(PackageSource& source, const EnvPlan& env, bool restart, CarState& state, const Ask& ask);
    bool WriteEnvOnly(const EnvPlan& env, CarState& state, const Ask& ask);
    bool RollBack(bool restart, CarState& state);
    bool PullLogs(const std::filesystem::path& folder);

private:
    bool Open();
    bool Probe(CarState& state);
    bool Remount();
    bool StopDaemon();
    std::vector<std::string> DaemonPids();
    bool Stage(const PackageFiles& files, const CarState& state);
    void ClearStaged(const CarState& state);
    bool Swap(const CarState& state);
    void Restore(CarState& state, const std::string& envBefore, bool envExisted);
    bool Verify(const std::string& path, const std::string& data, const CarState& state);
    bool WriteEnv(const EnvPlan& env, CarState& state, const Ask& ask);
    bool Firewall(CarState& state);
    void Restart(const CarState& state);
    void PullLogsAfterFailure();
    bool Cancelled();

    void Begin(uint8_t steps);
    void Step(const std::string& title);
    void Pass(const std::string& text);
    void Warn(const std::string& text);
    bool Fail(const std::string& text);  // always false, so a step can `return Fail(...)`
    bool Finish(bool ok, const std::string& what);

    static constexpr const char* kPluginDir = "/opt/hmi/ID5/data/ro/Plugins/NbtAad";
    static constexpr const char* kOldPluginDir = "/opt/aa/NbtAad.old";  // outside Plugins/, or iDrive may load it as a second app
    static constexpr int32_t kPfRulesNotLoadedYet = 9;  // pf_aa --check: pf.conf is right, the running filter reads it at reboot
    static constexpr const char* kStagedPluginDir = "/opt/aa/NbtAad.staged";  // likewise; same file system, so the swap is a rename

    CarConnection               mConnection;
    RunLog&                     mLog;
    const SettingsCatalog&      mCatalog;
    const std::atomic<bool>&    mCancel;
    std::unique_ptr<SshSession> mSession;
    std::unique_ptr<CarShell>   mShell;
    uint8_t                     mStep = 0;
    uint8_t                     mSteps = 0;
    uint8_t                     mWarnings = 0;
};

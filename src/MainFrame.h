// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include "CarFinder.h"
#include "CarState.h"
#include "Deployer.h"
#include "GitHubReleases.h"
#include "RunLog.h"
#include "SettingsCatalog.h"

#include <wx/frame.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <thread>

class GitHubPanel;
class LocalPanel;
class LogPanel;
class SettingsBook;
class wxButton;
class wxCheckBox;
class wxStaticText;
class wxTextCtrl;

class MainFrame : public wxFrame
{
public:
    explicit MainFrame(const SettingsCatalog& catalog);
    ~MainFrame() override;

private:
    void OnConnect();
    void OnRefreshReleases();
    void OnSendGitHub();
    void OnSendLocal();
    void OnWriteEnv();
    void OnRollBack();
    void OnPullLogs();
    void OnClose(wxCloseEvent& event);

    bool StartRun(const std::string& logPrefix, wxButton* button, std::function<void()> work);
    void CarRunFinished(bool ok, const CarState& state);
    void SetBusy(bool busy, wxButton* running = nullptr);
    bool CheckEnvBeforeSending(bool needsUnlockCheck);
    EnvPlan MakeEnvPlan() const;
    std::string EnvSummary() const;
    bool AskFromWorker(const std::string& question);
    void ShowCar();
    std::string CarVersionText() const;
    CarConnection Connection() const;
    bool RequireConnect();

    void LoadSettings();
    void SaveSettings() const;
    static std::filesystem::path ExeFolder();

    const SettingsCatalog&  mCatalog;
    wxTextCtrl*             mHost = nullptr;
    wxButton*               mConnect = nullptr;
    wxStaticText*           mCarInfo = nullptr;
    GitHubPanel*            mGitHub = nullptr;
    LocalPanel*             mLocal = nullptr;
    SettingsBook*           mBook = nullptr;
    wxButton*               mWriteEnv = nullptr;
    wxButton*               mRollBack = nullptr;
    wxButton*               mPullLogs = nullptr;
    wxCheckBox*             mRestart = nullptr;
    LogPanel*               mLogPanel = nullptr;

    CarState                 mCar;          // the last Connect or run; the tabs show its nbt-aad.env
    std::vector<ReleaseInfo> mAllReleases;  // every release fetched, not just the 20 shown: to name an older build on the car
    std::unique_ptr<RunLog>  mRunLog;       // one per run, so each run gets its own file
    std::thread              mWorker;       // SSH, downloads and discovery block for seconds; the window must stay responsive
    std::atomic<bool>        mCancel{ false };
    bool                     mBusy = false;
};

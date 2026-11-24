// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include "MainFrame.h"

#include "EnvFile.h"
#include "LogPanel.h"
#include "PackageSource.h"
#include "SettingsBook.h"
#include "SourcePanels.h"
#include "Version.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/fileconf.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>

#include <chrono>
#include <format>
#include <future>
#include <numeric>

namespace
{
    constexpr const char* kTitle = "NBT-AA-Installer";
    constexpr const char* kSettingsFile = "NBT-AA-Installer.ini";
    constexpr size_t kSummaryLines = 14;  // enough to read in a dialog; the log has the full diff

    std::string JoinList(const std::vector<std::string>& items)
    {
        return std::accumulate(items.begin(), items.end(), std::string(), [](const std::string& list, const std::string& item) { return list.empty() ? item : list + ", " + item; });
    }
}

MainFrame::MainFrame(const SettingsCatalog& catalog) : wxFrame(nullptr, wxID_ANY, std::format("{} {}", kTitle, kAppVersion), wxDefaultPosition, wxSize(1180, 980)), mCatalog(catalog)
{
    auto* panel = new wxPanel(this);
    mHost = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(170), -1));
    mConnect = new wxButton(panel, wxID_ANY, "Connect");
    mCarInfo = new wxStaticText(panel, wxID_ANY, wxEmptyString);
    mConnect->SetToolTip("Reads the car: install type, versions, nbt-aad.env and the iDrive options. Nothing is changed.\nIf nothing answers at Host, or Host is empty, it first looks for the car (TurboCode proxy, BMW's discovery broadcast, SSH on every car address) and fills in Host.");
    mHost->SetHint("empty = find the car");
    mConnect->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnConnect(); });

    auto* githubBox = new wxStaticBoxSizer(wxVERTICAL, panel, "From GitHub");
    mGitHub = new GitHubPanel(githubBox->GetStaticBox(), [this]() { OnRefreshReleases(); }, [this]() { OnSendGitHub(); });
    githubBox->Add(mGitHub, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(4)));
    auto* localBox = new wxStaticBoxSizer(wxVERTICAL, panel, "From this PC");
    mLocal = new LocalPanel(localBox->GetStaticBox(), [this]() { OnSendLocal(); });
    localBox->Add(mLocal, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(4)));

    mBook = new SettingsBook(panel, catalog);
    mWriteEnv = new wxButton(panel, wxID_ANY, "Write .env only");
    mRollBack = new wxButton(panel, wxID_ANY, "Roll back");
    mPullLogs = new wxButton(panel, wxID_ANY, "Pull car logs");
    mRestart = new wxCheckBox(panel, wxID_ANY, "Restart iDrive after sending or rolling back");
    mWriteEnv->SetToolTip("Writes only nbt-aad.env from the tabs. It applies at the next Start of Android Auto in iDrive.");
    mRollBack->SetToolTip("Puts back the files kept as .old by the last Send to car.");
    mPullLogs->SetToolTip("Copies NBT-AAD's logs from the car's RAM into the logs folder next to this program.");
    mWriteEnv->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnWriteEnv(); });
    mRollBack->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnRollBack(); });
    mPullLogs->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnPullLogs(); });
    mLogPanel = new LogPanel(panel);
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);

    auto* connection = new wxBoxSizer(wxHORIZONTAL);
    connection->Add(new wxStaticText(panel, wxID_ANY, "Host"), wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(4)));
    connection->Add(mHost, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(4)));
    connection->Add(mConnect, wxSizerFlags().CenterVertical().Border(wxRIGHT, FromDIP(12)));
    connection->Add(mCarInfo, wxSizerFlags(1).CenterVertical());
    auto* sources = new wxBoxSizer(wxHORIZONTAL);
    sources->Add(githubBox, wxSizerFlags(1).Expand().Border(wxRIGHT, FromDIP(6)));
    sources->Add(localBox, wxSizerFlags(1).Expand());
    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    actions->Add(mWriteEnv, wxSizerFlags().Border(wxRIGHT, FromDIP(4)));
    actions->Add(mRollBack, wxSizerFlags().Border(wxRIGHT, FromDIP(4)));
    actions->Add(mPullLogs);
    actions->AddStretchSpacer();
    actions->Add(mRestart, wxSizerFlags().CenterVertical());
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(connection, wxSizerFlags().Expand().Border(wxALL, FromDIP(8)));
    sizer->Add(sources, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, FromDIP(8)));
    sizer->Add(mBook, wxSizerFlags(5).Expand().Border(wxALL, FromDIP(8)));
    sizer->Add(actions, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, FromDIP(8)));
    sizer->Add(mLogPanel, wxSizerFlags(3).Expand().Border(wxALL, FromDIP(8)));
    panel->SetSizer(sizer);
    SetMinSize(FromDIP(wxSize(900, 760)));

    LoadSettings();
    ShowCar();
    SetBusy(false);
    CallAfter([this]() { OnRefreshReleases(); });  // the list is what most people come for
}

MainFrame::~MainFrame()
{
    mCancel = true;
    if (mWorker.joinable())
    {
        mWorker.join();
    }
}

void MainFrame::OnConnect()
{
    mBook->Flush();
    if (mCar.valid && mBook->Changed() && wxMessageBox("Your changes in the settings tabs are not on the car yet. Connect reads the car's file again and replaces them.\n\nContinue?", kTitle, wxOK | wxCANCEL | wxICON_QUESTION, this) != wxOK)
    {
        return;
    }
    SaveSettings();
    StartRun("connect", mConnect, [this, connection = Connection()]()
    {
        CarState state;
        bool ok = !connection.host.empty() && Deployer(connection, *mRunLog, mCatalog, mCancel).Connect(state);
        if (!ok && !state.valid && !mCancel)
        {
            // Nothing usable at Host, or no Host yet: look for the car, then connect to what was found.
            mRunLog->Write(LogLevel::Info, connection.host.empty() ? std::string("no host yet: looking for the car") : "no car at " + connection.host + ": looking for it");
            const FindResult found = CarFinder(*mRunLog).Find(mCancel);
            if (found.found && found.host != connection.host)
            {
                CallAfter([this, host = found.host]() { mHost->SetValue(wxString::FromUTF8(host)); SaveSettings(); });
                ok = Deployer(CarConnection{ found.host }, *mRunLog, mCatalog, mCancel).Connect(state);
            }
        }
        CallAfter([this, ok, state]() { CarRunFinished(ok, state); });
    });
}

void MainFrame::OnRefreshReleases()
{
    mGitHub->SetMessage("Reading the release list...");
    StartRun("releases", nullptr, [this]()
    {
        GitHubReleases list;
        std::string error;
        const bool ok = list.Fetch(mCatalog.Releases(), *mRunLog, mCancel, error);
        mRunLog->Write(ok ? LogLevel::Pass : LogLevel::Error, ok ? std::format("{} releases listed", list.Releases().size()) : error);
        CallAfter([this, ok, error, releases = list.Releases(), all = list.All()]()
        {
            SetBusy(false);
            mAllReleases = ok ? all : mAllReleases;
            ok ? mGitHub->SetReleases(releases) : mGitHub->SetMessage("⚠ " + error);
            ShowCar();  // an untagged build on the car can now be recognised
        });
    });
}

void MainFrame::OnSendGitHub()
{
    const ReleaseInfo* release = mGitHub->Selected();
    if (!release || !RequireConnect() || !CheckEnvBeforeSending(true))
    {
        return;
    }
    const NbtVersion car = NbtVersion::Find(CarVersionText());
    const NbtVersion picked = NbtVersion::Find(release->tag);
    std::string notes = !release->stable ? "\n⚠ This release is not marked stable: it is a test build." : std::string();
    notes += car.valid && picked.valid && picked.Compare(car) == 0 ? "\n⚠ The car already runs this version." : car.valid && picked.valid && picked.Compare(car) < 0 ? "\n⚠ This is OLDER than the version on the car." : std::string();
    const std::string question = std::format("Send {} ({}, {}) to the car at {}?\n\nOn the car now: {}{}\n\nSettings: {}\n\nThe tool downloads the files, stops Android Auto, keeps the current files as .old, uploads and checks the new ones, writes the settings and sets up the firewall rule{}.", release->name, release->tag, release->Date(), Connection().host, CarVersionText(), notes, EnvSummary(), mRestart->IsChecked() ? ", then restarts iDrive" : "");
    if (wxMessageBox(wxString::FromUTF8(question), "Send to car", wxOK | wxCANCEL | wxICON_QUESTION, this) != wxOK)
    {
        return;
    }
    auto source = std::make_shared<GitHubSource>(*release, ExeFolder() / "downloads");
    StartRun("send", nullptr, [this, source, plan = MakeEnvPlan(), restart = mRestart->IsChecked(), connection = Connection()]()
    {
        CarState state;
        const bool ok = Deployer(connection, *mRunLog, mCatalog, mCancel).Send(*source, plan, restart, state, [this](const std::string& question) { return AskFromWorker(question); });
        CallAfter([this, ok, state]() { CarRunFinished(ok, state); });
    });
}

void MainFrame::OnSendLocal()
{
    const std::string path = mLocal->Path();
    if (path.empty())
    {
        wxMessageBox("Choose a folder first.", kTitle, wxOK | wxICON_INFORMATION, this);
        return;
    }
    if (!RequireConnect() || !CheckEnvBeforeSending(true))
    {
        return;
    }
    auto source = std::make_shared<LocalSource>(std::filesystem::path(wxString::FromUTF8(path).ToStdWstring()));
    PackageFiles files;
    RunLog quiet;
    std::string error;
    std::string missing;
    const std::atomic<bool> never{ false };
    if (!source->Collect(files, quiet, never, error) || !files.Complete(missing))
    {
        wxMessageBox(wxString::FromUTF8("Nothing was sent: " + (error.empty() ? missing : error)), kTitle, wxOK | wxICON_WARNING, this);
        return;
    }
    const NbtVersion car = NbtVersion::Find(CarVersionText());
    const NbtVersion picked = NbtVersion::Find(files.Version());
    const std::string notes = car.valid && picked.valid && picked.Compare(car) < 0 ? "\n⚠ This is OLDER than the version on the car." : std::string();
    const std::string question = std::format("Send the files from\n{}\n({}) to the car at {}?\n\nOn the car now: {}{}\n\nSettings: {}\n\nThe tool stops Android Auto, keeps the current files as .old, uploads and checks the new ones, writes the settings and sets up the firewall rule{}.", path, files.Version().empty() ? std::string("version unknown") : files.Version(), Connection().host, CarVersionText(), notes, EnvSummary(), mRestart->IsChecked() ? ", then restarts iDrive" : "");
    if (wxMessageBox(wxString::FromUTF8(question), "Send to car", wxOK | wxCANCEL | wxICON_QUESTION, this) != wxOK)
    {
        return;
    }
    SaveSettings();
    StartRun("send", nullptr, [this, source, plan = MakeEnvPlan(), restart = mRestart->IsChecked(), connection = Connection()]()
    {
        CarState state;
        const bool ok = Deployer(connection, *mRunLog, mCatalog, mCancel).Send(*source, plan, restart, state, [this](const std::string& question) { return AskFromWorker(question); });
        CallAfter([this, ok, state]() { CarRunFinished(ok, state); });
    });
}

void MainFrame::OnWriteEnv()
{
    mBook->Flush();
    if (!RequireConnect())
    {
        return;
    }
    if (!mBook->Changed())
    {
        wxMessageBox("The settings are the same as on the car: nothing to write.", kTitle, wxOK | wxICON_INFORMATION, this);
        return;
    }
    if (!CheckEnvBeforeSending(true) || wxMessageBox(wxString::FromUTF8("Write these changes to nbt-aad.env on the car?\n\n" + EnvSummary() + "\n\nThey apply at the next Start of Android Auto in iDrive."), "Write .env only", wxOK | wxCANCEL | wxICON_QUESTION, this) != wxOK)
    {
        return;
    }
    StartRun("env", mWriteEnv, [this, plan = MakeEnvPlan(), connection = Connection()]()
    {
        CarState state;
        const bool ok = Deployer(connection, *mRunLog, mCatalog, mCancel).WriteEnvOnly(plan, state, [this](const std::string& question) { return AskFromWorker(question); });
        CallAfter([this, ok, state]() { CarRunFinished(ok, state); });
    });
}

void MainFrame::OnRollBack()
{
    if (!RequireConnect())
    {
        return;
    }
    if (!mCar.daemonBackup && !mCar.pluginBackup)
    {
        wxMessageBox("There are no .old files on the car to go back to.", kTitle, wxOK | wxICON_INFORMATION, this);
        return;
    }
    if (wxMessageBox(std::format("Put back the previous version kept as .old ({}{})? The current files are kept as .new.{}", mCar.daemonBackup ? "nbt-aad" : "", mCar.pluginBackup ? (mCar.daemonBackup ? " and the plugin" : "the plugin") : "", mRestart->IsChecked() ? "\n\niDrive restarts afterwards." : ""), "Roll back", wxOK | wxCANCEL | wxICON_WARNING, this) != wxOK)
    {
        return;
    }
    StartRun("rollback", mRollBack, [this, restart = mRestart->IsChecked(), connection = Connection()]()
    {
        CarState state;
        const bool ok = Deployer(connection, *mRunLog, mCatalog, mCancel).RollBack(restart, state);
        CallAfter([this, ok, state]() { CarRunFinished(ok, state); });
    });
}

void MainFrame::OnPullLogs()
{
    SaveSettings();
    StartRun("carlogs", mPullLogs, [this, connection = Connection()]()
    {
        Deployer(connection, *mRunLog, mCatalog, mCancel).PullLogs(mRunLog->FilePath().parent_path() / mRunLog->FilePath().stem());
        CallAfter([this]() { SetBusy(false); });
    });
}

void MainFrame::OnClose(wxCloseEvent& event)
{
    if (mBusy && event.CanVeto() && wxMessageBox("A run is still going. Closing stops it after the command in progress, which can leave the car half updated.\n\nClose anyway?", kTitle, wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES)
    {
        event.Veto();
        return;
    }
    mCancel = true;
    SaveSettings();
    event.Skip();
}

bool MainFrame::StartRun(const std::string& logPrefix, wxButton* button, std::function<void()> work)
{
    if (mBusy)
    {
        return false;
    }
    if (mWorker.joinable())
    {
        mWorker.join();  // the previous run already finished: its last act was SetBusy(false)
    }
    mRunLog = std::make_unique<RunLog>();
    if (!mRunLog->Open(ExeFolder() / "logs", logPrefix))
    {
        wxMessageBox("Cannot create the logs folder next to the .exe. Run the tool from a folder you can write to.", kTitle, wxOK | wxICON_ERROR, this);
        return false;
    }
    mRunLog->SetSink([this](LogLevel level, const std::string& line) { mLogPanel->Append(level, line); });
    mRunLog->AddSecret(kCarPassword);
    mRunLog->Write(LogLevel::Info, std::format("{} {}", kTitle, kAppVersion));  // which build wrote a log matters when reading it later
    mCancel = false;
    SetBusy(true, button);
    mWorker = std::thread(std::move(work));
    return true;
}

void MainFrame::CarRunFinished(bool ok, const CarState& state)
{
    SetBusy(false);
    if (state.valid)
    {
        mCar = state;
        mBook->Load(state.envText, state.dataFiles);  // what is on the car now, NBT_KEEP_PF from the firewall step included
    }
    ShowCar();
    if (!ok)
    {
        wxBell();
    }
}

void MainFrame::SetBusy(bool busy, wxButton* running)
{
    mBusy = busy;
    for (wxWindow* control : { static_cast<wxWindow*>(mHost), static_cast<wxWindow*>(mConnect), static_cast<wxWindow*>(mPullLogs), static_cast<wxWindow*>(mRestart) })
    {
        control->Enable(!busy);
    }
    mWriteEnv->Enable(!busy && mCar.valid);
    mRollBack->Enable(!busy && mCar.valid);
    mBook->Enable(!busy && mCar.valid);
    mGitHub->SetBusy(busy);
    mLocal->SetBusy(busy);
    mConnect->SetLabel(running == mConnect ? "Connecting..." : "Connect");
    mWriteEnv->SetLabel(running == mWriteEnv ? "Writing..." : "Write .env only");
    mRollBack->SetLabel(running == mRollBack ? "Rolling back..." : "Roll back");
    mPullLogs->SetLabel(running == mPullLogs ? "Pulling logs..." : "Pull car logs");
}

bool MainFrame::CheckEnvBeforeSending(bool needsUnlockCheck)
{
    mBook->Flush();
    const EnvFile& env = mBook->Env();
    const std::vector<uint16_t> longLines = env.LongLines();
    if (!longLines.empty())
    {
        std::string numbers;
        for (uint16_t line : longLines)
        {
            numbers += (numbers.empty() ? "" : ", ") + std::to_string(line);
        }
        wxMessageBox(wxString::FromUTF8(std::format("Line {} of nbt-aad.env is longer than {} characters. The car cuts such lines in two, and the rest can become a broken setting.\n\nShorten it in the File tab. Nothing was sent.", numbers, mCatalog.MaxLineLength())), kTitle, wxOK | wxICON_ERROR, this);
        return false;
    }
    const std::vector<std::string> ignored = env.IgnoredKeys();
    if (!ignored.empty() && wxMessageBox(wxString::FromUTF8(std::format("The car passes only the first {} settings to Android Auto. These will be ignored:\n\n{}\n\nSend anyway?", mCatalog.MaxSettings(), JoinList(ignored))), kTitle, wxOK | wxCANCEL | wxICON_WARNING, this) != wxOK)
    {
        return false;
    }
    const std::vector<std::string> overrides = mBook->ActiveOverrides();
    if (needsUnlockCheck && !mBook->OverridesUnlocked() && !overrides.empty() && wxMessageBox(wxString::FromUTF8("These lines were typed in the File tab and override iDrive's Android Auto options:\n\n" + JoinList(overrides) + "\n\nA line in nbt-aad.env always wins over the checkbox in iDrive. Send anyway?"), "iDrive overrides", wxOK | wxCANCEL | wxICON_WARNING, this) != wxOK)
    {
        return false;
    }
    return true;
}

EnvPlan MainFrame::MakeEnvPlan() const
{
    return EnvPlan{ mBook->Changed(), mBook->Text(), mCar.envText };
}

std::string MainFrame::EnvSummary() const
{
    const std::vector<std::string> changes = EnvFile::Diff(mCar.envText, mBook->Text(), mCatalog);
    if (!mBook->Changed() || changes.empty())
    {
        return "no changes, the car's nbt-aad.env stays as it is";
    }
    std::string summary = "nbt-aad.env changes:";
    for (size_t i = 0; i < changes.size() && i < kSummaryLines; ++i)
    {
        summary += "\n  " + changes[i];
    }
    return changes.size() > kSummaryLines ? summary + std::format("\n  ... and {} more", changes.size() - kSummaryLines) : summary;
}

bool MainFrame::AskFromWorker(const std::string& question)
{
    // Shared: if the window goes away first, the queued dialog must not touch a dead promise.
    auto answer = std::make_shared<std::promise<bool>>();
    std::future<bool> future = answer->get_future();
    CallAfter([this, answer, question]() { answer->set_value(wxMessageBox(wxString::FromUTF8(question), kTitle, wxOK | wxCANCEL | wxICON_WARNING, this) == wxOK); });
    while (future.wait_for(std::chrono::milliseconds(200)) != std::future_status::ready)
    {
        if (mCancel)
        {
            return false;
        }
    }
    return future.get();
}

void MainFrame::ShowCar()
{
    mCarInfo->SetLabel(wxString::FromUTF8(!mCar.valid ? std::string("Not connected: press Connect to read the car before changing anything") : std::format("{}  ·  {}  ·  nbt-aad {}  ·  {}{}", mCar.node, mCar.packaged ? "NBTApps package" : "hand install", mCar.daemonPresent ? CarVersionText() : std::string("not installed"), mCar.running ? "running" : "not running", mCar.daemonBackup || mCar.pluginBackup ? "  ·  .old backup on the car" : "")));
    mGitHub->SetCarDaemon(mCar.valid ? mCar.daemonVersion : std::string(), mCar.valid ? mCar.daemonSize : 0, mCar.daemonSha256);
    SetBusy(mBusy);
}

std::string MainFrame::CarVersionText() const
{
    if (!mCar.daemonVersion.empty())
    {
        return mCar.daemonVersion;
    }
    // Builds before v1.9.68 carry no version tag: recognise them by their SHA-256 (or size) among the GitHub releases.
    const ReleaseInfo* release = GitHubReleases::Match(mAllReleases, mCar.daemonSize, mCar.daemonSha256);
    return release ? std::format("{} (recognised by {})", release->tag, release->daemon.sha256.empty() ? "size" : "SHA-256") : std::string("unknown version (own build?)");
}

CarConnection MainFrame::Connection() const
{
    return CarConnection{ mHost->GetValue().Trim().Trim(false).utf8_string() };
}

bool MainFrame::RequireConnect()
{
    if (mCar.valid)
    {
        return true;
    }
    wxMessageBox("Press Connect first: the settings tabs must show the car's nbt-aad.env before anything is sent, or your settings on the car would be lost.", kTitle, wxOK | wxICON_INFORMATION, this);
    return false;
}

void MainFrame::LoadSettings()
{
    // Empty at first: Connect then finds the car itself (the TurboCode proxy included) and fills in the address.
    wxFileConfig config(wxEmptyString, wxEmptyString, (ExeFolder() / kSettingsFile).wstring(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE);
    mHost->SetValue(config.Read("Host", wxEmptyString));
    mLocal->SetPath(config.Read("LocalPath", wxEmptyString).utf8_string());
    mRestart->SetValue(config.ReadBool("Restart", true));
    mBook->ShowAdvanced(config.ReadBool("ShowAdvanced", false));
}

void MainFrame::SaveSettings() const
{
    wxFileConfig config(wxEmptyString, wxEmptyString, (ExeFolder() / kSettingsFile).wstring(), wxEmptyString, wxCONFIG_USE_LOCAL_FILE);
    config.Write("Host", mHost->GetValue());
    config.Write("LocalPath", wxString::FromUTF8(mLocal->Path()));
    config.Write("Restart", mRestart->IsChecked());
    config.Write("ShowAdvanced", mBook->AdvancedShown());
}

std::filesystem::path MainFrame::ExeFolder()
{
    return std::filesystem::path(wxStandardPaths::Get().GetExecutablePath().ToStdWstring()).parent_path();
}

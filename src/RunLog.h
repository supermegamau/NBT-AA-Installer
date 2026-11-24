// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include "LogLevel.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

// One log file per run, plus a live copy for the window. Safe to call from any thread.
class RunLog
{
public:
    using Sink = std::function<void(LogLevel, const std::string&)>;

    bool Open(const std::filesystem::path& folder, const std::string& prefix);
    void SetSink(Sink sink);
    void AddSecret(const std::string& secret);
    void Write(LogLevel level, const std::string& text);
    const std::filesystem::path& FilePath() const { return mPath; }

private:
    void WriteLine(LogLevel level, const std::string& line);
    std::string Mask(std::string text) const;
    static void Prune(const std::filesystem::path& folder, const std::string& prefix);

    static constexpr uint8_t kKeepLogs = 50;

    std::mutex               mMutex;    // the worker thread and the UI both log
    std::ofstream            mFile;
    std::filesystem::path    mPath;
    std::vector<std::string> mSecrets;  // never reach the file or the window, even inside commands
    Sink                     mSink;
};

// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include "RunLog.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <format>
#include <sstream>

namespace
{
    std::tm LocalNow(std::chrono::system_clock::time_point now)
    {
        std::tm local{};
        const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
        localtime_s(&local, &seconds);
        return local;
    }

    const char* Tag(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Step:    return "[STEP] ";
        case LogLevel::Command: return "  $ ";
        case LogLevel::Output:  return "    ";
        case LogLevel::Pass:    return "[ OK ] ";
        case LogLevel::Warning: return "[WARN] ";
        case LogLevel::Error:   return "[FAIL] ";
        default:                return "";
        }
    }
}

bool RunLog::Open(const std::filesystem::path& folder, const std::string& prefix)
{
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    const std::tm local = LocalNow(std::chrono::system_clock::now());
    mPath = folder / std::format("{}-{:04}-{:02}-{:02}_{:02}-{:02}-{:02}.log", prefix, local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);
    mFile.open(mPath, std::ios::out | std::ios::app | std::ios::binary);
    if (!mFile.is_open())
    {
        return false;
    }
    Prune(folder, prefix);
    return true;
}

void RunLog::SetSink(Sink sink)
{
    std::lock_guard lock(mMutex);
    mSink = std::move(sink);
}

void RunLog::AddSecret(const std::string& secret)
{
    std::lock_guard lock(mMutex);
    if (!secret.empty())
    {
        mSecrets.push_back(secret);
    }
}

void RunLog::Write(LogLevel level, const std::string& text)
{
    std::lock_guard lock(mMutex);
    std::istringstream lines(Mask(text));
    std::string line;
    while (std::getline(lines, line))
    {
        line.erase(std::find_if(line.rbegin(), line.rend(), [](char c) { return c != '\r'; }).base(), line.end());
        WriteLine(level, line);
    }
}

void RunLog::WriteLine(LogLevel level, const std::string& line)
{
    const auto now = std::chrono::system_clock::now();
    const std::tm local = LocalNow(now);
    const std::string stamped = std::format("{:02}:{:02}:{:02}.{:03} {}{}", local.tm_hour, local.tm_min, local.tm_sec, static_cast<int32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000), Tag(level), line);
    if (mFile.is_open())
    {
        mFile << stamped << "\r\n";
        mFile.flush();  // a crash or a pulled cable must not lose the end of the log
    }
    if (mSink)
    {
        mSink(level, stamped);
    }
}

std::string RunLog::Mask(std::string text) const
{
    for (const std::string& secret : mSecrets)
    {
        for (size_t at = text.find(secret); at != std::string::npos; at = text.find(secret, at + 3))
        {
            text.replace(at, secret.size(), "***");
        }
    }
    return text;
}

void RunLog::Prune(const std::filesystem::path& folder, const std::string& prefix)
{
    std::vector<std::filesystem::path> logs;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(folder, error))
    {
        if (entry.path().extension() == ".log" && entry.path().filename().string().starts_with(prefix + "-"))
        {
            logs.push_back(entry.path());
        }
    }
    std::sort(logs.begin(), logs.end());  // the timestamp in the name sorts oldest first
    for (size_t i = 0; i + kKeepLogs < logs.size(); ++i)
    {
        std::filesystem::remove(logs[i], error);
    }
}

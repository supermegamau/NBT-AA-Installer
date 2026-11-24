// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include "CarShell.h"

#include <algorithm>
#include <format>

namespace
{
    // The car has no `which`: these are the PATH folders the car hands its own programs.
    constexpr const char* kToolFolders = "/proc/boot /bin /usr/bin /sbin /usr/sbin /opt/os/bin /opt/sys/bin /opt/core/bin /opt/car/bin";
}

void CarShell::DetectNode()
{
    const ExecResult name = RunHere("uname -n");
    mNode = name.output.substr(0, name.output.find_first_of("\r\n"));
    mLog.Write(LogLevel::Info, mNode == kHostNode ? "connected straight to hu-omap: commands run here" : "connected to " + mNode + ": commands for hu-omap go through `on -f /net/hu-omap`");
}

ExecResult CarShell::Run(const std::string& script, bool logOutput)
{
    // sh -c on hu-omap, so redirects and paths are hu-omap's own, not the processor SSH landed on.
    return Send(mNode == kHostNode ? script : std::string("on -f /net/hu-omap /bin/sh -c ") + Quote(script), std::string(), logOutput);
}

ExecResult CarShell::RunHere(const std::string& script)
{
    return Send(script, std::string(), true);
}

ExecResult CarShell::Upload(const std::string& path, const std::string& data)
{
    mLog.Write(LogLevel::Info, std::format("  upload {} bytes to {}", data.size(), path));
    return Send(mNode == kHostNode ? "cat > " + Quote(path) : std::string("on -f /net/hu-omap /bin/sh -c ") + Quote("cat > " + Quote(path)), data, true);
}

std::string CarShell::FindTool(const std::string& name)
{
    const ExecResult found = Run(std::format("for d in {}; do [ -x $d/{} ] && echo $d/{} && break; done", kToolFolders, name, name));
    return found.output.substr(0, found.output.find_first_of("\r\n"));
}

std::string CarShell::Quote(const std::string& text)
{
    std::string quoted = "'";
    for (char c : text)
    {
        quoted += c == '\'' ? std::string("'\\''") : std::string(1, c);
    }
    return quoted + "'";
}

ExecResult CarShell::Send(const std::string& command, const std::string& input, bool logOutput)
{
    mLog.Write(LogLevel::Command, command);
    ExecResult result = mSession.Exec(command, input);
    if (!result.output.empty())
    {
        // A failed command's output is its error message: always worth showing, however it was asked to be logged.
        mLog.Write(LogLevel::Output, logOutput || result.exitCode != 0 ? result.output : std::format("({} lines, not shown)", std::count(result.output.begin(), result.output.end(), '\n')));
    }
    mLog.Write(LogLevel::Info, result.sent ? std::format("  exit {} ({} ms)", result.exitCode, result.milliseconds) : std::string("  not run"));
    return result;
}

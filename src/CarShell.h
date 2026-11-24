// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include "RunLog.h"
#include "SshSession.h"

#include <string>

// Runs shell scripts on hu-omap, the processor with /opt/aa and the plugin folder, whichever processor SSH landed on.
class CarShell
{
public:
    CarShell(SshSession& session, RunLog& log) : mSession(session), mLog(log)
    {
    }

    void DetectNode();
    const std::string& Node() const { return mNode; }

    ExecResult Run(const std::string& script, bool logOutput = true);
    ExecResult RunHere(const std::string& script);                       // on the processor SSH landed on
    ExecResult Upload(const std::string& path, const std::string& data);  // cat > path, fed through stdin
    std::string FindTool(const std::string& name);                         // full path, or empty

    static std::string Quote(const std::string& text);

private:
    ExecResult Send(const std::string& command, const std::string& input, bool logOutput);

    static constexpr const char* kHostNode = "hu-omap";

    SshSession& mSession;
    RunLog&     mLog;
    std::string mNode;           // the processor SSH landed on: commands for hu-omap go through `on -f` unless it is hu-omap
};

// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include "RunLog.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

struct FindResult
{
    bool        found = false;
    std::string host;  // the port follows from it (CarConnection::Port)
};

struct ProbeResult
{
    bool        open = false;
    std::string banner;  // the first line an SSH server sends; it also names the server software
};

// Looks for the head unit's SSH server without touching the car: the TurboCode proxy, BMW's HSFZ discovery broadcast,
// Windows' own neighbour table, then a port 22 knock on every address found.
class CarFinder
{
public:
    explicit CarFinder(RunLog& log);

    FindResult Find(const std::atomic<bool>& cancel);

private:
    struct Adapter
    {
        std::string name;
        std::string address;
    };

    std::vector<Adapter> LinkLocalAdapters();
    std::vector<std::string> AskWhoIsThere(const std::vector<Adapter>& adapters);
    void ListenForReplies(uintptr_t socket, std::vector<std::string>& responders);
    std::vector<std::string> Neighbours();
    static ProbeResult Probe(const std::string& address, uint16_t port);
    static bool IsLinkLocal(const std::string& address);
    void Step(const std::string& title);

    RunLog& mLog;
    uint8_t mStep = 0;
};

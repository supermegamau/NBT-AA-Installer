// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include "CarFinder.h"

#include "CarLogin.h"

#include <algorithm>
#include <chrono>
#include <format>

namespace
{
    constexpr uint8_t  kSteps = 5;
    constexpr uint16_t kHsfzPort = 6811;            // BMW HSFZ vehicle discovery, the same request ISTA, E-Sys and EdiabasLib send
    constexpr uint32_t kConnectTimeoutMs = 1000;    // link-local hosts answer in milliseconds; a silent one is not there
    constexpr uint32_t kBannerTimeoutMs = 2000;
    constexpr uint32_t kListenMs = 2000;
    constexpr uint8_t  kIdentificationRequest[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x11 };  // length 0, control word 0x0011

    // Head unit addresses seen in NBT-AAD's guides: cheap to try when nothing else turns them up.
    constexpr const char* kKnownAddresses[] =
    {
        "169.254.199.119",
        "169.254.199.99"
    };

    std::string Narrow(const wchar_t* text)
    {
        const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        std::string narrow(size > 0 ? static_cast<size_t>(size - 1) : 0u, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text, -1, narrow.data(), size, nullptr, nullptr);
        return narrow;
    }

    std::string ToText(const in_addr& address)
    {
        char text[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &address, text, sizeof(text));
        return text;
    }

    std::string Field(const std::string& reply, const std::string& name, size_t length)
    {
        const size_t at = reply.find(name);
        return at == std::string::npos ? std::string("?") : reply.substr(at + name.size(), length);
    }

    std::string Printable(std::string bytes)
    {
        std::replace_if(bytes.begin(), bytes.end(), [](char c) { return c < 0x20 || c > 0x7E; }, '.');
        return bytes;
    }

    void AddUnique(std::vector<std::string>& list, const std::string& value)
    {
        if (std::find(list.begin(), list.end(), value) == list.end())
        {
            list.push_back(value);
        }
    }
}

CarFinder::CarFinder(RunLog& log) : mLog(log)
{
}

FindResult CarFinder::Find(const std::atomic<bool>& cancel)
{
    FindResult result;
    mLog.Write(LogLevel::Info, "Find car: looks for the head unit's SSH server; nothing on the car is changed");

    Step(std::format("TurboCode proxy on this PC ({}:{})", kProxyHost, kProxyPort));
    const ProbeResult proxy = Probe(kProxyHost, kProxyPort);
    if (proxy.banner.starts_with("SSH-"))
    {
        mLog.Write(LogLevel::Pass, "the proxy is running and the car answers through it: " + proxy.banner);
        result = { true, kProxyHost };
    }
    else
    {
        mLog.Write(proxy.open ? LogLevel::Warning : LogLevel::Info, proxy.open ? "something listens on 6969 but no SSH server answered through it (is SSH unlocked?)" : "the proxy is not running");
    }

    Step("network adapters with a car address (169.254.x.x)");
    const std::vector<Adapter> adapters = LinkLocalAdapters();
    if (adapters.empty())
    {
        mLog.Write(LogLevel::Warning, "no adapter has a 169.254.x.x address: plug in the ENET cable and give Windows up to a minute to pick an automatic address");
    }

    Step(std::format("ask the car who is there (HSFZ vehicle identification, UDP broadcast to port {})", kHsfzPort));
    std::vector<std::string> candidates = AskWhoIsThere(adapters);

    Step("devices Windows has already seen on the car network (neighbour / ARP table)");
    for (const std::string& neighbour : Neighbours())
    {
        AddUnique(candidates, neighbour);
    }
    for (const char* known : kKnownAddresses)
    {
        AddUnique(candidates, known);
    }

    Step(std::format("look for an SSH server (port {}) on {} address(es)", kSshPort, candidates.size()));
    for (const std::string& candidate : candidates)
    {
        if (cancel)
        {
            mLog.Write(LogLevel::Warning, "cancelled");
            return result;
        }
        if (std::any_of(adapters.begin(), adapters.end(), [&candidate](const Adapter& adapter) { return adapter.address == candidate; }))
        {
            continue;  // this PC's own address
        }
        const ProbeResult ssh = Probe(candidate, kSshPort);
        mLog.Write(ssh.banner.starts_with("SSH-") ? LogLevel::Pass : LogLevel::Info, ssh.banner.starts_with("SSH-") ? candidate + " answers SSH: " + ssh.banner : candidate + (ssh.open ? ": port 22 open, but no SSH greeting" : ": no SSH on port 22"));
        if (ssh.banner.starts_with("SSH-") && (!result.found || result.host == kProxyHost))
        {
            result = { true, candidate };  // a direct address beats the proxy: it works without TurboCode running
        }
    }

    mLog.Write(result.found ? LogLevel::Pass : LogLevel::Error, result.found ? std::format("use host {} (filled in above). Log file: {}", result.host, mLog.FilePath().string()) : std::string("car not found: check the ENET cable, that SSH is unlocked (it closes again after a while), and the ignition is on"));
    return result;
}

std::vector<CarFinder::Adapter> CarFinder::LinkLocalAdapters()
{
    std::vector<Adapter> adapters;
    ULONG size = 16 * 1024;
    std::vector<uint8_t> buffer(size);
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()), &size) == ERROR_BUFFER_OVERFLOW)
    {
        buffer.resize(size);
        GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()), &size);
    }
    for (const IP_ADAPTER_ADDRESSES* adapter = reinterpret_cast<const IP_ADAPTER_ADDRESSES*>(buffer.data()); adapter; adapter = adapter->Next)
    {
        for (const IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress; unicast && adapter->OperStatus == IfOperStatusUp; unicast = unicast->Next)
        {
            const std::string address = ToText(reinterpret_cast<const sockaddr_in*>(unicast->Address.lpSockaddr)->sin_addr);
            if (IsLinkLocal(address))
            {
                adapters.push_back({ Narrow(adapter->FriendlyName), address });
                mLog.Write(LogLevel::Pass, std::format("{}: {}", adapters.back().name, address));
            }
        }
    }
    return adapters;
}

std::vector<std::string> CarFinder::AskWhoIsThere(const std::vector<Adapter>& adapters)
{
    std::vector<std::string> responders;
    for (const Adapter& adapter : adapters)
    {
        SOCKET udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udp == INVALID_SOCKET)
        {
            continue;
        }
        BOOL broadcast = TRUE;
        setsockopt(udp, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast), sizeof(broadcast));
        sockaddr_in local{};
        local.sin_family = AF_INET;
        inet_pton(AF_INET, adapter.address.c_str(), &local.sin_addr);
        sockaddr_in target{};
        target.sin_family = AF_INET;
        target.sin_port = htons(kHsfzPort);
        target.sin_addr.s_addr = INADDR_BROADCAST;
        // Bound to the adapter's own address so the broadcast leaves through the ENET cable, not Wi-Fi.
        if (bind(udp, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) == 0 && sendto(udp, reinterpret_cast<const char*>(kIdentificationRequest), sizeof(kIdentificationRequest), 0, reinterpret_cast<const sockaddr*>(&target), sizeof(target)) == sizeof(kIdentificationRequest))
        {
            mLog.Write(LogLevel::Command, std::format("UDP from {} to 255.255.255.255:{}: 00 00 00 00 00 11", adapter.address, kHsfzPort));
            ListenForReplies(udp, responders);
        }
        else
        {
            mLog.Write(LogLevel::Warning, std::format("could not send the broadcast from {} (error {})", adapter.address, WSAGetLastError()));
        }
        closesocket(udp);
    }
    if (responders.empty())
    {
        mLog.Write(LogLevel::Info, "nothing answered the broadcast (the gateway may want the ignition on)");
    }
    return responders;
}

void CarFinder::ListenForReplies(uintptr_t socket, std::vector<std::string>& responders)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kListenMs);
    for (auto now = std::chrono::steady_clock::now(); now < deadline; now = std::chrono::steady_clock::now())
    {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(static_cast<SOCKET>(socket), &readable);
        const int64_t remainingUs = std::chrono::duration_cast<std::chrono::microseconds>(deadline - now).count();
        timeval wait{ static_cast<long>(remainingUs / 1000000), static_cast<long>(remainingUs % 1000000) };
        if (select(0, &readable, nullptr, nullptr, &wait) != 1)
        {
            return;
        }
        char reply[1024];
        sockaddr_in sender{};
        int senderSize = sizeof(sender);
        const int got = recvfrom(static_cast<SOCKET>(socket), reply, sizeof(reply), 0, reinterpret_cast<sockaddr*>(&sender), &senderSize);
        if (got <= 0)
        {
            continue;
        }
        const std::string text(reply, static_cast<size_t>(got));
        // The answer is text after the 6-byte header: DIAGADRnnBMWMACxxxxxxxxxxxxBMWVINxxxxxxxxxxxxxxxxx
        mLog.Write(LogLevel::Pass, std::format("{} answered: VIN {}, MAC {}, diagnostic address {}", ToText(sender.sin_addr), Field(text, "BMWVIN", 17), Field(text, "BMWMAC", 12), Field(text, "DIAGADR", 2)));
        mLog.Write(LogLevel::Output, Printable(text));
        AddUnique(responders, ToText(sender.sin_addr));
    }
}

std::vector<std::string> CarFinder::Neighbours()
{
    std::vector<std::string> neighbours;
    PMIB_IPNET_TABLE2 table = nullptr;
    if (GetIpNetTable2(AF_INET, &table) != NO_ERROR)
    {
        mLog.Write(LogLevel::Warning, "could not read Windows' neighbour table");
        return neighbours;
    }
    for (ULONG i = 0; i < table->NumEntries; ++i)
    {
        const MIB_IPNET_ROW2& row = table->Table[i];
        const std::string address = ToText(row.Address.Ipv4.sin_addr);
        if (IsLinkLocal(address) && address != "169.254.255.255" && row.PhysicalAddressLength == 6 && row.State != NlnsUnreachable && row.State != NlnsIncomplete)
        {
            mLog.Write(LogLevel::Info, std::format("{} (MAC {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X})", address, row.PhysicalAddress[0], row.PhysicalAddress[1], row.PhysicalAddress[2], row.PhysicalAddress[3], row.PhysicalAddress[4], row.PhysicalAddress[5]));
            AddUnique(neighbours, address);
        }
    }
    FreeMibTable(table);
    if (neighbours.empty())
    {
        mLog.Write(LogLevel::Info, "no 169.254.x.x devices in the table yet");
    }
    return neighbours;
}

ProbeResult CarFinder::Probe(const std::string& address, uint16_t port)
{
    ProbeResult result;
    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    SOCKET tcp = inet_pton(AF_INET, address.c_str(), &target.sin_addr) == 1 ? socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) : INVALID_SOCKET;
    if (tcp == INVALID_SOCKET)
    {
        return result;
    }
    u_long nonBlocking = 1;
    ioctlsocket(tcp, FIONBIO, &nonBlocking);
    connect(tcp, reinterpret_cast<const sockaddr*>(&target), sizeof(target));
    fd_set writable;
    fd_set failed;  // Windows reports a refused connect here, not as writable
    FD_ZERO(&writable);
    FD_ZERO(&failed);
    FD_SET(tcp, &writable);
    FD_SET(tcp, &failed);
    timeval wait{ static_cast<long>(kConnectTimeoutMs / 1000), static_cast<long>(kConnectTimeoutMs % 1000 * 1000) };
    result.open = select(0, nullptr, &writable, &failed, &wait) == 1 && FD_ISSET(tcp, &writable);
    if (result.open)
    {
        nonBlocking = 0;
        ioctlsocket(tcp, FIONBIO, &nonBlocking);
        DWORD timeout = kBannerTimeoutMs;
        setsockopt(tcp, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        char banner[256] = {};
        const int got = recv(tcp, banner, sizeof(banner) - 1, 0);
        result.banner = got > 0 ? std::string(banner, static_cast<size_t>(got)) : std::string();
        result.banner = result.banner.substr(0, result.banner.find_first_of("\r\n"));
    }
    closesocket(tcp);
    return result;
}

bool CarFinder::IsLinkLocal(const std::string& address)
{
    return address.starts_with("169.254.");
}

void CarFinder::Step(const std::string& title)
{
    mLog.Write(LogLevel::Step, std::format("{}/{} {}", ++mStep, kSteps, title));
}

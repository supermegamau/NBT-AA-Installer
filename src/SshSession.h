// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include <cstdint>
#include <string>

struct _LIBSSH2_SESSION;  // libssh2's own name; kept out of this header so wx and winsock never meet

struct ExecResult
{
    bool        sent = false;   // false = the channel failed, so exitCode says nothing about the command
    int32_t     exitCode = -1;
    std::string output;         // stdout and stderr merged: order matters more than the stream for short commands
    uint32_t    milliseconds = 0;
};

// Winsock and libssh2 global state, alive for the whole program.
class SshLibrary
{
public:
    SshLibrary();
    ~SshLibrary();
    bool Ready() const { return mReady; }

private:
    bool mReady = false;
};

// One blocking SSH connection to the car.
class SshSession
{
public:
    SshSession() = default;
    ~SshSession();
    SshSession(const SshSession&) = delete;
    SshSession& operator=(const SshSession&) = delete;

    bool Connect(const std::string& host, uint16_t port, std::string& error);
    bool Login(const std::string& user, const std::string& password, std::string& error);
    std::string AuthMethods(const std::string& user);
    const std::string& AuthUsed() const { return mAuthUsed; }  // which login method worked, for the log
    std::string HostKeyType() const;
    std::string HostKeyFingerprint() const;
    std::string NegotiatedMethods() const;
    ExecResult Exec(const std::string& command, const std::string& input = std::string());  // input goes to the command's stdin, then EOF
    void Close();

private:
    bool OpenSocket(const std::string& host, uint16_t port, std::string& error);
    void OfferEveryMethod(int32_t methodType);
    std::string LastError() const;

    static constexpr uint16_t kConnectTimeoutSeconds = 10;
    static constexpr uint32_t kCommandTimeoutMs = 60000;     // pidin is slow, and a 1 MB upload over QNET takes seconds
    static constexpr uint32_t kMaxOutputBytes = 4u << 20;     // a runaway cat must not eat the PC's memory

    uintptr_t          mSocket = ~uintptr_t(0);   // winsock SOCKET without including winsock here
    _LIBSSH2_SESSION*  mSession = nullptr;
    std::string        mPassword;                  // keyboard-interactive asks for it from a C callback
    std::string        mAuthUsed;
};

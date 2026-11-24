// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include <winsock2.h>
#include <ws2tcpip.h>
#include <libssh2.h>

#include "SshSession.h"

#include <chrono>
#include <cstring>
#include <format>
#include <string>

namespace
{
    std::string Base64NoPadding(const unsigned char* data, size_t size)
    {
        static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string text;
        for (size_t i = 0; i < size; i += 3)
        {
            const uint32_t chunk = (uint32_t(data[i]) << 16) | (i + 1 < size ? uint32_t(data[i + 1]) << 8 : 0u) | (i + 2 < size ? uint32_t(data[i + 2]) : 0u);
            text += kAlphabet[(chunk >> 18) & 63];
            text += kAlphabet[(chunk >> 12) & 63];
            if (i + 1 < size)
            {
                text += kAlphabet[(chunk >> 6) & 63];
            }
            if (i + 2 < size)
            {
                text += kAlphabet[chunk & 63];
            }
        }
        return text;
    }

    // Answers every keyboard-interactive prompt with the password: old sshds offer only that, not "password".
    void AnswerPrompts(const char*, int, const char*, int, int promptCount, const LIBSSH2_USERAUTH_KBDINT_PROMPT*, LIBSSH2_USERAUTH_KBDINT_RESPONSE* responses, void** abstract)
    {
        const std::string& password = *static_cast<const std::string*>(*abstract);
        for (int i = 0; i < promptCount; ++i)
        {
            responses[i].text = _strdup(password.c_str());  // libssh2 frees it with free()
            responses[i].length = static_cast<unsigned int>(password.size());
        }
    }
}

SshLibrary::SshLibrary()
{
    WSADATA data{};
    mReady = WSAStartup(MAKEWORD(2, 2), &data) == 0 && libssh2_init(0) == 0;
}

SshLibrary::~SshLibrary()
{
    libssh2_exit();
    WSACleanup();
}

SshSession::~SshSession()
{
    Close();
}

bool SshSession::Connect(const std::string& host, uint16_t port, std::string& error)
{
    if (!OpenSocket(host, port, error))
    {
        return false;
    }
    mSession = libssh2_session_init_ex(nullptr, nullptr, nullptr, &mPassword);
    if (!mSession)
    {
        error = "libssh2 could not create a session";
        return false;
    }
    // The car's sshd is old: offer everything this build knows, legacy SHA-1 and DSA included.
    for (int32_t type : { LIBSSH2_METHOD_KEX, LIBSSH2_METHOD_HOSTKEY, LIBSSH2_METHOD_CRYPT_CS, LIBSSH2_METHOD_CRYPT_SC, LIBSSH2_METHOD_MAC_CS, LIBSSH2_METHOD_MAC_SC })
    {
        OfferEveryMethod(type);
    }
    libssh2_session_set_blocking(mSession, 1);
    libssh2_session_set_timeout(mSession, kCommandTimeoutMs);
    if (libssh2_session_handshake(mSession, static_cast<libssh2_socket_t>(mSocket)) != 0)
    {
        error = "SSH handshake failed: " + LastError();
        return false;
    }
    return true;
}

bool SshSession::OpenSocket(const std::string& host, uint16_t port, std::string& error)
{
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* found = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &found) != 0)
    {
        error = "cannot resolve " + host;
        return false;
    }
    error = "nothing answers on " + host + ":" + std::to_string(port) + " (is SSH unlocked and the proxy running?)";
    for (addrinfo* address = found; address && mSocket == ~uintptr_t(0); address = address->ai_next)
    {
        SOCKET candidate = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (candidate == INVALID_SOCKET)
        {
            continue;
        }
        // Non-blocking connect only to get a timeout: a blocking one waits ~20 s on a dead link-local address.
        u_long nonBlocking = 1;
        ioctlsocket(candidate, FIONBIO, &nonBlocking);
        connect(candidate, address->ai_addr, static_cast<int>(address->ai_addrlen));
        fd_set writable;
        fd_set failed;  // Windows reports a refused connect here, not as writable
        FD_ZERO(&writable);
        FD_ZERO(&failed);
        FD_SET(candidate, &writable);
        FD_SET(candidate, &failed);
        timeval wait{ kConnectTimeoutSeconds, 0 };
        int32_t socketError = 0;
        int socketErrorSize = sizeof(socketError);
        if (select(0, nullptr, &writable, &failed, &wait) == 1 && FD_ISSET(candidate, &writable) && getsockopt(candidate, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socketError), &socketErrorSize) == 0 && socketError == 0)
        {
            nonBlocking = 0;
            ioctlsocket(candidate, FIONBIO, &nonBlocking);
            mSocket = candidate;
        }
        else
        {
            closesocket(candidate);
        }
    }
    freeaddrinfo(found);
    return mSocket != ~uintptr_t(0);
}

void SshSession::OfferEveryMethod(int32_t methodType)
{
    const char** algorithms = nullptr;
    const int count = libssh2_session_supported_algs(mSession, methodType, &algorithms);
    if (count <= 0)
    {
        return;
    }
    std::string list;
    for (int i = 0; i < count; ++i)
    {
        list += (i ? "," : "") + std::string(algorithms[i]);
    }
    libssh2_free(mSession, algorithms);
    libssh2_session_method_pref(mSession, methodType, list.c_str());
}

std::string SshSession::AuthMethods(const std::string& user)
{
    const char* methods = libssh2_userauth_list(mSession, user.c_str(), static_cast<unsigned int>(user.size()));
    return methods ? methods : "(none listed)";
}

bool SshSession::Login(const std::string& user, const std::string& password, std::string& error)
{
    mPassword = password;
    const std::string methods = AuthMethods(user);
    mAuthUsed = "none";
    if (libssh2_userauth_authenticated(mSession))
    {
        return true;  // the server accepted "none": no password needed
    }
    mAuthUsed = "password";
    if (methods.find("password") != std::string::npos && libssh2_userauth_password(mSession, user.c_str(), password.c_str()) == 0)
    {
        return true;
    }
    mAuthUsed = "keyboard-interactive";
    if (methods.find("keyboard-interactive") != std::string::npos && libssh2_userauth_keyboard_interactive(mSession, user.c_str(), &AnswerPrompts) == 0)
    {
        return true;
    }
    mAuthUsed.clear();
    error = "login refused (server offers: " + methods + "): " + LastError();
    return false;
}

std::string SshSession::HostKeyType() const
{
    size_t size = 0;
    int type = LIBSSH2_HOSTKEY_TYPE_UNKNOWN;
    libssh2_session_hostkey(mSession, &size, &type);
    switch (type)
    {
    case LIBSSH2_HOSTKEY_TYPE_RSA:       return "ssh-rsa";
    case LIBSSH2_HOSTKEY_TYPE_DSS:       return "ssh-dss";
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_256: return "ecdsa-sha2-nistp256";
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_384: return "ecdsa-sha2-nistp384";
    case LIBSSH2_HOSTKEY_TYPE_ECDSA_521: return "ecdsa-sha2-nistp521";
    case LIBSSH2_HOSTKEY_TYPE_ED25519:   return "ssh-ed25519";
    default:                             return "unknown";
    }
}

std::string SshSession::HostKeyFingerprint() const
{
    const char* hash = libssh2_hostkey_hash(mSession, LIBSSH2_HOSTKEY_HASH_SHA256);
    return hash ? "SHA256:" + Base64NoPadding(reinterpret_cast<const unsigned char*>(hash), 32) : "unavailable";
}

std::string SshSession::NegotiatedMethods() const
{
    std::string text;
    for (const auto& [type, label] : { std::pair{ LIBSSH2_METHOD_KEX, "key exchange " }, std::pair{ LIBSSH2_METHOD_HOSTKEY, ", host key " }, std::pair{ LIBSSH2_METHOD_CRYPT_CS, ", cipher " }, std::pair{ LIBSSH2_METHOD_MAC_CS, ", mac " } })
    {
        const char* method = libssh2_session_methods(mSession, type);
        text += label + std::string(method ? method : "?");
    }
    return text;
}

ExecResult SshSession::Exec(const std::string& command, const std::string& input)
{
    ExecResult result;
    const auto start = std::chrono::steady_clock::now();
    LIBSSH2_CHANNEL* channel = libssh2_channel_open_session(mSession);
    if (!channel)
    {
        result.output = "cannot open a channel: " + LastError();
        return result;
    }
    libssh2_channel_handle_extended_data2(channel, LIBSSH2_CHANNEL_EXTENDED_DATA_MERGE);
    if (libssh2_channel_exec(channel, command.c_str()) == 0)
    {
        result.sent = true;
        for (size_t written = 0; written < input.size();)
        {
            const ssize_t sent = libssh2_channel_write(channel, input.data() + written, input.size() - written);
            if (sent <= 0)
            {
                result.output = "upload stopped after " + std::to_string(written) + " bytes: " + LastError();
                break;
            }
            written += static_cast<size_t>(sent);
        }
        libssh2_channel_send_eof(channel);  // cat > file ends here
        char buffer[4096];
        ssize_t got = libssh2_channel_read(channel, buffer, sizeof(buffer));
        for (; got > 0; got = libssh2_channel_read(channel, buffer, sizeof(buffer)))
        {
            if (result.output.size() < kMaxOutputBytes)
            {
                result.output.append(buffer, static_cast<size_t>(got));
            }
        }
        if (got < 0)
        {
            // No exit status ever came: reading one now would give a made-up 0 (seen with a command that never returns).
            result.output += std::format("{}(no answer within {} s: stopped waiting) {}", result.output.empty() || result.output.back() == '\n' ? "" : "\n", kCommandTimeoutMs / 1000, LastError());
            result.exitCode = -1;
        }
        else
        {
            libssh2_channel_close(channel);
            libssh2_channel_wait_closed(channel);
            result.exitCode = libssh2_channel_get_exit_status(channel);
        }
    }
    else
    {
        result.output = "the car refused the command: " + LastError();
    }
    libssh2_channel_free(channel);
    result.milliseconds = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
    return result;
}

std::string SshSession::LastError() const
{
    char* message = nullptr;
    if (!mSession)
    {
        return "no session";
    }
    libssh2_session_last_error(mSession, &message, nullptr, 0);
    return message ? message : "unknown error";
}

void SshSession::Close()
{
    if (mSession)
    {
        libssh2_session_disconnect(mSession, "NBT-AA-Installer done");
        libssh2_session_free(mSession);
        mSession = nullptr;
    }
    if (mSocket != ~uintptr_t(0))
    {
        closesocket(static_cast<SOCKET>(mSocket));
        mSocket = ~uintptr_t(0);
    }
}

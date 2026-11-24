// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#include <windows.h>
#include <winhttp.h>

#include "HttpClient.h"
#include "Version.h"

#include <format>
#include <memory>

namespace
{
    std::wstring Wide(const std::string& text)
    {
        const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        std::wstring wide(size > 0 ? static_cast<size_t>(size - 1) : 0u, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), size);
        return wide;
    }

    std::string Narrow(const std::wstring& text)
    {
        const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string narrow(size > 0 ? static_cast<size_t>(size - 1) : 0u, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, narrow.data(), size, nullptr, nullptr);
        return narrow;
    }

    // WinHTTP handles close with one call; this keeps every early return from leaking one.
    struct HandleCloser
    {
        void operator()(void* handle) const
        {
            WinHttpCloseHandle(handle);
        }
    };
    using Handle = std::unique_ptr<void, HandleCloser>;

    std::string SystemError(DWORD code)
    {
        wchar_t* text = nullptr;
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, GetModuleHandleW(L"winhttp.dll"), code, 0, reinterpret_cast<wchar_t*>(&text), 0, nullptr);
        const std::string message = text ? Narrow(text) : std::format("error {}", code);
        LocalFree(text);
        return message.substr(0, message.find_last_not_of("\r\n ") + 1);
    }
}

HttpResult HttpClient::Get(const std::string& url, const std::string& accept, const std::atomic<bool>& cancel)
{
    HttpResult result;
    const std::wstring wideUrl = Wide(url);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &parts))
    {
        result.error = "bad URL: " + url;
        return result;
    }
    const Handle session(WinHttpOpen(Wide(std::string("NBT-AA-Installer/") + kAppVersion).c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    const Handle connection(session ? WinHttpConnect(session.get(), std::wstring(parts.lpszHostName, parts.dwHostNameLength).c_str(), parts.nPort, 0) : nullptr);
    const Handle request(connection ? WinHttpOpenRequest(connection.get(), L"GET", (std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength) + std::wstring(parts.lpszExtraInfo, parts.dwExtraInfoLength)).c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr);
    if (!request)
    {
        result.error = "cannot start the request: " + SystemError(GetLastError());
        return result;
    }
    WinHttpSetTimeouts(request.get(), 10000, 10000, 15000, 30000);
    const std::wstring headers = Wide("Accept: " + accept + "\r\n");
    if (!WinHttpSendRequest(request.get(), headers.c_str(), static_cast<DWORD>(-1L), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request.get(), nullptr))
    {
        result.error = "no answer from " + Narrow(std::wstring(parts.lpszHostName, parts.dwHostNameLength)) + ": " + SystemError(GetLastError());
        return result;
    }
    DWORD status = 0;
    DWORD size = sizeof(status);
    WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    result.status = status;
    wchar_t remaining[32] = {};
    DWORD remainingSize = sizeof(remaining);
    if (WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_CUSTOM, L"X-RateLimit-Remaining", remaining, &remainingSize, WINHTTP_NO_HEADER_INDEX))
    {
        result.rateLimitLeft = Narrow(remaining);
    }
    for (DWORD available = 0; WinHttpQueryDataAvailable(request.get(), &available) && available > 0;)
    {
        if (cancel || result.body.size() + available > kMaxBytes)
        {
            result.error = cancel ? "cancelled" : "the download is far bigger than expected";
            return result;
        }
        const size_t start = result.body.size();
        result.body.resize(start + available);
        DWORD read = 0;
        WinHttpReadData(request.get(), result.body.data() + start, available, &read);
        result.body.resize(start + read);
    }
    result.ok = status >= 200 && status < 300;
    result.error = result.ok ? std::string() : std::format("HTTP {}", status);
    return result;
}

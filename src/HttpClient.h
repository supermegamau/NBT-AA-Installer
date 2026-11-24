// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 supermegamau - NBT-AA-Installer
#pragma once

#include <atomic>
#include <cstdint>
#include <string>

struct HttpResult
{
    bool        ok = false;     // transport worked and the status was 2xx
    uint32_t    status = 0;
    std::string body;
    std::string error;
    std::string rateLimitLeft;  // GitHub's X-RateLimit-Remaining: 60 requests an hour without a login
};

// HTTPS GET through Windows' own WinHTTP: system proxy settings and certificates, no extra library.
class HttpClient
{
public:
    static HttpResult Get(const std::string& url, const std::string& accept, const std::atomic<bool>& cancel);

private:
    static constexpr uint64_t kMaxBytes = 64ull << 20;  // a release asset is about 1 MB; anything this big is wrong
};

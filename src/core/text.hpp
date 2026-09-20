#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
#include <string_view>

namespace hd::text {

[[nodiscard]] inline std::string Ident(std::string_view s) {
    std::string r;
    r.reserve(s.size() + 1);
    for (char c : s) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_';
        r.push_back(ok ? c : '_');
    }
    if (!r.empty() && r[0] >= '0' && r[0] <= '9') r.insert(r.begin(), '_');
    if (r.empty()) r = "_unnamed";
    return r;
}

[[nodiscard]] inline std::string Json(std::string_view s) {
    std::string r;
    r.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '"':  r += "\\\""; break;
        case '\\': r += "\\\\"; break;
        case '\n': r += "\\n";  break;
        case '\r': r += "\\r";  break;
        case '\t': r += "\\t";  break;
        default:
            if (c >= 0x20) {
                r.push_back(c);
            } else {
                char b[8];
                std::snprintf(b, sizeof(b), "\\u%04x", c);
                r += b;
            }
        }
    }
    return r;
}

[[nodiscard]] inline std::string Timestamp() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    char b[64];
    std::snprintf(b, sizeof(b), "%04d-%02d-%02d %02d:%02d:%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return b;
}

template <class... TArgs>
[[nodiscard]] inline std::string Format(const char* fmt, TArgs... args) {
    char buf[256];
    const int n = std::snprintf(buf, sizeof(buf), fmt, args...);
    if (n <= 0) return {};
    const auto len = static_cast<std::size_t>(n);
    return std::string(buf, len < sizeof(buf) ? len : sizeof(buf) - 1);
}

[[nodiscard]] inline std::string HexAddress(std::uint64_t v) {
    return Format("0x%llX", static_cast<unsigned long long>(v));
}

}

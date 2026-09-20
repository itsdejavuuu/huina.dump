#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include "core/mem.hpp"
#include "core/seh.hpp"
#include "core/types.hpp"

namespace hd::sig {

inline constexpr std::size_t kMaxBytes = 48;

struct Compiled {
    std::array<std::uint8_t, kMaxBytes> bytes{};
    std::array<std::uint8_t, kMaxBytes> mask{};
    std::uint8_t len = 0;

    [[nodiscard]] constexpr std::size_t size() const noexcept { return len; }
    [[nodiscard]] constexpr const std::uint8_t* bytesData() const noexcept { return bytes.data(); }
    [[nodiscard]] constexpr const std::uint8_t* maskData() const noexcept { return mask.data(); }
    [[nodiscard]] constexpr bool matches(const std::uint8_t* p) const noexcept {
        for (std::size_t i = 0; i < len; ++i) {
            if (mask[i] && p[i] != bytes[i]) return false;
        }
        return true;
    }
};

[[nodiscard]] constexpr int HexNibble(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

[[nodiscard]] constexpr Compiled Compile(std::string_view ida) noexcept {
    Compiled c{};
    std::size_t i = 0;
    while (i < ida.size()) {
        const char ch = ida[i];
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            ++i;
            continue;
        }
        if (c.len >= kMaxBytes) return Compiled{};
        if (ch == '?') {
            ++i;
            if (i < ida.size() && ida[i] == '?') ++i;
            c.bytes[c.len] = 0;
            c.mask[c.len] = 0;
            ++c.len;
            continue;
        }
        if (i + 1 >= ida.size()) return Compiled{};
        const int hi = HexNibble(ida[i]);
        const int lo = HexNibble(ida[i + 1]);
        if (hi < 0 || lo < 0) return Compiled{};
        c.bytes[c.len] = static_cast<std::uint8_t>((hi << 4) | lo);
        c.mask[c.len] = 1;
        ++c.len;
        i += 2;
    }
    return c;
}

[[nodiscard]] constexpr bool IsUsable(const Compiled& c) noexcept { return c.len > 0; }

[[nodiscard]] constexpr bool FitsRip(const Compiled& c, std::size_t dispOff,
                                     std::size_t ripOff) noexcept {
    return IsUsable(c) && dispOff + 4 <= c.size() && ripOff <= c.size();
}

[[nodiscard]] constexpr bool FitsImm(const Compiled& c, std::size_t immOff) noexcept {
    return IsUsable(c) && immOff + 4 <= c.size();
}

namespace detail {

[[nodiscard]] inline uintptr_t FindInRange(const std::uint8_t* data, std::size_t size,
                                           const Compiled& c) noexcept {
    const std::size_t n = c.size();
    if (n == 0 || n > size) return 0;

    std::size_t anchor = 0;
    while (anchor < n && !c.maskData()[anchor]) ++anchor;
    if (anchor >= n) return reinterpret_cast<uintptr_t>(data);

    const std::size_t last = size - n;
    const std::uint8_t a = c.bytesData()[anchor];
    std::size_t start = 0;
    while (start <= last) {
        const void* found = std::memchr(data + start + anchor, a, last - start + 1);
        if (!found) return 0;
        start = static_cast<std::size_t>(static_cast<const std::uint8_t*>(found) - data) - anchor;
        if (c.matches(data + start)) return reinterpret_cast<uintptr_t>(data) + start;
        ++start;
    }
    return 0;
}

[[nodiscard]] inline bool SehFindInSpan(types::Va start, std::size_t size, const Compiled& c,
                                        types::Va& matchOut) noexcept {
    matchOut = types::Va{};
    __try {
        const uintptr_t m = FindInRange(reinterpret_cast<const std::uint8_t*>(start.raw()), size, c);
        matchOut = types::Va{ m };
        return m != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        matchOut = types::Va{};
        return false;
    }
}

}

[[nodiscard]] bool Find(const mem::Module& mod, const Compiled& c, types::Va& matchOut) noexcept;

[[nodiscard]] bool Rip(const mem::Module& mod, const Compiled& c,
                       std::size_t dispOff, std::size_t ripOff, types::Va& out) noexcept;

[[nodiscard]] bool Imm32(const mem::Module& mod, const Compiled& c,
                         std::size_t immOff, std::uint32_t& out) noexcept;

}

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <windows.h>
#include "core/types.hpp"

namespace hd::seh {

template <class T>
    requires std::is_trivially_copyable_v<T>
[[nodiscard]] inline bool Peek(const void* p, T& out) noexcept {
    __try {
        out = *static_cast<const T*>(p);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out = T{};
        return false;
    }
}

template <class T>
    requires std::is_trivially_copyable_v<T>
[[nodiscard]] inline bool Peek(types::Va addr, T& out) noexcept {
    return Peek(reinterpret_cast<const void*>(addr.raw()), out);
}

using Uptr = uintptr_t;

[[nodiscard]] inline bool Bytes(const void* src, void* dst, std::size_t n) noexcept {
    __try {
        const std::uint8_t* s = static_cast<const std::uint8_t*>(src);
        std::uint8_t* d = static_cast<std::uint8_t*>(dst);
        for (std::size_t i = 0; i < n; ++i) d[i] = s[i];
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

[[nodiscard]] inline bool StrLen(const char* p, std::size_t maxLen, std::size_t& lenOut) noexcept {
    lenOut = 0;
    if (!p) return false;
    __try {
        std::size_t i = 0;
        for (; i < maxLen; ++i) {
            if (p[i] == '\0') {
                lenOut = i;
                return i > 0;
            }
        }
        return false;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        lenOut = 0;
        return false;
    }
}

template <class TReturn, class... TArgs>
[[nodiscard]] inline TReturn Guard(TReturn (*fn)(TArgs...), std::type_identity_t<TArgs>... args) noexcept {
    __try {
        return fn(args...);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return TReturn{};
    }
}

}

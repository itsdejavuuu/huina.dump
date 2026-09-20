#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace hd::types {

template <class TValue, class TTag>
class Strong {
public:
    using Value = TValue;
    using Tag = TTag;

    constexpr Strong() noexcept = default;
    constexpr explicit Strong(TValue raw) noexcept : m_raw(raw) {}

    [[nodiscard]] constexpr TValue raw() const noexcept { return m_raw; }
    constexpr void set(TValue raw) noexcept { m_raw = raw; }

    [[nodiscard]] constexpr bool valid() const noexcept { return m_raw != TValue{}; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }

    friend constexpr bool operator==(Strong a, Strong b) noexcept { return a.m_raw == b.m_raw; }
    friend constexpr bool operator!=(Strong a, Strong b) noexcept { return a.m_raw != b.m_raw; }
    friend constexpr bool operator< (Strong a, Strong b) noexcept { return a.m_raw < b.m_raw; }
    friend constexpr bool operator> (Strong a, Strong b) noexcept { return a.m_raw > b.m_raw; }
    friend constexpr bool operator<=(Strong a, Strong b) noexcept { return a.m_raw <= b.m_raw; }
    friend constexpr bool operator>=(Strong a, Strong b) noexcept { return a.m_raw >= b.m_raw; }

private:
    TValue m_raw{};
};

struct RvaTag; struct VaTag; struct PropOffsetTag; struct StrideTag; struct ElementsTag;
struct ClassIdTag; struct LaneTag; struct PropFlagsTag;

using Rva = Strong<uintptr_t, RvaTag>;
using Va = Strong<uintptr_t, VaTag>;
using PropOffset = Strong<int32_t, PropOffsetTag>;
using Stride = Strong<int32_t, StrideTag>;
using Elements = Strong<int32_t, ElementsTag>;
using ClassId = Strong<int32_t, ClassIdTag>;
using Lane = Strong<int32_t, LaneTag>;
using PropFlags = Strong<int32_t, PropFlagsTag>;

[[nodiscard]] constexpr Va Advance(Va a, std::ptrdiff_t n) noexcept {
    return Va(static_cast<uintptr_t>(static_cast<intptr_t>(a.raw()) + n));
}
[[nodiscard]] constexpr Va operator+(Va a, PropOffset d) noexcept {
    return Advance(a, static_cast<intptr_t>(d.raw()));
}
[[nodiscard]] constexpr Va operator-(Va a, PropOffset d) noexcept {
    return Advance(a, -static_cast<intptr_t>(d.raw()));
}
[[nodiscard]] constexpr Va operator+(Va a, Rva r) noexcept { return Va(a.raw() + r.raw()); }
[[nodiscard]] constexpr Va operator-(Va a, Rva r) noexcept { return Va(a.raw() - r.raw()); }
[[nodiscard]] constexpr Rva operator-(Va a, Va b) noexcept { return Rva(a.raw() - b.raw()); }

[[nodiscard]] constexpr PropOffset operator+(PropOffset a, PropOffset b) noexcept {
    return PropOffset(static_cast<int32_t>(a.raw() + b.raw()));
}
[[nodiscard]] constexpr PropOffset operator-(PropOffset a, PropOffset b) noexcept {
    return PropOffset(static_cast<int32_t>(a.raw() - b.raw()));
}
[[nodiscard]] constexpr PropOffset operator*(Stride s, Elements n) noexcept {
    return PropOffset(static_cast<int32_t>(s.raw() * n.raw()));
}
template <class TInt>
    requires std::is_integral_v<TInt>
[[nodiscard]] constexpr Elements ElementsOf(TInt n) noexcept {
    return Elements{ static_cast<int32_t>(n) };
}

[[nodiscard]] constexpr bool InRange(Va value, Va base, std::size_t size) noexcept {
    return value.raw() >= base.raw() && value.raw() < base.raw() + size;
}

inline constexpr int32_t kMaxClassId = 10000;
[[nodiscard]] constexpr bool IsPlausibleClassId(int32_t v) noexcept {
    return v >= 0 && v <= kMaxClassId;
}

inline constexpr Lane kDefaultLane{ 0x28 };

inline constexpr uintptr_t kMinCodeAddress = 0x10000;

}

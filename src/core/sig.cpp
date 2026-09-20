#include "core/sig.hpp"

namespace hd::sig {

bool Find(const mem::Module& mod, const Compiled& c, types::Va& matchOut) noexcept {
    matchOut = types::Va{};
    if (!IsUsable(c)) return false;
    for (const mem::ExecSpan& span : mod.exec()) {
        types::Va m{};
        if (detail::SehFindInSpan(span.start, span.size, c, m)) {
            matchOut = m;
            return true;
        }
    }
    return false;
}

bool Rip(const mem::Module& mod, const Compiled& c,
         std::size_t dispOff, std::size_t ripOff, types::Va& out) noexcept {
    out = types::Va{};
    if (!IsUsable(c) || dispOff + 4 > c.size() || ripOff > c.size()) return false;

    for (const mem::ExecSpan& span : mod.exec()) {
        types::Va match{};
        if (!detail::SehFindInSpan(span.start, span.size, c, match)) continue;

        int32_t disp = 0;
        if (!seh::Peek(types::Advance(match, static_cast<std::ptrdiff_t>(dispOff)), disp)) continue;

        const types::Va abs = types::Advance(match, static_cast<std::ptrdiff_t>(ripOff) + disp);
        if (!mod.contains(abs)) continue;
        out = abs;
        return true;
    }
    return false;
}

bool Imm32(const mem::Module& mod, const Compiled& c,
           std::size_t immOff, std::uint32_t& out) noexcept {
    out = 0;
    if (!IsUsable(c) || immOff + 4 > c.size()) return false;

    for (const mem::ExecSpan& span : mod.exec()) {
        types::Va match{};
        if (!detail::SehFindInSpan(span.start, span.size, c, match)) continue;

        std::uint32_t v = 0;
        if (!seh::Peek(types::Advance(match, static_cast<std::ptrdiff_t>(immOff)), v)) continue;
        out = v;
        return true;
    }
    return false;
}

}

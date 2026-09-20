#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "core/seh.hpp"
#include "core/types.hpp"

namespace hd::mem {

[[nodiscard]] inline bool ReadCStr(const char* p, std::string& out, std::size_t maxLen) {
    out.clear();
    if (!p) return false;
    std::size_t len = 0;
    if (!seh::StrLen(p, maxLen, len)) return false;
    out.resize(len);
    if (!seh::Bytes(p, out.data(), len)) {
        out.clear();
        return false;
    }
    return true;
}

[[nodiscard]] inline bool IsPrintableAscii(const char* p, std::size_t maxLen = 128) {
    std::size_t len = 0;
    if (!seh::StrLen(p, maxLen, len)) return false;
    char buf[128];
    if (!seh::Bytes(p, buf, len)) return false;
    for (std::size_t i = 0; i < len; ++i) {
        if (buf[i] < 0x20 || buf[i] > 0x7E) return false;
    }
    return true;
}

struct ExecSpan {
    types::Va start{};
    std::size_t size = 0;
};

class Module {
public:
    [[nodiscard]] static std::optional<Module> Acquire(std::string_view name) noexcept;

    [[nodiscard]] std::string_view name() const noexcept { return m_name; }
    [[nodiscard]] types::Va base() const noexcept { return m_base; }
    [[nodiscard]] std::size_t imageSize() const noexcept { return m_imageSize; }
    [[nodiscard]] std::span<const ExecSpan> exec() const noexcept { return m_exec; }

    [[nodiscard]] bool contains(types::Va addr) const noexcept {
        return types::InRange(addr, m_base, m_imageSize);
    }
    [[nodiscard]] types::Rva asRva(types::Va addr) const noexcept { return addr - m_base; }

private:
    std::string m_name;
    types::Va m_base{};
    std::size_t m_imageSize = 0;
    std::vector<ExecSpan> m_exec;
};

}

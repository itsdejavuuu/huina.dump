#include "core/mem.hpp"

#include <array>
#include <cstring>
#include <windows.h>
#include <psapi.h>

namespace hd::mem {
namespace {

inline constexpr std::size_t kMaxSections = 64;

[[nodiscard]] bool SehPeekExecSections(uintptr_t base,
                                       std::array<ExecSpan, kMaxSections>& out,
                                       std::size_t& count) noexcept {
    count = 0;
    __try {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
        const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
            if ((sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) continue;
            if (count >= kMaxSections) break;
            const std::size_t size = sec->Misc.VirtualSize ? sec->Misc.VirtualSize
                                                           : sec->SizeOfRawData;
            if (!size) continue;
            out[count].start = types::Va{ base + sec->VirtualAddress };
            out[count].size = size;
            ++count;
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        count = 0;
        return false;
    }
}

}

std::optional<Module> Module::Acquire(std::string_view name) noexcept {
    if (name.empty() || name.size() >= MAX_PATH) return std::nullopt;

    char narrow[MAX_PATH]{};
    std::memcpy(narrow, name.data(), name.size());

    const HMODULE h = GetModuleHandleA(narrow);
    if (!h) return std::nullopt;

    MODULEINFO mi{};
    if (!K32GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi))) return std::nullopt;

    Module m;
    m.m_name.assign(name);
    m.m_base = types::Va{ reinterpret_cast<uintptr_t>(mi.lpBaseOfDll) };
    m.m_imageSize = static_cast<std::size_t>(mi.SizeOfImage);

    std::array<ExecSpan, kMaxSections> raw{};
    std::size_t count = 0;
    if (SehPeekExecSections(m.m_base.raw(), raw, count) && count > 0) {
        m.m_exec.assign(raw.begin(), raw.begin() + static_cast<std::ptrdiff_t>(count));
    }
    if (m.m_exec.empty()) {
        m.m_exec.push_back(ExecSpan{ m.m_base, m.m_imageSize });
    }
    return m;
}

}

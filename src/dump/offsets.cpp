#include "dump/offsets.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "core/mem.hpp"
#include "core/sig.hpp"

namespace hd::dump {
namespace {

enum class TargetModule { Client, Engine };
enum class TargetKind { Rip, Imm32 };

struct OffsetTarget {
    std::string_view name;
    TargetModule module;
    TargetKind kind;
    std::string_view ida;
    std::uint8_t firstOff;
    std::uint8_t secondOff;
};

inline constexpr std::array kOffsetTargets{
    OffsetTarget{ "entity_list",  TargetModule::Client, TargetKind::Rip, "48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 45 33 C0 C7 05", 3, 7 },
    OffsetTarget{ "local_player", TargetModule::Client, TargetKind::Rip, "48 8B 05 ?? ?? ?? ?? 48 8B D9 48 3B C1", 3, 7 },
    OffsetTarget{ "render",       TargetModule::Engine, TargetKind::Rip, "48 8B 0D ?? ?? ?? ?? 48 8B 01 FF 50 ?? 48 8B C8 E8 ?? ?? ?? ?? 8B D0", 3, 7 },
    OffsetTarget{ "client_state", TargetModule::Engine, TargetKind::Rip, "48 8B 0D ?? ?? ?? ?? 48 85 C9 74 ?? 48 8B 01 FF 50", 3, 7 },
    OffsetTarget{ "global_vars",  TargetModule::Engine, TargetKind::Rip, "48 8D 0D ?? ?? ?? ?? EB 02", 3, 7 },
    OffsetTarget{ "view_setup",   TargetModule::Engine, TargetKind::Rip, "48 8D 05 ?? ?? ?? ?? 48 89 05", 3, 7 },
    OffsetTarget{ "view_angles",  TargetModule::Engine, TargetKind::Rip, "48 8D 15 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? 0F 28 C7", 3, 7 },
    OffsetTarget{ "force_jump",   TargetModule::Client, TargetKind::Rip, "8B 0D ?? ?? ?? ?? F6 C1 03 74 03 83 CB 02", 2, 6 },
    OffsetTarget{ "bone_matrix",  TargetModule::Client, TargetKind::Imm32, "48 8D 04 40 48 C1 E0 04 48 03 87 ?? ?? ?? ??", 11, 0 },
    OffsetTarget{ "studio_hdr",   TargetModule::Client, TargetKind::Imm32, "48 89 ?? E8 1A 00 00", 3, 0 },
};

struct CompiledTarget {
    const OffsetTarget* spec = nullptr;
    sig::Compiled pattern{};
};

[[nodiscard]] constexpr std::array<CompiledTarget, kOffsetTargets.size()> CompileTargets() noexcept {
    std::array<CompiledTarget, kOffsetTargets.size()> out{};
    for (std::size_t i = 0; i < kOffsetTargets.size(); ++i) {
        out[i].spec = &kOffsetTargets[i];
        out[i].pattern = sig::Compile(kOffsetTargets[i].ida);
    }
    return out;
}

inline constexpr std::array kCompiledTargets = CompileTargets();

[[nodiscard]] constexpr bool TargetsAreValid() noexcept {
    for (const CompiledTarget& ct : kCompiledTargets) {
        const OffsetTarget& t = *ct.spec;
        if (t.name.empty() || t.ida.empty()) return false;
        if (t.kind == TargetKind::Rip) {
            if (!sig::FitsRip(ct.pattern, t.firstOff, t.secondOff)) return false;
        } else {
            if (t.secondOff != 0) return false;
            if (!sig::FitsImm(ct.pattern, t.firstOff)) return false;
        }
    }
    return true;
}
static_assert(TargetsAreValid(), "huina_dump: broken signature/offset in kOffsetTargets");

[[nodiscard]] constexpr std::string_view ModuleName(TargetModule m) noexcept {
    return (m == TargetModule::Client) ? std::string_view{ "client.dll" }
                                       : std::string_view{ "engine.dll" };
}

[[nodiscard]] constexpr const mem::Module& Pick(TargetModule m, const mem::Module& client,
                                                const mem::Module& engine) noexcept {
    return (m == TargetModule::Client) ? client : engine;
}

}

bool ResolveOffsets(const mem::Module& client, const mem::Module& engine,
                    std::vector<ResolvedOffset>& out, std::string& err) {
    out.clear();

    for (const CompiledTarget& ct : kCompiledTargets) {
        const OffsetTarget& t = *ct.spec;
        const mem::Module& mod = Pick(t.module, client, engine);
        const std::string module{ ModuleName(t.module) };

        if (t.kind == TargetKind::Rip) {
            types::Va absolute{};
            if (sig::Rip(mod, ct.pattern, t.firstOff, t.secondOff, absolute)) {
                ResolvedOffset o;
                o.name.assign(t.name);
                o.module = module;
                o.absolute = absolute;
                o.rva = mod.asRva(absolute);
                o.isImm = false;
                out.push_back(std::move(o));
            }
            continue;
        }

        std::uint32_t imm = 0;
        if (sig::Imm32(mod, ct.pattern, t.firstOff, imm)) {
            ResolvedOffset o;
            o.name.assign(t.name);
            o.module = module;
            o.absolute = types::Va{};
            o.rva = types::Rva{ static_cast<uintptr_t>(imm) };
            o.isImm = true;
            o.imm = imm;
            out.push_back(std::move(o));
        }
    }

    if (out.empty()) {
        err = "No pointer offsets resolved";
        return false;
    }
    return true;
}

}

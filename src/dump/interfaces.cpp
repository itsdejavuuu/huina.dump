#include "dump/interfaces.hpp"

#include <array>
#include <bitset>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <utility>
#include <vector>
#include <windows.h>
#include <psapi.h>
#include "core/seh.hpp"
#include "core/mem.hpp"
#include "dump/limits.hpp"

namespace hd::dump {
namespace {

using CreateInterfaceFn = void* (*)(const char*, int*);
using GetAllClassesFn = sdk::ClientClass* (*)(void*);

inline constexpr std::array kLaneCandidates{
    types::kDefaultLane,
    types::Lane{ 0x2C },
    types::Lane{ 0x30 },
    types::Lane{ 0x34 },
};

struct InterfaceTarget {
    std::string_view module;
    std::string_view version;
};

inline constexpr std::array kTargets{
    InterfaceTarget{ "client.dll", "VClient017" },
    InterfaceTarget{ "client.dll", "VClient018" },
    InterfaceTarget{ "client.dll", "VClient016" },
    InterfaceTarget{ "client.dll", "VClientEntityList003" },
    InterfaceTarget{ "client.dll", "VClientEntityList004" },
    InterfaceTarget{ "client.dll", "GameMovement001" },
    InterfaceTarget{ "client.dll", "VClientPrediction001" },
    InterfaceTarget{ "client.dll", "VClientTools001" },
    InterfaceTarget{ "client.dll", "Input004" },
    InterfaceTarget{ "client.dll", "Input005" },
    InterfaceTarget{ "engine.dll", "VEngineClient013" },
    InterfaceTarget{ "engine.dll", "VEngineClient014" },
    InterfaceTarget{ "engine.dll", "VEngineClient015" },
    InterfaceTarget{ "engine.dll", "VEngineClient016" },
    InterfaceTarget{ "engine.dll", "VModelInfoClient006" },
    InterfaceTarget{ "engine.dll", "VModelInfoClient007" },
    InterfaceTarget{ "engine.dll", "VDebugOverlay004" },
    InterfaceTarget{ "engine.dll", "VEngineEffects001" },
    InterfaceTarget{ "engine.dll", "StaticPropMgr002" },
    InterfaceTarget{ "engine.dll", "EngineTraceClient003" },
    InterfaceTarget{ "engine.dll", "EngineTraceClient004" },
    InterfaceTarget{ "engine.dll", "VEngineRenderView013" },
    InterfaceTarget{ "engine.dll", "VEngineRenderView014" },
    InterfaceTarget{ "engine.dll", "VEngineModel015" },
    InterfaceTarget{ "engine.dll", "VEngineModel016" },
    InterfaceTarget{ "vstdlib.dll", "VEngineCvar004" },
    InterfaceTarget{ "vstdlib.dll", "VEngineCvar007" },
    InterfaceTarget{ "lua_shared.dll", "LUASHARED_INTERFACE_VERSION001" },
    InterfaceTarget{ "materialsystem.dll", "VMaterialSystem080" },
    InterfaceTarget{ "materialsystem.dll", "VMaterialSystem081" },
    InterfaceTarget{ "vguimatsurface.dll", "VGUI_Surface031" },
    InterfaceTarget{ "vguimatsurface.dll", "VGUI_Surface032" },
    InterfaceTarget{ "vgui2.dll", "VGUI_Panel009" },
    InterfaceTarget{ "vgui2.dll", "VGUI_System010" },
    InterfaceTarget{ "vphysics.dll", "VPhysicsSurfaceProps001" },
    InterfaceTarget{ "vphysics.dll", "VPhysicsCollision007" },
    InterfaceTarget{ "datacache.dll", "MDLCache004" },
    InterfaceTarget{ "datacache.dll", "StudioDataCache005" },
    InterfaceTarget{ "engine.dll", "VModelInfoClient004" },
    InterfaceTarget{ "engine.dll", "GAMEEVENTSMANAGER002" },
    InterfaceTarget{ "lua_shared.dll", "LUASHARED003" },
    InterfaceTarget{ "inputsystem.dll", "InputSystemVersion001" },
    InterfaceTarget{ "engine.dll", "VEngineClient015" },
    InterfaceTarget{ "server.dll", "ServerGameDLL009" },
    InterfaceTarget{ "server.dll", "ServerGameDLL010" },
    InterfaceTarget{ "server.dll", "ServerTools001" },
    InterfaceTarget{ "server.dll", "GameMovement001" },
};

[[nodiscard]] bool ValidateChain(sdk::ClientClass* head) noexcept {
    for (const types::Lane lane : kLaneCandidates) {
        int count = 0;
        sdk::ClientClass* cur = head;
        bool ok = true;
        while (cur && count < kValidateNodes) {
            sdk::ClientClass node{};
            if (!seh::Bytes(static_cast<const void*>(cur), &node, sizeof(node))) {
                ok = false;
                break;
            }
            if (!mem::IsPrintableAscii(node.m_pNetworkName, kMaxStringLen)) {
                ok = false;
                break;
            }
            if (!node.m_pRecvTable) {
                ok = false;
                break;
            }
            if (sdk::ReadClassId(node, lane).raw() < 0) {
                ok = false;
                break;
            }
            cur = node.m_pNext;
            ++count;
        }
        if (ok && count >= kMinValidateNodes) return true;
    }
    return false;
}

}

types::Va GrabInterface(std::string_view module, std::string_view version) noexcept {
    if (module.empty() || version.empty() || module.size() >= MAX_PATH) return types::Va{};

    char modBuf[MAX_PATH]{};
    std::memcpy(modBuf, module.data(), module.size());

    char verBuf[kMaxStringLen]{};
    const std::size_t verLen = version.size() < kMaxStringLen - 1 ? version.size() : kMaxStringLen - 1;
    std::memcpy(verBuf, version.data(), verLen);

    const HMODULE h = GetModuleHandleA(modBuf);
    if (!h) return types::Va{};

    void* proc = reinterpret_cast<void*>(GetProcAddress(h, "CreateInterface"));
    if (!proc) return types::Va{};

    void* instance = seh::Guard(reinterpret_cast<CreateInterfaceFn>(proc),
                                static_cast<const char*>(verBuf), static_cast<int*>(nullptr));
    return types::Va{ reinterpret_cast<uintptr_t>(instance) };
}

std::vector<InterfaceHandle> EnumerateInterfaces() {
    std::vector<InterfaceHandle> out;
    out.reserve(kTargets.size());
    for (const InterfaceTarget& t : kTargets) {
        const types::Va instance = GrabInterface(t.module, t.version);
        if (!instance.valid()) continue;

        types::Va vtable{};
        (void)seh::Peek(instance, vtable);

        InterfaceHandle h;
        h.module.assign(t.module);
        h.version.assign(t.version);
        h.instance = instance;
        h.vtable = vtable;
        out.push_back(std::move(h));
    }
    return out;
}

namespace {

struct IfaceRegNode {
    void* createFn = nullptr;
    const char* name = nullptr;
    IfaceRegNode* next = nullptr;
};

[[nodiscard]] std::string LowerName(std::string_view v) {
    std::string s(v);
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

[[nodiscard]] std::vector<std::string> ProcessDllNames() {
    std::vector<std::string> out;
    HMODULE mods[1024]{};
    DWORD needed = 0;
    if (!K32EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed)) return out;
    const std::size_t n = needed / sizeof(HMODULE);
    char base[MAX_PATH]{};
    for (std::size_t i = 0; i < n; ++i) {
        base[0] = '\0';
        if (!K32GetModuleBaseNameA(GetCurrentProcess(), mods[i], base, MAX_PATH)) continue;
        if (!base[0]) continue;
        out.push_back(LowerName(base));
    }
    return out;
}

[[nodiscard]] IfaceRegNode* RegHeadFromCreateInterface(HMODULE h) {
    void* proc = reinterpret_cast<void*>(GetProcAddress(h, "CreateInterface"));
    if (!proc) return nullptr;
    MODULEINFO mi{};
    if (!K32GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi))) return nullptr;
    const uintptr_t base = reinterpret_cast<uintptr_t>(mi.lpBaseOfDll);
    const uintptr_t size = static_cast<uintptr_t>(mi.SizeOfImage);

    for (int off = 0; off < 512; ++off) {
        std::uint8_t b0 = 0, b1 = 0, b2 = 0;
        const void* p = reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(proc) + off);
        if (!seh::Bytes(p, &b0, 1)) break;
        if (!seh::Bytes(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(proc) + off + 1), &b1, 1)) break;
        if (!seh::Bytes(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(proc) + off + 2), &b2, 1)) break;
        if (b0 != 0x48 || (b1 != 0x8B && b1 != 0x8D)) continue;
        if ((b2 & 0xC7) != 0x05) continue;
        std::int32_t disp = 0;
        if (!seh::Bytes(reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(proc) + off + 3), &disp, 4)) continue;
        const uintptr_t slot = reinterpret_cast<uintptr_t>(proc) + off + 7 + static_cast<intptr_t>(disp);
        if (slot < base || slot + 8 > base + size) continue;
        IfaceRegNode* head = nullptr;
        if (!seh::Peek(reinterpret_cast<const void*>(slot), head) || !head) continue;
        IfaceRegNode n0{}, n1{};
        if (!seh::Bytes(head, &n0, sizeof(n0)) || !n0.name) continue;
        if (!mem::IsPrintableAscii(n0.name, kMaxStringLen)) continue;
        if (n0.next && seh::Bytes(n0.next, &n1, sizeof(n1)) && n1.name &&
            !mem::IsPrintableAscii(n1.name, kMaxStringLen))
            continue;
        return head;
    }
    return nullptr;
}

[[nodiscard]] std::vector<std::string> RegNames(const char* module) {
    std::vector<std::string> out;
    const HMODULE h = GetModuleHandleA(module);
    if (!h) return out;
    IfaceRegNode* head = nullptr;
    if (void* pRegs = reinterpret_cast<void*>(GetProcAddress(h, "s_pInterfaceRegs")))
        (void)seh::Peek(pRegs, head);
    if (!head) head = RegHeadFromCreateInterface(h);
    if (!head) return out;
    IfaceRegNode* cur = head;
    for (int guard = 0; guard < 5000 && cur; ++guard) {
        IfaceRegNode node{};
        if (!seh::Bytes(static_cast<const void*>(cur), &node, sizeof(node))) break;
        std::string name;
        if (node.name && mem::ReadCStr(node.name, name, kMaxStringLen) && !name.empty())
            out.push_back(std::move(name));
        cur = node.next;
    }
    return out;
}

}

bool IsCheatInterface(std::string_view module, std::string_view version) noexcept {
    const std::string m = LowerName(module);
    char vb[128]{};
    const std::size_t n = version.size() < sizeof(vb) - 1 ? version.size() : sizeof(vb) - 1;
    for (std::size_t i = 0; i < n; ++i)
        vb[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(version[i])));
    const std::string_view v(vb, n);

    auto starts = [&](std::string_view p) { return v.size() >= p.size() && v.substr(0, p.size()) == p; };

    if (m == "client.dll")
        return starts("vclient") || starts("vcliententitylist") || starts("gamemovement") ||
               starts("vclientprediction") || starts("vclienttools") || starts("input");
    if (m == "engine.dll")
        return starts("vengineclient") || starts("vmodelinfoclient") || starts("vdebugoverlay") ||
               starts("vengineeffects") || starts("staticpropmgr") || starts("enginetraceclient") ||
               starts("venginerenderview") || starts("venginemodel") || starts("gameeventsmanager") ||
               starts("vsoundemittersystem") || starts("veffectssound");
    if (m == "vstdlib.dll") return starts("venginecvar");
    if (m == "lua_shared.dll") return starts("luashared");
    if (m == "materialsystem.dll") return starts("vmaterialsystem");
    if (m == "vguimatsurface.dll") return starts("vgui_surface");
    if (m == "vgui2.dll") return starts("vgui_panel") || starts("vgui_system");
    if (m == "inputsystem.dll") return starts("inputsystem");
    if (m == "datacache.dll") return starts("mdlcache") || starts("studiodatacache");
    if (m == "vphysics.dll") return starts("vphysicssurfaceprops") || starts("vphysicscollision");
    if (m == "studiorender.dll") return starts("vstudioRender") || starts("vstudiorender");
    if (m == "server.dll")
        return starts("servergamedll") || starts("servertools") || starts("gamemovement");
    return false;
}

std::vector<InterfaceHandle> EnumerateAllInterfaces() {
    std::vector<InterfaceHandle> out = EnumerateInterfaces();
    auto seen = [&](std::string_view m, std::string_view v) {
        for (const auto& h : out)
            if (h.module == m && h.version == v) return true;
        return false;
    };

    for (const std::string& mod : ProcessDllNames()) {
        if (mod.empty() || mod == "huina_dump.dll") continue;
        char modBuf[MAX_PATH]{};
        std::memcpy(modBuf, mod.data(), mod.size() > MAX_PATH - 1 ? MAX_PATH - 1 : mod.size());
        for (const std::string& name : RegNames(modBuf)) {
            if (seen(mod, name)) continue;
            const types::Va instance = GrabInterface(mod, name);
            if (!instance.valid()) continue;
            types::Va vtable{};
            (void)seh::Peek(instance, vtable);
            InterfaceHandle h;
            h.module = mod;
            h.version = name;
            h.instance = instance;
            h.vtable = vtable;
            out.push_back(std::move(h));
        }
    }
    return out;
}

sdk::ClientClass* FindClassListHead(void* clientInstance) noexcept {
    if (!clientInstance) return nullptr;

    seh::Uptr vtable = 0;
    if (!seh::Peek(static_cast<const void*>(clientInstance), vtable) || !vtable) return nullptr;

    for (int pass = 0; pass < 2; ++pass) {
        for (int idx = 0; idx < kHeadScanSlots; ++idx) {
            if (pass == 0 && idx != kGetAllClassesSlot) continue;

            seh::Uptr slot = 0;
            const void* slotAddr = reinterpret_cast<const void*>(
                vtable + static_cast<uintptr_t>(idx) * sizeof(void*));
            if (!seh::Peek(slotAddr, slot)) break;
            if (slot < types::kMinCodeAddress) continue;

            sdk::ClientClass* head =
                seh::Guard(reinterpret_cast<GetAllClassesFn>(slot), clientInstance);
            if (head && ValidateChain(head)) return head;
        }
    }
    return nullptr;
}

types::Lane DetectClassIdLane(sdk::ClientClass* head, bool& reliableOut) noexcept {
    reliableOut = false;
    if (!head) return types::kDefaultLane;

    types::Lane best = types::kDefaultLane;
    std::size_t bestDistinct = 0;

    for (const types::Lane lane : kLaneCandidates) {
        std::bitset<static_cast<std::size_t>(types::kMaxClassId) + 1> seen;
        sdk::ClientClass* cur = head;
        for (int n = 0; n < kLaneProbeNodes && cur; ++n) {
            sdk::ClientClass node{};
            if (!seh::Bytes(static_cast<const void*>(cur), &node, sizeof(node))) break;
            const int32_t v = sdk::ReadLane(node, lane);
            if (types::IsPlausibleClassId(v)) seen.set(static_cast<std::size_t>(v));
            if (!node.m_pNext) break;
            cur = node.m_pNext;
        }

        const std::size_t distinct = seen.count();
        if (distinct > bestDistinct) {
            bestDistinct = distinct;
            best = lane;
        }
    }

    reliableOut = bestDistinct >= kMinDistinctForReliable;
    if (!reliableOut) return types::kDefaultLane;
    return best;
}

bool HasAssignedClassIds(sdk::ClientClass* head, types::Lane lane) noexcept {
    if (lane.raw() < 0 || lane.raw() > kMaxLaneProbe) lane = types::kDefaultLane;
    sdk::ClientClass* cur = head;
    for (int n = 0; n < kLaneProbeNodes && cur; ++n) {
        sdk::ClientClass node{};
        if (!seh::Bytes(static_cast<const void*>(cur), &node, sizeof(node))) break;
        const int32_t id = sdk::ReadLane(node, lane);
        if (id >= 1 && id <= types::kMaxClassId) return true;
        if (!node.m_pNext) break;
        cur = node.m_pNext;
    }
    return false;
}

}
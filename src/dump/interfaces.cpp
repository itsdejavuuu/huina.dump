#include "dump/interfaces.hpp"

#include <array>
#include <bitset>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <utility>
#include <windows.h>
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
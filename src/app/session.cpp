#include "app/session.hpp"
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <windows.h>
#include "core/log.hpp"
#include "core/seh.hpp"
#include "core/text.hpp"
#include "dump/limits.hpp"
#include "emit/report.hpp"

namespace hd::app {
namespace {

inline constexpr int kModuleWaitAttempts = 300;
inline constexpr DWORD kModuleWaitDelayMs = 100;

inline constexpr std::array kVClientVersions{
    std::string_view{ "VClient018" },
    std::string_view{ "VClient017" },
    std::string_view{ "VClient016" },
};

class DumpSession {
public:
    explicit DumpSession(HMODULE self) noexcept : m_self(self) {}

    [[nodiscard]] ExitCode Run() {
        (void)m_self;

        if (!PrepareDir()) {
            log::Sink::Line("dir");
            MessageBoxA(nullptr, "Cannot create C:\\huina.dump.", "huina_dump", MB_OK | MB_ICONERROR);
            return ExitCode::DumpDir;
        }
        log::Sink::Open(m_dir + "\\dump.log");

        if (!LoadModules()) return ExitCode::NoModules;
        if (!ResolveSignatures()) return ExitCode::Signatures;
        if (!CollectInterfacesAndClasses()) return ExitCode::NoVClient;
        if (!m_head) {
            log::Sink::Line("classes");
            MessageBoxA(nullptr, "GetAllClasses failed.", "huina_dump", MB_OK | MB_ICONERROR);
            return ExitCode::NoClassList;
        }
        if (!GateOnMenu()) return ExitCode::MenuGate;
        if (!WalkNetvars()) return ExitCode::NoNetvars;
        DumpPerClass();
        if (!WriteReport()) return ExitCode::Write;

        log::Sink::Line("done");
        MessageBoxA(nullptr, ("Done: " + m_dir).c_str(),
                    "huina_dump", MB_OK | MB_ICONINFORMATION);
        return ExitCode::Ok;
    }

private:
    [[nodiscard]] bool PrepareDir();
    [[nodiscard]] bool LoadModules();
    [[nodiscard]] bool ResolveSignatures();
    [[nodiscard]] bool CollectInterfacesAndClasses();
    [[nodiscard]] bool GateOnMenu();
    [[nodiscard]] bool WalkNetvars();
    void DumpPerClass();
    [[nodiscard]] bool WriteReport();

    HMODULE m_self = nullptr;
    std::string m_dir;
    mem::Module m_client;
    mem::Module m_engine;
    std::vector<dump::ResolvedOffset> m_offsets;
    std::vector<dump::InterfaceHandle> m_interfaces;
    sdk::ClientClass* m_head = nullptr;
    types::Lane m_lane = types::kDefaultLane;
    bool m_laneReliable = false;
    std::vector<dump::NetvarEntry> m_entries;
    std::vector<dump::ClassInfo> m_classes;
    std::vector<dump::ClassDump> m_classDumps;
    dump::WalkStats m_stats;
};

bool DumpSession::PrepareDir() {
    static constexpr const char* kDir = "C:\\huina.dump";
    static constexpr const char* kClasses = "C:\\huina.dump\\Classes";
    CreateDirectoryA(kDir, nullptr);
    CreateDirectoryA(kClasses, nullptr);
    const DWORD attr = GetFileAttributesA(kDir);
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) return false;
    m_dir = kDir;
    return true;
}

bool DumpSession::LoadModules() {
    for (int attempt = 0; attempt < kModuleWaitAttempts; ++attempt) {
        std::optional<mem::Module> client = mem::Module::Acquire("client.dll");
        std::optional<mem::Module> engine = mem::Module::Acquire("engine.dll");
        if (client && engine) {
            m_client = std::move(*client);
            m_engine = std::move(*engine);
            return true;
        }
        Sleep(kModuleWaitDelayMs);
    }

    log::Sink::Line("modules");
    MessageBoxA(nullptr, "client.dll / engine.dll not found.",
                "huina_dump", MB_OK | MB_ICONERROR);
    return false;
}

bool DumpSession::ResolveSignatures() {
    std::string err;
    if (!dump::ResolveOffsets(m_client, m_engine, m_offsets, err)) {
        log::Sink::Line("sig");
        MessageBoxA(nullptr, "Pattern scanning failed.", "huina_dump", MB_OK | MB_ICONERROR);
        return false;
    }
    log::Sink::Line(text::Format("offsets %llu",
                                 static_cast<unsigned long long>(m_offsets.size())));
    return true;
}

bool DumpSession::CollectInterfacesAndClasses() {
    m_interfaces = dump::EnumerateInterfaces();

    types::Va vclient{};
    for (const std::string_view version : kVClientVersions) {
        vclient = dump::GrabInterface("client.dll", version);
        if (vclient.valid()) break;
    }
    if (!vclient.valid()) {
        log::Sink::Line("vclient");
        MessageBoxA(nullptr, "VClient not found.", "huina_dump", MB_OK | MB_ICONERROR);
        return false;
    }

    m_head = dump::FindClassListHead(reinterpret_cast<void*>(vclient.raw()));
    if (!m_head) return false;

    m_lane = dump::DetectClassIdLane(m_head, m_laneReliable);
    log::Sink::Line(text::Format("ifaces %llu",
                                 static_cast<unsigned long long>(m_interfaces.size())));
    return true;
}

bool DumpSession::GateOnMenu() {
    if (dump::HasAssignedClassIds(m_head, m_lane)) return true;

    log::Sink::Line("menu");
    MessageBoxA(nullptr, "Join any map and reinject dll.",
                "huina_dump", MB_OK | MB_ICONWARNING);
    return false;
}

bool DumpSession::WalkNetvars() {
    if (!dump::WalkAll(m_head, m_lane, m_entries, m_classes, m_stats)) {
        log::Sink::Line("netvars");
        MessageBoxA(nullptr, "Netvar walk failed.", "huina_dump", MB_OK | MB_ICONERROR);
        return false;
    }

    log::Sink::Line(text::Format("netvars %llu classes %llu",
                                 static_cast<unsigned long long>(m_stats.entries),
                                 static_cast<unsigned long long>(m_stats.classes)));
    return true;
}

void DumpSession::DumpPerClass() {
    m_classDumps.reserve(m_classes.size());

    sdk::ClientClass* cur = m_head;
    std::size_t guard = 0;
    while (cur && guard < dump::kClassChainLimit) {
        sdk::ClientClass node{};
        if (!seh::Bytes(static_cast<const void*>(cur), &node, sizeof(node))) break;

        std::string className;
        if (mem::ReadCStr(node.m_pNetworkName, className, dump::kMaxStringLen) && node.m_pRecvTable) {
            dump::ClassDump cd;
            cd.name = std::move(className);
            cd.id = sdk::ReadClassId(node, m_lane).raw();

            uintptr_t tableName = 0;
            if (seh::Peek(static_cast<const void*>(&node.m_pRecvTable->m_pNetTableName), tableName)) {
                (void)mem::ReadCStr(reinterpret_cast<const char*>(tableName), cd.table, dump::kMaxStringLen);
            }
            (void)dump::WalkClass(node.m_pRecvTable, cd.props);
            m_classDumps.push_back(std::move(cd));
        }
        cur = node.m_pNext;
        ++guard;
    }
}

bool DumpSession::WriteReport() {
    const std::array<mem::Module, 2> modules{ m_client, m_engine };
    const emit::Report report{
        m_dir,
        modules,
        m_offsets,
        m_entries,
        m_classes,
        m_classDumps,
        m_interfaces,
        m_lane,
        m_laneReliable,
    };

    std::string err;
    if (!emit::WriteAll(report, m_stats, err)) {
        log::Sink::Line("write");
        MessageBoxA(nullptr, "Write failed.", "huina_dump", MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}

}

DWORD ToDword(ExitCode code) noexcept {
    return static_cast<DWORD>(code);
}

namespace {

[[nodiscard]] DWORD RunImpl(HMODULE self) noexcept {
    DumpSession session(self);
    return ToDword(session.Run());
}
}

DWORD RunGuarded(HMODULE self) noexcept {
    __try {
        return RunImpl(self);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        log::Sink::Line("seh");
        MessageBoxA(nullptr, "Unhandled error.",
                    "huina_dump", MB_OK | MB_ICONERROR);
        return ToDword(ExitCode::Unhandled);
    }
}

}

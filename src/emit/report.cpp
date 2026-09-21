#include "emit/report.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>
#include "core/mem.hpp"
#include "core/seh.hpp"
#include "core/text.hpp"
#include "dump/interfaces.hpp"
#include "sdk/recv.hpp"

namespace hd::emit {
namespace {

std::string Hex32(std::uint32_t v) {
    return text::Format("0x%X", v);
}

bool WriteOffsetsHpp(const Report& r, const TableIndex& index, dump::WalkStats& stats) {
    std::ofstream f(r.dir + "\\offsets.hpp", std::ios::trunc);
    if (!f) return false;

    f << "#pragma once\n#include <cstdint>\n\n";
    f << "namespace Offsets {\n";
    for (const dump::ResolvedOffset& o : r.offsets) {
        f << "\tinline constexpr uintptr_t " << text::Ident(o.name)
          << " = 0x" << std::hex << std::uppercase << o.rva.raw() << std::dec << ";\n";
    }
    f << "}\n\nnamespace NetVars {\n";

    std::set<std::string> usedNamespaces;

    for (const auto& [table, props] : index.tables()) {
        const std::string ns = text::Ident(table);
        if (!usedNamespaces.insert(ns).second) ++stats.namespaceCollisions;

        f << "\tnamespace " << ns << " {\n";

        std::set<std::string> done;
        for (const dump::NetvarEntry* p : props) {
            if (p->elements > 1 && p->prop.find('[') == std::string::npos && !done.count(p->prop)) {
                done.insert(p->prop);
                std::string prefix = p->prop + "[";
                std::vector<const dump::NetvarEntry*> group;
                for (const dump::NetvarEntry* q : props) {
                    if (q->prop.rfind(prefix, 0) == 0) group.push_back(q);
                }
                if (group.size() > 1) {
                    f << "\t\tinline constexpr uintptr_t " << text::Ident(p->prop)
                      << "[" << group.size() << "] = {";
                    for (const dump::NetvarEntry* g : group) {
                        f << " 0x" << std::hex << std::uppercase
                          << static_cast<std::uint32_t>(g->offset.raw()) << std::dec << ",";
                    }
                    f << " };\n";
                }
            }
        }

        for (const dump::NetvarEntry* p : props) {
            std::string n = p->prop;
            for (char& c : n) {
                if (c == '[') c = '_';
                else if (c == ']') c = '_';
            }
            while (!n.empty() && n.back() == '_') n.pop_back();
            f << "\t\tinline constexpr uintptr_t " << text::Ident(n)
              << " = 0x" << std::hex << std::uppercase
              << static_cast<std::uint32_t>(p->offset.raw()) << std::dec << ";\n";
        }
        f << "\t}\n";
    }
    f << "}\n";
    return true;
}

bool WriteOffsetsJson(const Report& r) {
    std::ofstream f(r.dir + "\\offsets.json", std::ios::trunc);
    if (!f) return false;

    f << "{\n  \"timestamp\": \"" << text::Timestamp() << "\",\n";
    f << "  \"modules\": [\n";
    for (std::size_t i = 0; i < r.modules.size(); ++i) {
        f << "    {\"name\": \"" << text::Json(r.modules[i].name())
          << "\", \"base\": \"" << text::HexAddress(r.modules[i].base().raw())
          << "\", \"size\": " << r.modules[i].imageSize() << "}"
          << (i + 1 < r.modules.size() ? "," : "") << "\n";
    }
    f << "  ],\n  \"offsets\": [\n";
    for (std::size_t i = 0; i < r.offsets.size(); ++i) {
        const dump::ResolvedOffset& o = r.offsets[i];
        f << "    {\"name\": \"" << text::Json(o.name) << "\", \"module\": \""
          << text::Json(o.module) << "\", \"rva\": \"" << text::HexAddress(o.rva.raw()) << "\""
          << (o.isImm ? ", \"imm\": true" : "") << "}"
          << (i + 1 < r.offsets.size() ? "," : "") << "\n";
    }
    f << "  ]\n}\n";
    return true;
}

bool WriteNetvarsTxt(const Report& r) {
    std::ofstream f(r.dir + "\\netvars.txt", std::ios::trunc);
    if (!f) return false;
    for (const dump::NetvarEntry& e : r.netvars) {
        f << "[" << e.table << "] " << e.prop << ": "
          << Hex32(static_cast<std::uint32_t>(e.offset.raw()))
          << " (" << sdk::PropTypeLabel(e.type) << ")\n";
    }
    return true;
}

bool WriteNetvarsJson(const Report& r) {
    std::ofstream f(r.dir + "\\netvars.json", std::ios::trunc);
    if (!f) return false;
    f << "[\n";
    for (std::size_t i = 0; i < r.netvars.size(); ++i) {
        const dump::NetvarEntry& e = r.netvars[i];
        f << "  {\"table\": \"" << text::Json(e.table) << "\", \"prop\": \""
          << text::Json(e.prop) << "\", \"offset\": \""
          << Hex32(static_cast<std::uint32_t>(e.offset.raw())) << "\", \"type\": \""
          << sdk::PropTypeName(e.type) << "\", \"typeId\": " << e.type
          << ", \"elements\": " << e.elements
          << ", \"stride\": " << e.stride << ", \"flags\": " << e.flags << "}"
          << (i + 1 < r.netvars.size() ? "," : "") << "\n";
    }
    f << "]\n";
    return true;
}

bool WriteClassFiles(const Report& r, const TableIndex& index, dump::WalkStats& stats) {
    std::set<std::string> usedNames;
    std::size_t collisions = 0;
    const auto claim = [&](const std::string& fname) {
        if (!usedNames.insert(fname).second) ++collisions;
    };

    std::size_t count = 0;
    for (const auto& [table, props] : index.tables()) {
        const std::string fn = text::Ident(table) + ".txt";
        claim(fn);
        std::ofstream f(r.dir + "\\Classes\\" + fn, std::ios::trunc);
        if (!f) continue;
        for (const dump::NetvarEntry* p : props) {
            f << "[" << p->table << "] " << p->prop << ": "
              << Hex32(static_cast<std::uint32_t>(p->offset.raw()))
              << " (" << sdk::PropTypeLabel(p->type) << ")\n";
        }
        ++count;
    }
    stats.tableFiles = count;

    std::size_t classCount = 0;
    std::size_t classRows = 0;
    for (const dump::ClassDump& cd : r.classDumps) {
        const std::string fn = text::Ident(cd.name) + ".txt";
        claim(fn);
        std::ofstream f(r.dir + "\\Classes\\" + fn, std::ios::trunc);
        if (!f) continue;
        for (const dump::NetvarEntry& p : cd.props) {
            f << "[" << p.table << "] " << p.prop << ": "
              << Hex32(static_cast<std::uint32_t>(p.offset.raw()))
              << " (" << sdk::PropTypeLabel(p.type) << ")\n";
            ++classRows;
        }
        ++classCount;
    }
    stats.classFiles = classCount;
    stats.classRows = classRows;
    stats.fileNameCollisions = collisions;
    return true;
}

namespace {

struct ModBase {
    uintptr_t base = 0;
    std::size_t size = 0;
    bool known = false;
};

[[nodiscard]] const ModBase& CachedModBase(const std::string& module) {
    static std::map<std::string, ModBase> cache;
    auto it = cache.find(module);
    if (it != cache.end()) return it->second;
    ModBase mb;
    if (auto m = mem::Module::Acquire(module)) {
        mb.base = m->base().raw();
        mb.size = m->imageSize();
        mb.known = true;
    }
    return cache.emplace(module, mb).first->second;
}

inline constexpr int kVtableSlots = 128;

[[nodiscard]] bool IsExecAddr(uintptr_t addr) noexcept {
    if (addr < 0x10000) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    constexpr DWORD kExec = PAGE_EXECUTE | PAGE_EXECUTE_READ |
                            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (mbi.Protect & kExec) != 0;
}

struct DumpedVtable {
    const dump::InterfaceHandle* h = nullptr;
    std::vector<uintptr_t> fns;
    int garbage = 0;
};

[[nodiscard]] DumpedVtable DumpVtable(const dump::InterfaceHandle& h) {
    DumpedVtable d;
    d.h = &h;
    if (!h.vtable.valid()) {
        d.garbage = 1;
        return d;
    }
    int consec = 0;
    for (int s = 0; s < kVtableSlots; ++s) {
        uintptr_t fn = 0;
        const void* slot = reinterpret_cast<const void*>(
            h.vtable.raw() + static_cast<uintptr_t>(s) * sizeof(void*));
        const bool ok = seh::Peek(slot, fn) && fn && IsExecAddr(fn);
        d.fns.push_back(ok ? fn : 0);
        if (!ok) {
            ++d.garbage;
            if (++consec >= 2) break;
        } else {
            consec = 0;
        }
    }
    return d;
}

[[nodiscard]] bool KeepVtable(const DumpedVtable& d) noexcept {
    if (d.fns.empty()) return false;
    return d.garbage * 2 <= static_cast<int>(d.fns.size());
}

[[nodiscard]] std::vector<DumpedVtable> SortedKeptVtables(const Report& r, dump::WalkStats* stats = nullptr) {
    std::vector<DumpedVtable> out;
    out.reserve(r.interfaces.size());
    for (const auto& h : r.interfaces) {
        DumpedVtable d = DumpVtable(h);
        if (!KeepVtable(d)) {
            if (stats) ++stats->vtableSkipped;
            continue;
        }
        out.push_back(std::move(d));
    }
    std::sort(out.begin(), out.end(), [](const DumpedVtable& a, const DumpedVtable& b) {
        if (a.h->module != b.h->module) return a.h->module < b.h->module;
        return a.h->version < b.h->version;
    });
    return out;
}

}

bool WriteVtablesTxt(const Report& r, dump::WalkStats& stats) {
    std::ofstream f(r.dir + "\\interfaces_vtables.txt", std::ios::trunc);
    if (!f) return false;

    const std::vector<DumpedVtable> kept = SortedKeptVtables(r, &stats);
    f << "vtables (" << kept.size() << " kept, " << stats.vtableSkipped
      << " skipped, cap " << kVtableSlots << ")\n";
    f << "index: absolute (module+RVA), ??? = garbage/end\n";
    for (const DumpedVtable& d : kept) {
        const dump::InterfaceHandle* i = d.h;
        f << "\n[" << i->module << "] " << i->version
          << " vtable=" << text::HexAddress(i->vtable.raw()) << "\n";
        const ModBase& mb = CachedModBase(i->module);
        ++stats.vtableIfaces;
        for (std::size_t s = 0; s < d.fns.size(); ++s) {
            const uintptr_t fn = d.fns[s];
            if (!fn) {
                f << "  [" << s << "] ???\n";
                continue;
            }
            ++stats.vtableSlots;
            if (mb.known && fn >= mb.base && fn < mb.base + mb.size) {
                f << "  [" << s << "] " << text::HexAddress(fn) << " (" << i->module
                  << "+0x" << std::hex << std::uppercase << (fn - mb.base) << std::dec << ")\n";
            } else {
                f << "  [" << s << "] " << text::HexAddress(fn) << "\n";
            }
        }
    }
    return true;
}

bool WriteInterfacesTxt(const Report& r, dump::WalkStats& stats) {
    std::ofstream f(r.dir + "\\interfaces.txt", std::ios::trunc);
    if (!f) return false;

    const std::vector<DumpedVtable> kept = SortedKeptVtables(r, nullptr);
    (void)stats;
    f << "interfaces (" << kept.size() << " kept, " << stats.vtableSkipped << " skipped)\n";
    std::string lastModule;
    for (const DumpedVtable& d : kept) {
        const dump::InterfaceHandle* i = d.h;
        if (i->module != lastModule) {
            f << "\n[" << i->module << "]\n";
            lastModule = i->module;
        }
        f << i->version << " = " << text::HexAddress(i->instance.raw())
          << " (vtable " << text::HexAddress(i->vtable.raw())
          << ", slots " << d.fns.size() << ")\n";
    }
    f << "\nclient classes (" << r.classes.size() << ")\n";
    for (const dump::ClassInfo& c : r.classes) {
        f << c.name << " (id " << c.id << ") -> " << c.table << "\n";
    }
    return true;
}

bool WriteInterfacesJson(const Report& r, dump::WalkStats& stats) {
    std::ofstream f(r.dir + "\\interfaces.json", std::ios::trunc);
    if (!f) return false;
    const std::vector<DumpedVtable> kept = SortedKeptVtables(r, nullptr);
    (void)stats;
    f << "{\n  \"timestamp\": \"" << text::Timestamp() << "\",\n  \"interfaces\": [\n";
    for (std::size_t i = 0; i < kept.size(); ++i) {
        const dump::InterfaceHandle& h = *kept[i].h;
        f << "    {\"module\": \"" << text::Json(h.module) << "\", \"version\": \""
          << text::Json(h.version) << "\", \"instance\": \"" << text::HexAddress(h.instance.raw())
          << "\", \"vtable\": \"" << text::HexAddress(h.vtable.raw())
          << "\", \"slots\": " << kept[i].fns.size() << "}"
          << (i + 1 < kept.size() ? "," : "") << "\n";
    }
    f << "  ]\n}\n";
    return true;
}

namespace {

void PrintHierarchyProps(std::ofstream& f, const std::vector<dump::HierarchyProp>& props,
                         int depth, std::size_t& counter) {
    std::string pad(static_cast<std::size_t>(depth) * 2, ' ');
    for (const dump::HierarchyProp& p : props) {
        ++counter;
        f << pad << p.name << " +"
          << text::Format("0x%X", static_cast<unsigned>(p.offset))
          << " (" << sdk::PropTypeLabel(p.type) << ", flags " << p.flags << ")";
        if (!p.table.empty()) f << " -> " << p.table;
        f << "\n";
        if (!p.children.empty()) PrintHierarchyProps(f, p.children, depth + 1, counter);
    }
}

}

bool WriteRecvHierarchy(const Report& r, dump::WalkStats& stats) {
    std::ofstream f(r.dir + "\\recv_hierarchy.txt", std::ios::trunc);
    if (!f) return false;
    f << "recv hierarchy (" << r.hierarchies.size() << " classes)\n";
    f << "class (id) -> root table, props with total offsets\n";
    std::size_t props = 0;
    for (const dump::ClassHierarchy& ch : r.hierarchies) {
        f << "\n" << ch.className << " (id " << ch.id << ") -> " << ch.rootTable << "\n";
        PrintHierarchyProps(f, ch.props, 1, props);
    }
    stats.hierarchies = r.hierarchies.size();
    stats.hierarchyProps = props;
    return true;
}

bool WriteInfoTxt(const Report& r, const dump::WalkStats& stats) {
    std::ofstream f(r.dir + "\\dump_info.txt", std::ios::trunc);
    if (!f) return false;

    f << "huina_dump\ntimestamp: " << text::Timestamp() << "\n";
    for (const mem::Module& m : r.modules) {
        f << "module: " << m.name() << " base=" << text::HexAddress(m.base().raw())
          << " size=" << m.imageSize() << "\n";
    }
    f << "classIdLane: "
      << text::Format("0x%X (%s)", static_cast<unsigned>(r.classIdLane.raw()),
                      r.laneReliable ? "reliable" : "UNRELIABLE")
      << "\n";
    f << "classes: " << stats.classes << "\n";
    f << "classes_without_recvtable: " << stats.classesWithoutTable << "\n";
    f << "classes_skipped: " << stats.classesSkipped << "\n";
    f << "table visits: " << stats.tableVisits << "\n";
    f << "netvar entries (unique): " << stats.entries << "\n";
    f << "netvar entries (total, with dupes): " << stats.totalPushed << "\n";
    f << "duplicates removed: " << stats.duplicates << "\n";
    f << "digit arrays expanded: " << stats.digitArraysExpanded << "\n";
    f << "memoized walk skips: " << stats.memoHits << "\n";
    f << "skipped (digit lead): " << stats.skippedDigits << "\n";
    f << "skipped (bad name): " << stats.skippedNullName << "\n";
    f << "table files: " << stats.tableFiles << "\n";
    f << "class files: " << stats.classFiles << "\n";
    f << "class rows: " << stats.classRows << "\n";
    f << "hierarchies: " << stats.hierarchies << "\n";
    f << "hierarchy props: " << stats.hierarchyProps << "\n";
    f << "vtable ifaces: " << stats.vtableIfaces << "\n";
    f << "vtable slots: " << stats.vtableSlots << "\n";
    f << "vtable skipped (garbage): " << stats.vtableSkipped << "\n";
    f << "filename collisions: " << stats.fileNameCollisions << "\n";
    f << "namespace collisions: " << stats.namespaceCollisions << "\n";
    return true;
}

}

TableIndex TableIndex::Build(std::span<const dump::NetvarEntry> entries) {
    TableIndex index;
    for (const dump::NetvarEntry& e : entries) {
        index.m_byTable[e.table].push_back(&e);
    }
    return index;
}

bool WriteAll(const Report& report, dump::WalkStats& stats, std::string& err) {
    CreateDirectoryA((report.dir + "\\Classes").c_str(), nullptr);

    const TableIndex index = TableIndex::Build(report.netvars);

    if (!WriteOffsetsHpp(report, index, stats)) { err = "write offsets.hpp"; return false; }
    if (!WriteOffsetsJson(report)) { err = "write offsets.json"; return false; }
    if (!WriteNetvarsTxt(report)) { err = "write netvars.txt"; return false; }
    if (!WriteNetvarsJson(report)) { err = "write netvars.json"; return false; }
    if (!WriteVtablesTxt(report, stats)) { err = "write interfaces_vtables.txt"; return false; }
    if (!WriteInterfacesTxt(report, stats)) { err = "write interfaces.txt"; return false; }
    if (!WriteInterfacesJson(report, stats)) { err = "write interfaces.json"; return false; }
    if (!WriteRecvHierarchy(report, stats)) { err = "write recv_hierarchy.txt"; return false; }
    WriteClassFiles(report, index, stats);
    if (!WriteInfoTxt(report, stats)) { err = "write dump_info.txt"; return false; }
    return true;
}

}

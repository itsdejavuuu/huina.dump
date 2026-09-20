#include "emit/report.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>
#include "core/text.hpp"
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

bool WriteInterfacesTxt(const Report& r) {
    std::ofstream f(r.dir + "\\interfaces.txt", std::ios::trunc);
    if (!f) return false;

    f << "=== Interfaces (" << r.interfaces.size() << ") ===\n";
    for (const dump::InterfaceHandle& i : r.interfaces) {
        f << "[" << i.module << "] " << i.version << " = " << text::HexAddress(i.instance.raw())
          << " (vtable " << text::HexAddress(i.vtable.raw()) << ")\n";
    }
    f << "\n=== ClientClasses (" << r.classes.size() << ") ===\n";
    for (const dump::ClassInfo& c : r.classes) {
        f << c.name << " (id " << c.id << ") -> " << c.table << "\n";
    }
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
    if (!WriteInterfacesTxt(report)) { err = "write interfaces.txt"; return false; }
    WriteClassFiles(report, index, stats);
    if (!WriteInfoTxt(report, stats)) { err = "write dump_info.txt"; return false; }
    return true;
}

}

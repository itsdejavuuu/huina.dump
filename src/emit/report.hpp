#pragma once

#include <map>
#include <span>
#include <string>
#include <vector>
#include "core/mem.hpp"
#include "core/types.hpp"
#include "dump/interfaces.hpp"
#include "dump/netvars.hpp"
#include "dump/offsets.hpp"

namespace hd::emit {

class TableIndex {
public:
    using Map = std::map<std::string, std::vector<const dump::NetvarEntry*>, std::less<>>;

    [[nodiscard]] static TableIndex Build(std::span<const dump::NetvarEntry> entries);
    [[nodiscard]] const Map& tables() const noexcept { return m_byTable; }

private:
    Map m_byTable;
};

struct Report {
    std::string dir;
    std::span<const mem::Module> modules;
    std::span<const dump::ResolvedOffset> offsets;
    std::span<const dump::NetvarEntry> netvars;
    std::span<const dump::ClassInfo> classes;
    std::span<const dump::ClassDump> classDumps;
    std::span<const dump::ClassHierarchy> hierarchies;
    std::span<const dump::InterfaceHandle> interfaces;
    types::Lane classIdLane{};
    bool laneReliable = false;
};

[[nodiscard]] bool WriteAll(const Report& report, dump::WalkStats& stats, std::string& err);

}

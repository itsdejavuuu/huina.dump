#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "core/types.hpp"
#include "sdk/recv.hpp"

namespace hd::dump {

struct NetvarEntry {
    std::string table;
    std::string prop;
    types::PropOffset offset{};
    int type = -1;
    int elements = 1;
    int stride = 0;
    int flags = 0;
};

struct ClassInfo {
    std::string name;
    int id = -1;
    std::string table;
};

struct ClassDump {
    std::string name;
    int id = -1;
    std::string table;
    std::vector<NetvarEntry> props;
};

struct HierarchyProp {
    std::string name;
    int32_t offset = 0;
    int type = -1;
    int flags = 0;
    std::string table;
    std::vector<HierarchyProp> children;
};

struct ClassHierarchy {
    std::string className;
    int id = -1;
    std::string rootTable;
    std::vector<HierarchyProp> props;
};

struct WalkStats {
    std::size_t classes = 0;
    std::size_t classesWithoutTable = 0;
    std::size_t classesSkipped = 0;
    std::size_t tableVisits = 0;
    std::size_t entries = 0;
    std::size_t totalPushed = 0;
    std::size_t duplicates = 0;
    std::size_t skippedDigits = 0;
    std::size_t skippedNullName = 0;
    std::size_t digitArraysExpanded = 0;
    std::size_t memoHits = 0;
    std::size_t tableFiles = 0;
    std::size_t classFiles = 0;
    std::size_t classRows = 0;
    std::size_t fileNameCollisions = 0;
    std::size_t namespaceCollisions = 0;
    std::size_t hierarchies = 0;
    std::size_t hierarchyProps = 0;
    std::size_t vtableIfaces = 0;
    std::size_t vtableSlots = 0;
    std::size_t vtableSkipped = 0;
};

[[nodiscard]] bool WalkAll(sdk::ClientClass* head, types::Lane lane,
                           std::vector<NetvarEntry>& entries,
                           std::vector<ClassInfo>& classes,
                           WalkStats& stats);

[[nodiscard]] bool WalkClass(sdk::RecvTable* root, std::vector<NetvarEntry>& out);

[[nodiscard]] bool BuildHierarchies(sdk::ClientClass* head, types::Lane lane,
                                    std::vector<ClassHierarchy>& out);

}

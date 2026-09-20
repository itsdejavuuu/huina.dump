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
};

[[nodiscard]] bool WalkAll(sdk::ClientClass* head, types::Lane lane,
                           std::vector<NetvarEntry>& entries,
                           std::vector<ClassInfo>& classes,
                           WalkStats& stats);

[[nodiscard]] bool WalkClass(sdk::RecvTable* root, std::vector<NetvarEntry>& out);

}

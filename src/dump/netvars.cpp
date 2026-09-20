#include "dump/netvars.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include "core/seh.hpp"
#include "core/mem.hpp"
#include "dump/limits.hpp"

namespace hd::dump {
namespace {

struct WalkKey {
    const sdk::RecvTable* table = nullptr;
    types::PropOffset base{};

    friend bool operator==(const WalkKey& a, const WalkKey& b) noexcept {
        return a.table == b.table && a.base == b.base;
    }
};

struct WalkKeyHash {
    std::size_t operator()(const WalkKey& k) const noexcept {
        const std::size_t h1 = std::hash<const void*>{}(static_cast<const void*>(k.table));
        const std::size_t h2 = std::hash<int32_t>{}(k.base.raw());
        return h1 ^ (h2 + 0x9e3779b9u + (h1 << 6) + (h1 >> 2));
    }
};

struct VectorSink {
    std::vector<NetvarEntry>* out = nullptr;
    void push(const NetvarEntry& e) const { out->push_back(e); }
};

struct TableHeader {
    sdk::RecvProp* props = nullptr;
    int32_t nProps = 0;
    std::string name;
};

[[nodiscard]] bool ReadTableHeader(sdk::RecvTable* t, TableHeader& out) {
    if (!t) return false;
    int32_t n = 0;
    uintptr_t props = 0;
    uintptr_t namePtr = 0;
    if (!seh::Peek(static_cast<const void*>(&t->m_nProps), n)) return false;
    if (n <= 0 || n > kMaxTableProps) return false;
    if (!seh::Peek(static_cast<const void*>(&t->m_pProps), props) || !props) return false;
    if (!seh::Peek(static_cast<const void*>(&t->m_pNetTableName), namePtr)) return false;
    out.nProps = n;
    out.props = reinterpret_cast<sdk::RecvProp*>(props);
    return mem::ReadCStr(reinterpret_cast<const char*>(namePtr), out.name, kMaxStringLen);
}

template <class TSink>
void ExpandDigitArray(sdk::RecvTable* subTable, const std::string& parentTable,
                      const std::string& parentProp, types::PropOffset parentBase,
                      types::Stride parentStride, int parentType,
                      TSink& sink, WalkStats& stats) {
    if (!subTable) return;

    int32_t n = 0;
    uintptr_t props = 0;
    if (!seh::Peek(static_cast<const void*>(&subTable->m_nProps), n)) return;
    if (n <= 0 || n >= kMaxDigitArrayProps) return;
    if (!seh::Peek(static_cast<const void*>(&subTable->m_pProps), props) || !props) return;
    auto* arr = reinterpret_cast<sdk::RecvProp*>(props);

    struct Elem {
        int index;
        types::PropOffset subOffset;
        int type;
        int flags;
        int stride;
    };
    std::vector<Elem> elems;
    for (int i = 0; i < n; ++i) {
        sdk::RecvProp p{};
        if (!seh::Bytes(static_cast<const void*>(arr + i), &p, sizeof(p))) return;

        std::string nm;
        if (!mem::ReadCStr(p.m_pVarName, nm, kMaxDigitNameLen)) return;

        bool allDigits = true;
        for (const char c : nm) {
            if (c < '0' || c > '9') { allDigits = false; break; }
        }
        if (!allDigits) {
            if (nm == "lengthproxy" || nm == "lengthprop") continue;
            return;
        }

        int idx = 0;
        for (const char c : nm) idx = idx * 10 + (c - '0');
        elems.push_back(Elem{ idx, types::PropOffset{ p.m_Offset }, p.m_RecvType,
                              p.m_Flags, p.m_ElementStride });
    }
    if (elems.empty()) return;

    const bool useStride = (parentStride.raw() > 0 && parentStride.raw() < kMaxElementStride);
    std::sort(elems.begin(), elems.end(),
              [](const Elem& a, const Elem& b) { return a.index < b.index; });

    for (const Elem& el : elems) {
        NetvarEntry e;
        e.table = parentTable;
        e.prop = parentProp + "[" + std::to_string(el.index) + "]";
        e.offset = useStride ? parentBase + parentStride * types::ElementsOf(el.index)
                             : parentBase + el.subOffset;
        e.type = (parentType == static_cast<int>(sdk::PropKind::Array)) ? parentType : el.type;
        e.elements = static_cast<int>(elems.size());
        e.stride = useStride ? parentStride.raw() : el.stride;
        e.flags = el.flags;
        sink.push(e);
    }
    ++stats.digitArraysExpanded;
}

class Walker {
public:
    explicit Walker(WalkStats& stats) noexcept : m_stats(stats) {}

    template <class TSink>
    void Walk(sdk::RecvTable* table, types::PropOffset base, int depth, TSink& sink) {
        if (!table || depth > kMaxWalkDepth) return;

        for (const sdk::RecvTable* onStack : m_stack) {
            if (onStack == table) return;
        }

        if (!m_memo.insert(WalkKey{ table, base }).second) {
            ++m_stats.memoHits;
            return;
        }

        TableHeader h;
        if (!ReadTableHeader(table, h)) return;
        ++m_stats.tableVisits;
        m_stack.push_back(table);

        for (int32_t i = 0; i < h.nProps; ++i) {
            sdk::RecvProp prop{};
            if (!seh::Bytes(static_cast<const void*>(h.props + i), &prop, sizeof(prop))) continue;

            std::string propName;
            if (!mem::ReadCStr(prop.m_pVarName, propName, kMaxStringLen)) {
                ++m_stats.skippedNullName;
                continue;
            }
            if (propName[0] >= '0' && propName[0] <= '9') {
                ++m_stats.skippedDigits;
                continue;
            }

            const bool isBaseClass = (propName == "baseclass");
            const types::PropOffset total = base + types::PropOffset{ prop.m_Offset };

            if (!isBaseClass) {
                const int count = (prop.m_nElements > 0) ? prop.m_nElements : 1;
                if (count > 1 && prop.m_pDataTable == nullptr) {
                    NetvarEntry baseEntry;
                    baseEntry.table = h.name;
                    baseEntry.prop = propName;
                    baseEntry.offset = total;
                    baseEntry.type = prop.m_RecvType;
                    baseEntry.elements = count;
                    baseEntry.stride = prop.m_ElementStride;
                    baseEntry.flags = prop.m_Flags;
                    sink.push(baseEntry);

                    if (prop.m_ElementStride > 0 && prop.m_ElementStride < kMaxElementStride &&
                        count < kMaxDigitArrayProps) {
                        for (int j = 0; j < count; ++j) {
                            NetvarEntry e = baseEntry;
                            e.prop = propName + "[" + std::to_string(j) + "]";
                            e.offset = total + types::Stride{ prop.m_ElementStride } *
                                                  types::ElementsOf(j);
                            sink.push(e);
                        }
                    }
                } else {
                    NetvarEntry e;
                    e.table = h.name;
                    e.prop = propName;
                    e.offset = total;
                    e.type = prop.m_RecvType;
                    e.elements = 1;
                    e.stride = prop.m_ElementStride;
                    e.flags = prop.m_Flags;
                    sink.push(e);
                }
            }

            if (prop.m_pDataTable) {
                if (!isBaseClass) {
                    ExpandDigitArray(prop.m_pDataTable, h.name, propName, total,
                                     types::Stride{ prop.m_ElementStride }, prop.m_RecvType,
                                     sink, m_stats);
                }
                Walk(prop.m_pDataTable, total, depth + 1, sink);
            }
        }

        m_stack.pop_back();
    }

private:
    std::unordered_set<WalkKey, WalkKeyHash> m_memo;
    std::vector<sdk::RecvTable*> m_stack;
    WalkStats& m_stats;
};

}

bool WalkAll(sdk::ClientClass* head, types::Lane lane,
             std::vector<NetvarEntry>& entries,
             std::vector<ClassInfo>& classes,
             WalkStats& stats) {
    entries.clear();
    classes.clear();
    if (!head) return false;
    if (lane.raw() < 0 || lane.raw() > kMaxLaneProbe) lane = types::kDefaultLane;

    VectorSink sink{ &entries };
    Walker walker(stats);

    sdk::ClientClass* cur = head;
    std::size_t guard = 0;
    while (cur && guard < kClassChainLimit) {
        sdk::ClientClass node{};
        if (!seh::Bytes(static_cast<const void*>(cur), &node, sizeof(node))) {
            ++stats.classesSkipped;
            break;
        }

        std::string className;
        if (mem::ReadCStr(node.m_pNetworkName, className, kMaxStringLen)) {
            ClassInfo ci;
            ci.name = std::move(className);
            ci.id = sdk::ReadClassId(node, lane).raw();

            if (node.m_pRecvTable) {
                uintptr_t namePtr = 0;
                if (seh::Peek(static_cast<const void*>(&node.m_pRecvTable->m_pNetTableName), namePtr)) {
                    (void)mem::ReadCStr(reinterpret_cast<const char*>(namePtr), ci.table, kMaxStringLen);
                }
                walker.Walk(node.m_pRecvTable, types::PropOffset{}, 0, sink);
            } else {
                ++stats.classesWithoutTable;
            }
            classes.push_back(std::move(ci));
        } else {
            ++stats.classesSkipped;
        }
        ++stats.classes;
        cur = node.m_pNext;
        ++guard;
    }

    std::sort(entries.begin(), entries.end(), [](const NetvarEntry& a, const NetvarEntry& b) {
        if (a.table != b.table) return a.table < b.table;
        if (a.prop != b.prop) return a.prop < b.prop;
        return a.offset < b.offset;
    });

    const std::size_t before = entries.size();
    entries.erase(
        std::unique(entries.begin(), entries.end(),
                    [](const NetvarEntry& a, const NetvarEntry& b) {
                        return a.table == b.table && a.prop == b.prop && a.offset == b.offset;
                    }),
        entries.end());
    stats.totalPushed = before;
    stats.duplicates = before - entries.size();
    stats.entries = entries.size();
    return !entries.empty();
}

bool WalkClass(sdk::RecvTable* root, std::vector<NetvarEntry>& out) {
    out.clear();
    if (!root) return false;

    WalkStats local{};
    VectorSink sink{ &out };
    Walker walker(local);
    walker.Walk(root, types::PropOffset{}, 0, sink);

    std::sort(out.begin(), out.end(), [](const NetvarEntry& a, const NetvarEntry& b) {
        if (a.offset != b.offset) return a.offset < b.offset;
        if (a.table != b.table) return a.table < b.table;
        return a.prop < b.prop;
    });
    out.erase(
        std::unique(out.begin(), out.end(),
                    [](const NetvarEntry& a, const NetvarEntry& b) {
                        return a.table == b.table && a.prop == b.prop && a.offset == b.offset;
                    }),
        out.end());
    return !out.empty();
}

}
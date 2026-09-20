#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include "core/types.hpp"

namespace hd::sdk {

enum class PropKind : int32_t { Int = 0, Float, Vector, VectorXY, String, Array, DataTable, Count };

inline constexpr std::string_view kPropKindNames[] = {
    "Int", "Float", "Vector", "VectorXY", "String", "Array", "DataTable",
};
static_assert(sizeof(kPropKindNames) / sizeof(kPropKindNames[0]) ==
              static_cast<std::size_t>(PropKind::Count));

[[nodiscard]] inline std::string_view PropTypeName(int type) noexcept {
    const bool known = type >= 0 && type < static_cast<int>(PropKind::Count);
    return known ? kPropKindNames[static_cast<std::size_t>(type)] : std::string_view{ "Unknown" };
}

[[nodiscard]] inline std::string PropTypeLabel(int type) {
    if (type >= 0 && type <= static_cast<int>(PropKind::DataTable)) return std::string(PropTypeName(type));
    return std::string("Unknown(") + std::to_string(type) + ")";
}

struct RecvTable;
struct ClientClass;

struct RecvProp {
    const char*  m_pVarName;
    int32_t      m_RecvType;
    int32_t      m_Flags;
    int32_t      m_StringBufferSize;
    int32_t      m_bInsideArray;
    const void*  m_pExtraData;
    RecvProp*    m_pArrayProp;
    void*        m_ArrayLengthProxy;
    void*        m_ProxyFn;
    void*        m_DataTableProxyFn;
    RecvTable*   m_pDataTable;
    int32_t      m_Offset;
    int32_t      m_ElementStride;
    int32_t      m_nElements;
    const char*  m_pParentArrayPropName;
};

struct RecvTable {
    RecvProp* m_pProps;
    int32_t   m_nProps;
    int32_t   _pad;
    void*     m_pDecoder;
    char*     m_pNetTableName;
};

struct ClientClass {
    void*        m_pCreateFn;
    void*        m_pCreateEventFn;
    char*        m_pNetworkName;
    RecvTable*   m_pRecvTable;
    ClientClass* m_pNext;
    int32_t      m_ClassID;
};

static_assert(sizeof(RecvProp) == 0x60 &&
              offsetof(RecvProp, m_RecvType) == 0x08 && offsetof(RecvProp, m_Flags) == 0x0C &&
              offsetof(RecvProp, m_pDataTable) == 0x40 && offsetof(RecvProp, m_Offset) == 0x48 &&
              offsetof(RecvProp, m_ElementStride) == 0x4C && offsetof(RecvProp, m_nElements) == 0x50 &&
              offsetof(RecvProp, m_pParentArrayPropName) == 0x58, "RecvProp layout broken");
static_assert(offsetof(RecvTable, m_pProps) == 0x00 && offsetof(RecvTable, m_nProps) == 0x08 &&
              offsetof(RecvTable, m_pNetTableName) == 0x18, "RecvTable layout broken");
static_assert(offsetof(ClientClass, m_pNetworkName) == 0x10 && offsetof(ClientClass, m_pRecvTable) == 0x18 &&
              offsetof(ClientClass, m_pNext) == 0x20 && offsetof(ClientClass, m_ClassID) == 0x28,
              "ClientClass layout broken");

[[nodiscard]] inline int32_t ReadLane(const ClientClass& node, types::Lane lane) noexcept {
    int32_t v = 0;
    std::memcpy(&v, reinterpret_cast<const std::uint8_t*>(&node) + lane.raw(), sizeof(v));
    return v;
}

[[nodiscard]] inline types::ClassId ReadClassId(const ClientClass& node, types::Lane lane) noexcept {
    const int32_t v = ReadLane(node, lane);
    return types::IsPlausibleClassId(v) ? types::ClassId{ v } : types::ClassId{ -1 };
}

}

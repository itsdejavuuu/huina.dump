#pragma once
#include <cstddef>
#include <cstdint>

namespace hd::dump {

inline constexpr int kGetAllClassesSlot = 8;
inline constexpr int kHeadScanSlots = 64;
inline constexpr int kValidateNodes = 8;
inline constexpr int kMinValidateNodes = 3;
inline constexpr int kLaneProbeNodes = 64;
inline constexpr int kMaxLaneProbe = 64;
inline constexpr std::size_t kMinDistinctForReliable = 10;
inline constexpr std::size_t kClassChainLimit = 5000;

inline constexpr int32_t kMaxTableProps = 4096;
inline constexpr int32_t kMaxWalkDepth = 32;
inline constexpr int32_t kMaxDigitArrayProps = 512;
inline constexpr int32_t kMaxElementStride = 0x10000;

inline constexpr std::size_t kMaxStringLen = 128;
inline constexpr std::size_t kMaxDigitNameLen = 16;

}

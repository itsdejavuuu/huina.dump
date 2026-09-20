#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "core/types.hpp"
#include "sdk/recv.hpp"

namespace hd::dump {

struct InterfaceHandle {
    std::string module;
    std::string version;
    types::Va instance{};
    types::Va vtable{};
};

[[nodiscard]] std::vector<InterfaceHandle> EnumerateInterfaces();

[[nodiscard]] types::Va GrabInterface(std::string_view module, std::string_view version) noexcept;

[[nodiscard]] sdk::ClientClass* FindClassListHead(void* clientInstance) noexcept;

[[nodiscard]] types::Lane DetectClassIdLane(sdk::ClientClass* head, bool& reliableOut) noexcept;

[[nodiscard]] bool HasAssignedClassIds(sdk::ClientClass* head, types::Lane lane) noexcept;

}

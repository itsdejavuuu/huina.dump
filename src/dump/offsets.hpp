#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "core/types.hpp"

namespace hd::mem { class Module; }

namespace hd::dump {

struct ResolvedOffset {
    std::string name;
    std::string module;
    types::Rva rva{};
    types::Va absolute{};
    bool isImm = false;
    std::uint32_t imm = 0;
};

[[nodiscard]] bool ResolveOffsets(const mem::Module& client, const mem::Module& engine,
                                  std::vector<ResolvedOffset>& out, std::string& err);

}

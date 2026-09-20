#pragma once

#include <cstddef>
#include <string_view>

namespace hd::log {

class Sink {
public:
    static void Open(std::string_view path);
    static void Line(std::string_view msg);

private:
    static void WriteRaw(const char* data, std::size_t size) noexcept;
};

inline void Line(std::string_view msg) { Sink::Line(msg); }

}

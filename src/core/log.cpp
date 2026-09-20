#include "core/log.hpp"
#include <mutex>
#include <string>
#include <windows.h>

namespace hd::log {
namespace {

std::mutex s_mutex;
std::string s_file;

}

void Sink::Open(std::string_view path) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_file.assign(path);
    std::FILE* f = nullptr;
    fopen_s(&f, s_file.c_str(), "wb");
    if (f) std::fclose(f);
}

void Sink::WriteRaw(const char* data, std::size_t size) noexcept {
    if (s_file.empty()) return;
    HANDLE h = CreateFileA(s_file.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(h, data, static_cast<DWORD>(size), &written, nullptr);
    const char nl[] = "\r\n";
    WriteFile(h, nl, 2, &written, nullptr);
    CloseHandle(h);
}

void Sink::Line(std::string_view msg) {
    std::lock_guard<std::mutex> lock(s_mutex);
    WriteRaw(msg.data(), msg.size());
}

}

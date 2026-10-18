#include "core/paths.h"

#include <string>

namespace ddmod::paths {
namespace {

std::filesystem::path g_modDir;
std::filesystem::path g_exePath;

std::filesystem::path ModuleFileName(HMODULE module) {
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD len = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (len == 0) return {};
        if (len < buffer.size()) {
            buffer.resize(len);
            return buffer;
        }
        buffer.resize(buffer.size() * 2);  // path longer than the buffer, try again
    }
}

}  // namespace

void Init(HMODULE self) {
    g_modDir = ModuleFileName(self).parent_path();
    g_exePath = ModuleFileName(nullptr);
}

const std::filesystem::path& ModDir() { return g_modDir; }

const std::filesystem::path& ExePath() { return g_exePath; }

}  // namespace ddmod::paths

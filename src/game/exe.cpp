#include "game/exe.h"

#include <windows.h>

#include "core/log.h"
#include "core/paths.h"
#include "core/scanner.h"
#include "core/sha256.h"

namespace ddmod::game {
namespace {

struct KnownBuild {
    const char* sha256;
    const char* name;
};

// See docs/re/game-version.md. Same hash for German and English Steam and on the Steam Deck.
constexpr KnownBuild kKnownBuilds[] = {
    {"CD9F7A07C7A605C21052B3517346C7A63083063FB491276A3E4C9836C9B42A67", "1.0062A (Steam)"},
};

ExeInfo g_exe;

}  // namespace

void IdentifyExe(const std::filesystem::path& exe) {
    g_exe = {};
    const auto digest = sha256::OfFile(exe);
    if (!digest) {
        const DWORD error = GetLastError();
        log::Warn("EXE hash: cannot read {} (error {}) - fixed-address fallbacks disabled", paths::ToUtf8(exe),
                  error);
        return;
    }
    g_exe.sha256 = sha256::ToHex(*digest);
    for (const KnownBuild& known : kKnownBuilds) {
        if (g_exe.sha256 == known.sha256) {
            g_exe.build = known.name;
            g_exe.verified = true;
            log::Info("EXE hash: {} = {} (verified)", g_exe.sha256, known.name);
            return;
        }
    }
    log::Warn("EXE hash: {} is not a known build - signature scans only, fixed-address fallbacks disabled",
              g_exe.sha256);
}

const ExeInfo& Exe() { return g_exe; }

std::uintptr_t Resolve(const Signature& sig) {
    const HMODULE exe = GetModuleHandleW(nullptr);
    const auto area = sig.code ? scanner::Area::Code : scanner::Area::Image;
    if (const std::uintptr_t match = scanner::FindUnique(sig.name, exe, sig.pattern, area)) {
        return match + static_cast<std::uintptr_t>(sig.offset);
    }
    if (sig.fallbackRva && g_exe.verified) {
        const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(exe) + *sig.fallbackRva;
        log::Warn("Signature '{}': using fixed fallback 0x{:08X} for {}", sig.name, address, g_exe.build);
        return address;
    }
    log::Error("Signature '{}': unresolved", sig.name);
    return 0;
}

}  // namespace ddmod::game

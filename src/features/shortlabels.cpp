#include "features/shortlabels.h"

#include <cstring>

#include "core/hooks.h"
#include "core/log.h"
#include "game/exe.h"

namespace ddmod::features {
namespace {

using TextLoaderFn = void(__thiscall*)(void* table, const char* file);

TextLoaderFn g_textLoader = nullptr;

template <typename T>
T Field(const void* object, std::size_t offset) {
    T value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(object) + offset, sizeof(T));
    return value;
}

// Tables are a few thousand entries; anything larger is not the layout this code expects.
constexpr int kMaxCount = 100000;

bool Replace(char* text, const ShortLabel& label) {
    if (!text || std::strlen(text) != label.original.size() ||
        std::memcmp(text, label.original.data(), label.original.size()) != 0) {
        return false;
    }
    std::memcpy(text, label.shorter.data(), label.shorter.size());
    text[label.shorter.size()] = '\0';
    return true;
}

// See FontInitDetour (fontmode.cpp) for the __fastcall stand-in of __thiscall.
void __fastcall TextLoaderDetour(void* table, void* /*edx*/, const char* file) {
    g_textLoader(table, file);
    const int changed = ShortenLabels(table, kShortLabels);
    log::Info("Short labels: {} text(s) shortened in {}", changed, file ? file : "?");
}

}  // namespace

int ShortenLabels(void* table, std::span<const ShortLabel> labels) {
    if (!table) return 0;
    const auto sections = Field<std::uint8_t**>(table, kTableSections);
    const int sectionCount = Field<int>(table, kTableSectionCount);
    if (!sections || sectionCount <= 0 || sectionCount > kMaxCount) return 0;
    int changed = 0;
    for (int s = 0; s < sectionCount; ++s) {
        const std::uint8_t* section = sections[s];
        if (!section) continue;
        const auto entries = Field<std::uint8_t**>(section, kSectionEntries);
        const int entryCount = Field<int>(section, kSectionEntryCount);
        if (!entries || entryCount <= 0 || entryCount > kMaxCount) continue;
        for (int e = 0; e < entryCount; ++e) {
            std::uint8_t* entry = entries[e];
            if (!entry) continue;
            for (const ShortLabel& label : labels) {
                if (label.shorter.size() > label.original.size()) continue;
                if (!Replace(Field<char*>(entry, kEntryText), label)) continue;
                Replace(Field<char*>(entry, kEntryResolved), label);
                const auto length = static_cast<std::uint32_t>(label.shorter.size());
                std::memcpy(entry + kEntryLength, &length, sizeof(length));
                ++changed;
                break;
            }
        }
    }
    return changed;
}

bool InstallShortLabels() {
    const std::uintptr_t loader = game::Resolve({"TextLoader", kTextLoaderPattern, 0, kTextLoaderRva});
    if (!loader) {
        log::Warn("Short labels: text loader not found - original menu texts");
        return false;
    }
    if (!hooks::CreateRaw("TextLoader", reinterpret_cast<void*>(loader), reinterpret_cast<void*>(&TextLoaderDetour),
                          reinterpret_cast<void**>(&g_textLoader)) ||
        !hooks::Enable("TextLoader")) {
        return false;
    }
    log::Info("Short labels hook installed ([Fonts] ShortLabels=1)");
    return true;
}

}  // namespace ddmod::features

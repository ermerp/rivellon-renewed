#include "core/scanner.h"

#include <cstring>

#include "core/log.h"

namespace ddmod::scanner {

std::vector<Section> Sections(HMODULE module) {
    std::vector<Section> out;
    if (!module) return out;
    const auto* base = reinterpret_cast<const std::uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return out;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return out;

    const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
        // VirtualSize is the mapped size; fall back to the raw size for linkers that leave it at 0.
        const DWORD size = section->Misc.VirtualSize ? section->Misc.VirtualSize : section->SizeOfRawData;
        if (size == 0) continue;
        char name[IMAGE_SIZEOF_SHORT_NAME + 1] = {};
        std::memcpy(name, section->Name, IMAGE_SIZEOF_SHORT_NAME);
        out.push_back({name, std::span(base + section->VirtualAddress, size),
                       (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0});
    }
    return out;
}

std::vector<std::uintptr_t> FindAll(HMODULE module, const pattern::Pattern& pattern, Area area,
                                    std::size_t maxMatches) {
    std::vector<std::uintptr_t> out;
    for (const Section& section : Sections(module)) {
        if (area == Area::Code && !section.executable) continue;
        if (out.size() >= maxMatches) break;
        for (const std::size_t offset : pattern::FindAll(section.bytes, pattern, maxMatches - out.size())) {
            out.push_back(reinterpret_cast<std::uintptr_t>(section.bytes.data()) + offset);
        }
    }
    return out;
}

std::uintptr_t FindUnique(std::string_view name, HMODULE module, std::string_view patternText, Area area) {
    const auto pattern = pattern::Parse(patternText);
    if (!pattern) {
        log::Error("Signature '{}': invalid pattern \"{}\"", name, patternText);
        return 0;
    }
    // Two matches are enough to know the pattern is ambiguous.
    const auto matches = FindAll(module, *pattern, area, 2);
    if (matches.empty()) {
        log::Warn("Signature '{}': not found", name);
        return 0;
    }
    if (matches.size() > 1) {
        log::Warn("Signature '{}': ambiguous (0x{:08X}, 0x{:08X}, ...)", name, matches[0], matches[1]);
        return 0;
    }
    log::Debug("Signature '{}': 0x{:08X}", name, matches[0]);
    return matches[0];
}

std::uintptr_t FollowRel32(std::uintptr_t operand) {
    std::int32_t rel = 0;
    std::memcpy(&rel, reinterpret_cast<const void*>(operand), sizeof(rel));
    return operand + sizeof(rel) + static_cast<std::uintptr_t>(static_cast<std::intptr_t>(rel));
}

std::uint32_t ReadU32(std::uintptr_t address) {
    std::uint32_t value = 0;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
    return value;
}

}  // namespace ddmod::scanner

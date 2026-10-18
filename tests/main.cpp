// Unit tests for the parts of ddmod that do not need the game: SHA-256, pattern parsing and
// matching, PE section scanning, the MinHook wrapper, the hotkey parser and log rotation.
// Exit code 0 = all passed.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <windows.h>

#include "core/hooks.h"
#include "core/log.h"
#include "core/pattern.h"
#include "core/scanner.h"
#include "core/sha256.h"
#include "input/keys.h"

namespace {

int g_failures = 0;
int g_checks = 0;

void Check(bool condition, const char* what, int line) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::printf("FAILED (line %d): %s\n", line, what);
    }
}
#define CHECK(x) Check((x), #x, __LINE__)

std::span<const std::uint8_t> Bytes(std::string_view text) {
    return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

std::string HexOf(std::string_view text) { return ddmod::sha256::ToHex(ddmod::sha256::Of(Bytes(text))); }

void TestSha256() {
    using namespace ddmod::sha256;
    // FIPS 180-4 / NIST example vectors.
    CHECK(HexOf("") == "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855");
    CHECK(HexOf("abc") == "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD");
    CHECK(HexOf("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248D6A61D20638B8E5C026930C3E6039A33CE45964FF2167F6ECEDD419DB06C1");

    const std::string million(1'000'000, 'a');
    CHECK(HexOf(million) == "CDC76E5C9914FB9281A1C7E284D73E67F1809A48A497200E046D39CCC7112CD0");

    // Feeding odd-sized chunks must give the same digest as one call.
    Hasher hasher;
    const auto data = Bytes(million);
    for (std::size_t pos = 0, step = 1; pos < data.size(); pos += step, step = step % 97 + 1) {
        hasher.Update(data.subspan(pos, std::min(step, data.size() - pos)));
    }
    CHECK(ToHex(hasher.Finish()) == HexOf(million));
}

void TestPatternParse() {
    using ddmod::pattern::Parse;
    const auto p = Parse("8B 0D ?? ?? ?? ?? 85 c9");
    CHECK(p.has_value());
    CHECK(p && p->size() == 8);
    CHECK(p && p->bytes[0] == 0x8B && p->bytes[7] == 0xC9);
    CHECK(p && p->wildcard[2] && p->wildcard[5] && !p->wildcard[6]);
    CHECK(Parse("  E8 ? ? ? ?  ").has_value());
    CHECK(!Parse("").has_value());
    CHECK(!Parse("?? ??").has_value());
    CHECK(!Parse("8G").has_value());
    CHECK(!Parse("123").has_value());
    CHECK(!Parse("8B0D").has_value());
}

void TestPatternFind() {
    using namespace ddmod::pattern;
    const std::uint8_t data[] = {0x55, 0x8B, 0xEC, 0x55, 0x8B, 0xEC, 0x90, 0x55, 0x55, 0x8B};
    const auto find = [&](const char* text, std::size_t max = SIZE_MAX) { return FindAll(data, *Parse(text), max); };

    CHECK(find("55 8B EC") == (std::vector<std::size_t>{0, 3}));
    CHECK(find("55 ?? EC") == (std::vector<std::size_t>{0, 3}));
    CHECK(find("?? 55") == (std::vector<std::size_t>{2, 6, 7}));  // leading wildcard
    CHECK(find("55 8B") == (std::vector<std::size_t>{0, 3, 8}));   // match ending at the last byte
    CHECK(find("8B ?? ??").size() == 2);                           // the third 8B has no room
    CHECK(find("55 55") == (std::vector<std::size_t>{7}));
    CHECK(find("55", 2) == (std::vector<std::size_t>{0, 3}));
    CHECK(find("CC").empty());
    CHECK(FindAll(std::span(data, 2), *Parse("55 8B EC")).empty());  // pattern longer than data
}

// A marker in read-only data: found with Area::Image, not with Area::Code.
extern const std::uint8_t kMarker[];
const std::uint8_t kMarker[] = {0xD1, 0x7E, 0x5A, 0x11, 0xC0, 0xDE, 0x13, 0x37, 0xFE, 0xED};

void TestScanner() {
    using namespace ddmod;
    const HMODULE self = GetModuleHandleW(nullptr);
    const auto sections = scanner::Sections(self);
    bool hasCode = false;
    for (const auto& s : sections) hasCode |= s.executable && s.name == ".text";
    CHECK(hasCode);

    const char* text = "D1 7E 5A 11 C0 DE 13 37 FE ED";
    const auto inImage = scanner::FindAll(self, *pattern::Parse(text), scanner::Area::Image);
    CHECK(inImage.size() == 1);
    CHECK(!inImage.empty() && inImage[0] == reinterpret_cast<std::uintptr_t>(kMarker));
    CHECK(scanner::FindAll(self, *pattern::Parse(text), scanner::Area::Code).empty());
    CHECK(scanner::FindUnique("marker", self, "D1 7E 5A ?? C0 DE", scanner::Area::Image) ==
          reinterpret_cast<std::uintptr_t>(kMarker));
    CHECK(scanner::FindUnique("bad", self, "XY", scanner::Area::Image) == 0);

    // call rel32 at offset 0 of the buffer; the target is 0x10 bytes past the end of the call.
    std::uint8_t call[5] = {0xE8, 0x10, 0x00, 0x00, 0x00};
    const auto base = reinterpret_cast<std::uintptr_t>(call);
    CHECK(scanner::FollowRel32(base + 1) == base + 5 + 0x10);
    const std::int32_t back = -0x20;
    std::memcpy(call + 1, &back, sizeof(back));
    CHECK(scanner::FollowRel32(base + 1) == base + 5 - 0x20);
    std::memcpy(call + 1, "\x78\x56\x34\x12", 4);
    CHECK(scanner::ReadU32(base + 1) == 0x12345678u);
}

__declspec(noinline) int __cdecl Multiply(int a, int b) { return a * b + (a ^ 7); }
__declspec(noinline) int __cdecl Subtract(int a, int b) { return (a - b) * 3 + (b | 5); }

using BinaryFn = int(__cdecl*)(int, int);
BinaryFn g_multiplyOriginal = nullptr;
BinaryFn g_subtractOriginal = nullptr;
int g_detourCalls = 0;

int __cdecl MultiplyDetour(int a, int b) {
    ++g_detourCalls;
    return g_multiplyOriginal(a, b) + 1000;
}

int __cdecl SubtractDetour(int a, int b) {
    ++g_detourCalls;
    return g_subtractOriginal(a, b) - 1000;
}

void TestHooks() {
    using namespace ddmod;
    // Call through volatile pointers so the compiler cannot fold the calls.
    BinaryFn volatile multiply = &Multiply;
    BinaryFn volatile subtract = &Subtract;
    const int plainMultiply = multiply(6, 7);
    const int plainSubtract = subtract(9, 4);

    CHECK(hooks::CreateRaw("early", reinterpret_cast<void*>(&Multiply), nullptr, nullptr) == false);  // before Init
    CHECK(hooks::Init());
    CHECK(hooks::Init());  // idempotent

    CHECK(hooks::Create("multiply", &Multiply, &MultiplyDetour, &g_multiplyOriginal));
    CHECK(!hooks::Create("multiply", &Subtract, &SubtractDetour, &g_subtractOriginal));  // duplicate name
    CHECK(!hooks::CreateRaw("null", nullptr, reinterpret_cast<void*>(&SubtractDetour), nullptr));
    CHECK(multiply(6, 7) == plainMultiply);  // created, not enabled yet

    CHECK(hooks::Enable("multiply"));
    CHECK(hooks::IsEnabled("multiply"));
    CHECK(multiply(6, 7) == plainMultiply + 1000);
    CHECK(g_detourCalls == 1);

    CHECK(hooks::Disable("multiply"));
    CHECK(!hooks::IsEnabled("multiply"));
    CHECK(multiply(6, 7) == plainMultiply);

    CHECK(hooks::Create("subtract", &Subtract, &SubtractDetour, &g_subtractOriginal));
    CHECK(hooks::EnableAll());
    CHECK(multiply(6, 7) == plainMultiply + 1000);
    CHECK(subtract(9, 4) == plainSubtract - 1000);

    CHECK(hooks::Remove("subtract"));
    CHECK(subtract(9, 4) == plainSubtract);
    CHECK(!hooks::Enable("subtract"));  // gone

    hooks::Shutdown();
    CHECK(multiply(6, 7) == plainMultiply);
    CHECK(!hooks::IsEnabled("multiply"));
}

void TestKeyCombo() {
    using namespace ddmod::input;
    const auto parse = [](const char* text) { return ParseKeyCombo(text); };
    const auto is = [](std::optional<KeyCombo> c, std::uint8_t vk, std::uint8_t mods) {
        return c && c->vk == vk && c->mods == mods;
    };

    CHECK(is(parse("Ctrl+Shift+F10"), VK_F10, kCtrl | kShift));
    CHECK(is(parse("shift + ctrl + f10"), VK_F10, kCtrl | kShift));  // order and case do not matter
    CHECK(is(parse("Control+Alt+Delete"), VK_DELETE, kCtrl | kAlt));
    CHECK(is(parse("F1"), VK_F1, kNone));
    CHECK(is(parse("F24"), VK_F24, kNone));
    CHECK(is(parse("a"), 'A', kNone));
    CHECK(is(parse("Alt+7"), '7', kAlt));
    CHECK(is(parse("Num0"), VK_NUMPAD0, kNone));
    CHECK(is(parse("Ctrl+NumAdd"), VK_ADD, kCtrl));
    CHECK(is(parse("PgDn"), VK_NEXT, kNone));
    CHECK(is(parse("  Esc "), VK_ESCAPE, kNone));

    // Unbound.
    CHECK(is(parse(""), 0, kNone));
    CHECK(is(parse("   "), 0, kNone));
    CHECK(is(parse("none"), 0, kNone));
    CHECK(!parse("")->Bound());

    // Syntax errors.
    CHECK(!parse("Ctrl"));            // modifier without key
    CHECK(!parse("Ctrl+"));
    CHECK(!parse("+F1"));
    CHECK(!parse("Ctrl++F1"));
    CHECK(!parse("Ctrl+Ctrl+F1"));    // repeated modifier
    CHECK(!parse("F1+Ctrl"));         // key must come last
    CHECK(!parse("F1+F2"));
    CHECK(!parse("F0"));
    CHECK(!parse("F25"));
    CHECK(!parse("F01"));
    CHECK(!parse("Num10"));
    CHECK(!parse("Win+F1"));
    CHECK(!parse("PrintScreen"));
    CHECK(!parse("?"));
    CHECK(!parse("Ctrl+None"));

    // Canonical text, and round trip.
    CHECK(ToString(KeyCombo{}) == "None");
    CHECK(ToString(*parse("shift+alt+ctrl+f10")) == "Ctrl+Shift+Alt+F10");
    CHECK(ToString(*parse("ctrl+pgup")) == "Ctrl+PageUp");
    CHECK(ToString(*parse("num5")) == "Num5");
    CHECK(ToString(*parse("q")) == "Q");
    CHECK(ToString(KeyCombo{0xE2, kNone}) == "VK_E2");
    for (const char* text : {"Ctrl+Shift+F10", "Alt+Enter", "NumDecimal", "Shift+Z", "ScrollLock", "F24"}) {
        CHECK(ToString(*parse(text)) == text);
    }

    // Matching needs the exact modifier set.
    const KeyCombo hotkey = *parse("Ctrl+Shift+F10");
    CHECK(Matches(hotkey, VK_F10, kCtrl | kShift));
    CHECK(!Matches(hotkey, VK_F10, kCtrl));
    CHECK(!Matches(hotkey, VK_F10, kCtrl | kShift | kAlt));
    CHECK(!Matches(hotkey, VK_F11, kCtrl | kShift));
    CHECK(!Matches(KeyCombo{}, 0, kNone));
}

std::string ReadText(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

void TestLogRotation() {
    namespace fs = std::filesystem;
    using namespace ddmod;
    const fs::path dir = fs::temp_directory_path() / std::format("ddmod_tests_{}", GetCurrentProcessId());
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path file = dir / "ddmod.log";
    const fs::path prev = dir / "ddmod.prev.log";
    CHECK(log::PreviousFile(file) == prev);

    log::Open(file, log::Level::Info);  // nothing to keep yet
    log::Info("first start");
    log::Close();
    CHECK(!fs::exists(prev));

    log::Open(file, log::Level::Info);
    log::Info("second start");
    log::Close();
    CHECK(ReadText(prev).find("first start") != std::string::npos);
    CHECK(ReadText(file).find("second start") != std::string::npos);
    CHECK(ReadText(file).find("first start") == std::string::npos);

    log::Open(file, log::Level::Info);  // only one older copy is kept
    log::Info("third start");
    log::Close();
    CHECK(ReadText(prev).find("second start") != std::string::npos);
    CHECK(ReadText(prev).find("first start") == std::string::npos);
    CHECK(ReadText(file).find("third start") != std::string::npos);

    fs::remove_all(dir);
}

}  // namespace

int main() {
    TestSha256();
    TestPatternParse();
    TestPatternFind();
    TestScanner();
    TestHooks();
    TestKeyCombo();
    TestLogRotation();
    std::printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}

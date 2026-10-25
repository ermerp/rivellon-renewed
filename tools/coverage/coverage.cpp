// ddmod_coverage.asi - developer tool, not shipped. Measures which functions of the host
// executable run at all: at load time it puts a one-shot breakpoint (int3) on every function
// entry listed in ddmod_coverage.in (game folder), and a vectored exception handler records
// the first hit with its time and puts the original byte back. Only memory is changed, never
// the EXE file. Results go to ddmod_coverage.out every second and on unload.
//
// ddmod_coverage.in: first line "timestamp <hex>" = PE TimeDateStamp the list was made for
// (the tool does nothing for another EXE), then one hex address per line. A line
// "defer-window" makes the tool wait for the game window (class "Divinity") before it puts
// the breakpoints in - needed with RivellonRenewed.asi in the same folder, whose signature
// scans and MinHook read the function bytes this tool replaces; start-up code is then missed.
// ddmod_coverage-<start time>.out: "<hex address>\t<ms after load>" per function hit so far.

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <windows.h>

namespace {

struct Entry {
    std::uintptr_t address;
    std::uint8_t original;
    std::atomic<std::uint32_t> hitMs;  // 0 = not hit yet; stored as ms + 1
};

Entry* g_entries = nullptr;
std::size_t g_count = 0;
ULONGLONG g_start = 0;
std::atomic<std::uint32_t> g_hits{0};
std::wstring g_outPath;
HANDLE g_stop = nullptr;

std::wstring GameDir() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring dir = path;
    return dir.substr(0, dir.find_last_of(L"\\/") + 1);
}

Entry* Find(std::uintptr_t address) {
    Entry* end = g_entries + g_count;
    Entry* it = std::lower_bound(g_entries, end, address,
                                 [](const Entry& e, std::uintptr_t a) { return e.address < a; });
    return (it != end && it->address == address) ? it : nullptr;
}

LONG CALLBACK OnException(EXCEPTION_POINTERS* info) {
    if (info->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT) return EXCEPTION_CONTINUE_SEARCH;
    const auto address = reinterpret_cast<std::uintptr_t>(info->ExceptionRecord->ExceptionAddress);
    Entry* e = Find(address);
    if (!e) return EXCEPTION_CONTINUE_SEARCH;
    std::uint32_t expected = 0;
    const auto ms = static_cast<std::uint32_t>(GetTickCount64() - g_start) + 1;
    if (e->hitMs.compare_exchange_strong(expected, ms)) g_hits.fetch_add(1);
    // Another thread may have restored the byte already; writing it again is harmless.
    *reinterpret_cast<volatile std::uint8_t*>(address) = e->original;
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), 1);
    info->ContextRecord->Eip = static_cast<DWORD>(address);
    return EXCEPTION_CONTINUE_EXECUTION;
}

void WriteResults() {
    const std::wstring tmp = g_outPath + L".tmp";
    FILE* f = nullptr;
    if (_wfopen_s(&f, tmp.c_str(), L"w") != 0 || !f) return;
    std::fprintf(f, "# functions %u hit %u\n", static_cast<unsigned>(g_count), g_hits.load());
    for (std::size_t i = 0; i < g_count; ++i) {
        const std::uint32_t ms = g_entries[i].hitMs.load();
        if (ms) std::fprintf(f, "%08X\t%u\n", static_cast<unsigned>(g_entries[i].address), ms - 1);
    }
    std::fclose(f);
    MoveFileExW(tmp.c_str(), g_outPath.c_str(), MOVEFILE_REPLACE_EXISTING);
}

bool g_deferWindow = false;
std::uintptr_t g_textLo = 0, g_textHi = 0;

void PlaceBreakpoints() {
    DWORD old = 0;
    VirtualProtect(reinterpret_cast<void*>(g_textLo), g_textHi - g_textLo, PAGE_EXECUTE_READWRITE, &old);
    for (std::size_t i = 0; i < g_count; ++i) {
        // Re-read the byte now: with defer-window a hook may have rewritten the entry since.
        auto* p = reinterpret_cast<std::uint8_t*>(g_entries[i].address);
        g_entries[i].original = *p;
        if (*p != 0xCC) *p = 0xCC;
    }
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(g_textLo), g_textHi - g_textLo);
}

DWORD WINAPI Writer(void*) {
    if (g_deferWindow) {
        while (!FindWindowW(L"Divinity", nullptr)) {
            if (WaitForSingleObject(g_stop, 200) != WAIT_TIMEOUT) return 0;
        }
        Sleep(2000);
        PlaceBreakpoints();
    }
    std::uint32_t written = ~0u;
    while (WaitForSingleObject(g_stop, 1000) == WAIT_TIMEOUT) {
        if (g_hits.load() != written) {
            written = g_hits.load();
            WriteResults();
        }
    }
    return 0;
}

// Reads the address list; false if it is missing or made for another EXE.
bool LoadList(const std::wstring& path, std::vector<std::uintptr_t>& out) {
    std::ifstream in(path);
    std::string word;
    unsigned long stamp = 0;
    if (!(in >> word >> std::hex >> stamp) || word != "timestamp") return false;
    const auto base = reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr));
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != stamp) return false;
    while (in >> word) {
        if (word == "defer-window") {
            g_deferWindow = true;
            continue;
        }
        out.push_back(std::stoul(word, nullptr, 16));
    }
    return !out.empty();
}

void Start() {
    const std::wstring dir = GameDir();
    std::vector<std::uintptr_t> list;
    if (!LoadList(dir + L"ddmod_coverage.in", list)) return;
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());

    const auto base = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    const auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const IMAGE_SECTION_HEADER* text = IMAGE_FIRST_SECTION(nt);  // .text is the first section
    const auto lo = reinterpret_cast<std::uintptr_t>(base) + text->VirtualAddress;
    const auto hi = lo + text->Misc.VirtualSize;
    const auto entry = reinterpret_cast<std::uintptr_t>(base) + nt->OptionalHeader.AddressOfEntryPoint;

    g_entries = new Entry[list.size()];
    for (std::uintptr_t a : list) {
        if (a < lo || a >= hi || a == entry) continue;
        const std::uint8_t b = *reinterpret_cast<const std::uint8_t*>(a);
        if (b == 0xCC) continue;  // padding or already patched
        Entry& e = g_entries[g_count++];
        e.address = a;
        e.original = b;
        e.hitMs = 0;
    }
    SYSTEMTIME now{};
    GetLocalTime(&now);
    wchar_t stamp[32] = {};
    swprintf_s(stamp, L"%04u%02u%02u-%02u%02u%02u", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
               now.wSecond);
    g_outPath = dir + L"ddmod_coverage-" + stamp + L".out";  // one file per start
    g_start = GetTickCount64();
    g_textLo = lo;
    g_textHi = hi;
    AddVectoredExceptionHandler(1, &OnException);
    if (!g_deferWindow) PlaceBreakpoints();

    g_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    CloseHandle(CreateThread(nullptr, 0, &Writer, nullptr, 0, nullptr));
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        Start();
    } else if (reason == DLL_PROCESS_DETACH && g_entries) {
        WriteResults();
    }
    return TRUE;
}

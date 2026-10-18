// Overlay backend for the Direct3D 6 renderer (slash1.dll), see docs/re/renderer-d3d6.md.
// The frame is reached through CD3D6VideoMem (DllSlashedGetInfo(3)), the object div.exe itself
// locks mid-scene to write 16-bit pixels. Its counted lock always locks the real back buffer;
// the pixel format comes from that surface, because windowed on a 32-bit desktop the back
// buffer is not 16 bit.

#include <cstdint>
#include <string>

#include <ddraw.h>

#include "core/log.h"
#include "core/sha256.h"
#include "overlay/backend.h"

namespace ddmod::overlay {
namespace {

// slash1.dll of 1.0062A (docs/re/game-version.md). The object layout below is build-specific.
constexpr const char* kSlash1Sha256 = "B2590BFE86956F142560558691BEBC489C4F3C0D761C3CEFCD8F75DEBD2EF3A5";

// CD3D6VideoMem (docs/re/renderer-d3d6.md): virtual methods (thiscall) by vtable byte offset ...
constexpr int kLock = 0x04;
constexpr int kUnlock = 0x08;
constexpr int kBase = 0x10;
constexpr int kWidth = 0x1C;
constexpr int kHeight = 0x20;
constexpr int kPitch = 0x24;
// ... and data members by byte offset.
constexpr int kLockCountField = 0x18;
constexpr int kSurfaceField = 0x24;

using GetInfoFn = void*(__cdecl*)(int index);
using VoidMethod = void(__thiscall*)(void* self);
using IntMethod = int(__thiscall*)(void* self);
using PtrMethod = std::uint8_t*(__thiscall*)(void* self);

template <typename Fn>
Fn Method(void* object, int offset) {
    void** vtable = *static_cast<void***>(object);
    return reinterpret_cast<Fn>(vtable[offset / 4]);
}

template <typename T>
T Field(void* object, int offset) {
    return *reinterpret_cast<T*>(static_cast<std::uint8_t*>(object) + offset);
}

bool IsVerifiedSlash1(HMODULE module) {
    wchar_t path[MAX_PATH] = {};
    if (!GetModuleFileNameW(module, path, MAX_PATH)) return false;
    const auto digest = sha256::OfFile(path);
    if (!digest) {
        log::Warn("Overlay: cannot read slash1.dll to verify it");
        return false;
    }
    const std::string hex = sha256::ToHex(*digest);
    if (hex != kSlash1Sha256) {
        log::Warn("Overlay: slash1.dll {} is not the verified build - overlay disabled", hex);
        return false;
    }
    log::Info("Overlay: slash1.dll {} (verified)", hex);
    return true;
}

std::uintptr_t ImageSize(HMODULE module) {
    const auto base = reinterpret_cast<const std::uint8_t*>(module);
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    return nt->OptionalHeader.SizeOfImage;
}

bool FormatOf(IDirectDrawSurface4* surface, PixelFormat& format) {
    DDPIXELFORMAT pf{};
    pf.dwSize = sizeof(pf);
    if (FAILED(surface->GetPixelFormat(&pf)) || !(pf.dwFlags & DDPF_RGB)) return false;
    const auto is = [&pf](DWORD bits, DWORD r, DWORD g, DWORD b) {
        return pf.dwRGBBitCount == bits && pf.dwRBitMask == r && pf.dwGBitMask == g && pf.dwBBitMask == b;
    };
    if (is(16, 0xF800, 0x07E0, 0x001F)) format = PixelFormat::Rgb565;
    else if (is(16, 0x7C00, 0x03E0, 0x001F)) format = PixelFormat::Rgb555;
    else if (is(32, 0xFF0000, 0x00FF00, 0x0000FF)) format = PixelFormat::Xrgb8888;
    else {
        log::Warn("Overlay: unsupported back-buffer format ({} bit, masks {:X}/{:X}/{:X}) - not drawing",
                  pf.dwRGBBitCount, pf.dwRBitMask, pf.dwGBitMask, pf.dwBBitMask);
        return false;
    }
    return true;
}

class D3d6Backend final : public Backend {
public:
    D3d6Backend(HMODULE module, GetInfoFn getInfo)
        : moduleBase_(reinterpret_cast<std::uintptr_t>(module)), moduleSize_(ImageSize(module)), getInfo_(getInfo) {}

    const char* Name() const override { return "Direct3D 6 (slash1.dll)"; }

    bool BeginFrame(SurfaceView& view) override {
        void* videoMem = getInfo_(3);
        if (!videoMem || !Plausible(videoMem)) return false;
        auto* surface = Field<IDirectDrawSurface4*>(videoMem, kSurfaceField);
        if (!surface) return false;
        if (surface != surface_) {
            // New back buffer (start or resolution change): look up its format once.
            surface_ = surface;
            formatOk_ = FormatOf(surface, format_);
            if (formatOk_) {
                log::Info("Overlay: back buffer {}x{} {}", Method<IntMethod>(videoMem, kWidth)(videoMem),
                          Method<IntMethod>(videoMem, kHeight)(videoMem), ToString(format_));
            }
        }
        if (!formatOk_) return false;

        const int countBefore = Field<int>(videoMem, kLockCountField);
        Method<VoidMethod>(videoMem, kLock)(videoMem);
        // A lost surface that cannot be restored leaves the count unchanged and no pixels.
        locked_ = Field<int>(videoMem, kLockCountField) != countBefore;
        std::uint8_t* base = Method<PtrMethod>(videoMem, kBase)(videoMem);
        if (!base) {
            if (locked_) Method<VoidMethod>(videoMem, kUnlock)(videoMem);
            locked_ = false;
            return false;
        }
        videoMem_ = videoMem;
        view.pixels = base;
        view.pitch = Method<IntMethod>(videoMem, kPitch)(videoMem);
        view.width = Method<IntMethod>(videoMem, kWidth)(videoMem);
        view.height = Method<IntMethod>(videoMem, kHeight)(videoMem);
        view.format = format_;
        return true;
    }

    void EndFrame() override {
        if (locked_ && videoMem_) Method<VoidMethod>(videoMem_, kUnlock)(videoMem_);
        locked_ = false;
        videoMem_ = nullptr;
    }

private:
    // The object's vtable must lie inside slash1.dll - guards against a wrong GetInfo index.
    bool Plausible(void* object) {
        const auto vtable = reinterpret_cast<std::uintptr_t>(*static_cast<void**>(object));
        if (vtable - moduleBase_ < moduleSize_) return true;
        if (!warnedVtable_) log::Error("Overlay: GetInfo(3) object has a vtable outside slash1.dll - not drawing");
        warnedVtable_ = true;
        return false;
    }

    std::uintptr_t moduleBase_;
    std::uintptr_t moduleSize_;
    GetInfoFn getInfo_;
    IDirectDrawSurface4* surface_ = nullptr;
    PixelFormat format_ = PixelFormat::Rgb565;
    bool formatOk_ = false;
    bool warnedVtable_ = false;
    void* videoMem_ = nullptr;
    bool locked_ = false;
};

}  // namespace

std::unique_ptr<Backend> CreateD3d6Backend(HMODULE slash1) {
    if (!IsVerifiedSlash1(slash1)) return nullptr;
    const auto getInfo = reinterpret_cast<GetInfoFn>(GetProcAddress(slash1, "DllSlashedGetInfo"));
    if (!getInfo) {
        log::Error("Overlay: slash1.dll has no DllSlashedGetInfo export");
        return nullptr;
    }
    return std::make_unique<D3d6Backend>(slash1, getInfo);
}

}  // namespace ddmod::overlay

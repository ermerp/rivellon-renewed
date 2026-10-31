cmake_minimum_required(VERSION 3.24)

project(ddmod LANGUAGES C CXX)

# Public name of the mod and the stem of every file it puts into the game folder:
# <stem>.asi, <stem>.ini, <stem>.log and the folder <stem>\ (fonts, logs of older starts).
set(MOD_NAME "Rivellon Renewed")
set(MOD_FILE_STEM "RivellonRenewed")
# Release version as players see it (log, overlay banner, release zip RivellonRenewed-<version>.zip).
set(MOD_VERSION "0.01")

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Divine Divinity is a 32-bit game - configure with -A Win32 (or an x86 toolchain).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Static CRT, so the plugin does not depend on a VC++ redistributable (also under Proton).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# Reproducible builds: no time stamps in objects and binaries, so the same commit gives the same
# RivellonRenewed.asi and release zip on every build in the same folder (tools/release/New-Release.ps1;
# anonymous namespace names depend on the absolute source path).
add_compile_options(/Brepro)
add_link_options(/Brepro)

# --- MinHook (BSD-2-Clause), pinned to the commit of release v1.3.4 ---
# Only the sources are fetched; we build them ourselves instead of using MinHook's own
# CMakeLists, so the static CRT and x86 settings match ddmod exactly.
include(FetchContent)
FetchContent_Declare(minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        c3fcafdc10146beb5919319d0683e44e3c30d537  # v1.3.4
    SOURCE_SUBDIR  do-not-add  # skip MinHook's CMakeLists.txt
)
FetchContent_MakeAvailable(minhook)

add_library(minhook STATIC
    ${minhook_SOURCE_DIR}/src/buffer.c
    ${minhook_SOURCE_DIR}/src/hook.c
    ${minhook_SOURCE_DIR}/src/trampoline.c
    ${minhook_SOURCE_DIR}/src/hde/hde32.c
)
target_include_directories(minhook PUBLIC ${minhook_SOURCE_DIR}/include)
target_compile_options(minhook PRIVATE /W3)

# --- stb_truetype (public domain / MIT), pinned commit of the stb repository ---
# Only the header stb_truetype.h is used (outlines of the bundled faces, src/text/ttface.cpp).
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        2c980bb59875b0d32144a71867fbdebb2f77cd20  # 2026-08-01, stb_truetype v1.26
    SOURCE_SUBDIR  do-not-add  # header-only, no CMakeLists.txt
)
FetchContent_MakeAvailable(stb)

# --- ddmod core (shared by the plugin and the unit tests) ---
add_library(ddmod_core STATIC
    src/core/paths.cpp
    src/core/log.cpp
    src/core/config.cpp
    src/core/sha256.cpp
    src/core/pattern.cpp
    src/core/scanner.cpp
    src/core/hooks.cpp
    src/core/patch.cpp
    src/game/exe.cpp
    src/game/configfile.cpp
    src/game/window.cpp
    src/game/fontlog.cpp
    src/input/keys.cpp
    src/input/input.cpp
    src/fonts/fontchain.cpp
    src/fonts/fonthook.cpp
    src/fonts/fontcolors.cpp
    src/fonts/fontmode.cpp
    src/fonts/families.cpp
    src/fonts/familypaint.cpp
    src/fonts/glyphtable.cpp
    src/fonts/spacing.cpp
    src/text/style.cpp
    src/text/face.cpp
    src/text/gdiface.cpp
    src/text/ttface.cpp
    src/text/symbols.cpp
    src/features/dgvoodoo.cpp
    src/features/fog.cpp
    src/features/movement.cpp
    src/features/resolution.cpp
    src/features/shortlabels.cpp
    src/overlay/surface.cpp
    src/overlay/glyphs.cpp
    src/overlay/fnt.cpp
    src/overlay/d3d6.cpp
    src/overlay/banner.cpp
    src/overlay/overlay.cpp
)
target_include_directories(ddmod_core PUBLIC src)
target_include_directories(ddmod_core PRIVATE ${stb_SOURCE_DIR})
target_compile_definitions(ddmod_core PUBLIC
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    DDMOD_VERSION="${MOD_VERSION}"
    DDMOD_NAME="${MOD_NAME}"
    DDMOD_FILE_STEM="${MOD_FILE_STEM}"
)
target_compile_options(ddmod_core PUBLIC /W4 /permissive- /utf-8)
target_link_libraries(ddmod_core PUBLIC minhook)

# --- RivellonRenewed.asi ---
add_library(ddmod SHARED
    src/proxy/dllmain.cpp
)
target_link_libraries(ddmod PRIVATE ddmod_core version)

# Ultimate ASI Loader picks up *.asi files.
set_target_properties(ddmod PROPERTIES OUTPUT_NAME "${MOD_FILE_STEM}" SUFFIX ".asi")

# --- Unit tests (run natively, no game needed): ctest -C Release --test-dir build ---
enable_testing()
add_executable(ddmod_tests tests/main.cpp)
target_link_libraries(ddmod_tests PRIVATE ddmod_core)
# Lets the tests load the bundled font from assets/fonts.
target_compile_definitions(ddmod_tests PRIVATE DDMOD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME ddmod_tests COMMAND ddmod_tests)

# --- Developer tools (not shipped) ---
# Font comparison sheet for design decisions: build/Release/ddmod_fontpreview.exe
add_executable(ddmod_fontpreview tools/fontpreview/main.cpp tools/fontpreview/scale.cpp tools/fontpreview/scene.cpp
               tools/fontpreview/arrows.cpp tools/fontpreview/labels.cpp)
target_link_libraries(ddmod_fontpreview PRIVATE ddmod_core gdiplus)
# Function coverage of div.exe at runtime (docs/re/coverage.md): build/Release/ddmod_coverage.asi
add_library(ddmod_coverage SHARED tools/coverage/coverage.cpp)
target_compile_options(ddmod_coverage PRIVATE /W4 /permissive-)
target_compile_definitions(ddmod_coverage PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
set_target_properties(ddmod_coverage PROPERTIES SUFFIX ".asi")

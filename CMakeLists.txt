cmake_minimum_required(VERSION 3.24)

project(ddmod VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Divine Divinity is a 32-bit game - configure with -A Win32 (or an x86 toolchain).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Static CRT, so the plugin does not depend on a VC++ redistributable (also under Proton).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(ddmod SHARED
    src/proxy/dllmain.cpp
    src/core/paths.cpp
    src/core/log.cpp
    src/core/config.cpp
)

target_include_directories(ddmod PRIVATE src)
target_compile_definitions(ddmod PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    DDMOD_VERSION="${PROJECT_VERSION}"
)
target_compile_options(ddmod PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(ddmod PRIVATE version)

# Ultimate ASI Loader picks up *.asi files.
set_target_properties(ddmod PROPERTIES SUFFIX ".asi")

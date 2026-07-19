cmake_minimum_required(VERSION 3.24)

project(Py4GW VERSION 0.1.0 LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Py4GW currently targets Windows only.")
endif()

if(CMAKE_SIZEOF_VOID_P AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Py4GW must be configured as a 32-bit build. Use -A Win32.")
endif()
# i686-w64-mingw32 cross build (Linux -> 32-bit Windows DLL, run under Wine).
# CMAKE_SYSTEM_NAME=Windows keeps WIN32 true above; this flag gates every
# MSVC-only construct below (force-includes, .lib names, /MP, MSVC-only find).
set(PY4GW_MINGW OFF)
if(MINGW OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND WIN32))
    set(PY4GW_MINGW ON)
    message(STATUS "Py4GW: MinGW cross-compile build enabled.")
    # 32-bit i686 defaults to no SSE; GW itself requires SSE2, and several TUs
    # use <xmmintrin.h>/<emmintrin.h> intrinsics that are always_inline and fail
    # to inline without the ISA enabled.
    add_compile_options(-msse2 -mfpmath=sse)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# GCC/MinGW needs GNU extensions on for the MSVC-isms (anonymous structs,
# __try macro handling via mingw_compat.h, ms_abi bits in headers).
if(PY4GW_MINGW)
    set(CMAKE_CXX_EXTENSIONS ON)
else()
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

if(MSVC)
    # Compile translation units in parallel within each target.
    add_compile_options(/MP)
endif()

# Translate the MSVC force-include flag (/FI<hdr>) used on several targets. On
# MinGW every Py4GW TU force-includes mingw_compat.h (which pulls in
# base/error_handling.h itself), supplying the __try/__except/_ReturnAddress
# shims the MSVC source assumes.
if(PY4GW_MINGW)
    set(PY4GW_FORCE_INCLUDE_ERRH "-include${CMAKE_CURRENT_SOURCE_DIR}/include/mingw_compat.h")
else()
    set(PY4GW_FORCE_INCLUDE_ERRH "/FIbase/error_handling.h")
endif()

# System library names differ between MSVC (import-lib filenames) and MinGW
# (-l names resolved from the cross sysroot).
if(PY4GW_MINGW)
    set(PY4GW_D3D9_LIB   d3d9)
    set(PY4GW_SYS_LIBS
        d3d9 d3dcompiler dbghelp winmm windowscodecs ole32 oleaut32
        gdiplus comctl32 gdi32 user32 shell32 uuid version imm32 dwmapi psapi)
else()
    set(PY4GW_D3D9_LIB   d3d9.lib)
    set(PY4GW_SYS_LIBS
        d3d9.lib d3dcompiler.lib dbghelp.lib winmm.lib windowscodecs.lib
        ole32.lib gdiplus.lib comctl32.lib)
endif()

option(PY4GW_BUILD_IMGUI_DEMO "Compile Dear ImGui demo sources." ON)
option(PY4GW_ENABLE_FREETYPE "Rasterize ImGui fonts with the vendored FreeType (sharper than stb_truetype)." ON)

# Keep the injected DLL beside the runtime payload directories so the module can
# resolve fonts/, scripts/, and offsets/ directly from the repository root.
set(PY4GW_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PY4GW_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PY4GW_RUNTIME_OUTPUT_DIRECTORY}")

foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} "${PY4GW_RUNTIME_OUTPUT_DIRECTORY}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config} "${PY4GW_RUNTIME_OUTPUT_DIRECTORY}")
endforeach()

if(PY4GW_MINGW)
    # Cross build: there is no target-arch Python to probe. The toolchain file
    # pre-seeds the 32-bit headers/import-lib; expose them as the same
    # Python3::Python target the rest of the file links against.
    find_package(Python3 QUIET COMPONENTS Interpreter)  # host interpreter only, optional
    add_library(Python3::Python INTERFACE IMPORTED)
    set_target_properties(Python3::Python PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${PY4GW_PY_INCLUDE}"
        INTERFACE_LINK_LIBRARIES "${PY4GW_PY_LIB}"
    )
else()
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/pybind11)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/minhook)

if(PY4GW_ENABLE_FREETYPE)
    # Vendored FreeType (third_party/freetype), built lean and fully self-contained:
    # every optional external (zlib/bzip2/png/harfbuzz/brotli) is disabled so it pulls
    # in no system library, matching how the rest of third_party/ is vendored. Install
    # rules are suppressed so it never leaks into a Py4GW install.
    set(FT_DISABLE_ZLIB     ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2    ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG      ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI   ON CACHE BOOL "" FORCE)
    set(SKIP_INSTALL_ALL    ON CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third_party/freetype)
endif()

set(IMGUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_dx9.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_win32.cpp
)

if(PY4GW_BUILD_IMGUI_DEMO)
    list(APPEND IMGUI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_demo.cpp)
endif()

if(PY4GW_ENABLE_FREETYPE)
    list(APPEND IMGUI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/misc/freetype/imgui_freetype.cpp)
endif()

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
)
# Do NOT define IMGUI_DISABLE_OBSOLETE_FUNCTIONS for a subset of targets: it
# changes sizeof(ImGuiIO), and any TU compiled with a different setting than
# imgui.cpp fails IMGUI_CHECKVERSION ("Mismatched struct layout!") in Debug.
# It stays off entirely because third_party addons (ImAnim) still use
# obsolete ImGui APIs.
target_compile_definitions(imgui
    PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
)
target_link_libraries(imgui PUBLIC ${PY4GW_D3D9_LIB})

if(PY4GW_ENABLE_FREETYPE)
    # Compile imgui_draw.cpp + imgui_freetype.cpp with IMGUI_ENABLE_FREETYPE so the
    # atlas auto-installs the FreeType loader (imgui_draw.cpp: SetFontLoader on first
    # build). PRIVATE is sufficient: no consumer TU references FreeType directly, and
    # the define does not affect any public ImGui struct layout. freetype's include
    # dirs arrive transitively via its PUBLIC interface.
    target_link_libraries(imgui PUBLIC freetype)
    target_compile_definitions(imgui PRIVATE IMGUI_ENABLE_FREETYPE)
endif()

file(GLOB_RECURSE PY4GW_IMGUI_ADDON_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/imgui/addons/*.cpp
)

set(IMGUI_ADDON_SOURCES
    ${PY4GW_IMGUI_ADDON_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/imgui-filebrowser/ImGuiFileBrowser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/ImAnim/im_anim.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/ImGuiColorTextEdit/TextEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/implot/implot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/implot/implot_items.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/implot/implot_demo.cpp
)

add_library(imgui_addons STATIC ${IMGUI_ADDON_SOURCES})
target_include_directories(imgui_addons
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/imgui-filebrowser
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/ImHotKey
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/imgui_markdown
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/ImAnim
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/imgui_club/imgui_memory_editor
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/ImGuiColorTextEdit
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/implot
)
target_compile_definitions(imgui_addons
    PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
)
target_compile_options(imgui_addons PRIVATE ${PY4GW_FORCE_INCLUDE_ERRH})
target_link_libraries(imgui_addons PUBLIC imgui ${PY4GW_D3D9_LIB})

file(GLOB_RECURSE PY4GW_ALL_SOURCE_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

set(PY4GW_SOURCES ${PY4GW_ALL_SOURCE_FILES})
list(REMOVE_ITEM PY4GW_SOURCES ${PY4GW_IMGUI_ADDON_SOURCES})

# arenanet_texture.cpp is pure MSVC __asm{}/__declspec(naked) ATEX decode that
# GCC can't compile; arenanet_texture_stub.cpp is its MinGW replacement (empty
# AtexDecompress -- texture decompression is unused by the scanner/pathing/bot
# APIs). Exactly one of the two must be in the build to avoid a duplicate
# AtexDecompress symbol.
if(PY4GW_MINGW)
    list(REMOVE_ITEM PY4GW_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GW/textures/arenanet_texture.cpp)
else()
    list(REMOVE_ITEM PY4GW_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GW/textures/arenanet_texture_stub.cpp)
endif()

file(GLOB_RECURSE PY4GW_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${IMGUI_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PY4GW_IMGUI_ADDON_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PY4GW_ALL_SOURCE_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PY4GW_HEADERS})

add_library(Py4GW SHARED ${PY4GW_SOURCES} ${PY4GW_HEADERS})

set_target_properties(Py4GW PROPERTIES
    OUTPUT_NAME "Py4GW"
    PREFIX ""
)

target_include_directories(Py4GW
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/addons/imgui-filebrowser
)

target_compile_definitions(Py4GW
    PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
        PYBIND11_DETAILED_ERROR_MESSAGES
)
target_compile_options(Py4GW PRIVATE ${PY4GW_FORCE_INCLUDE_ERRH})

target_link_libraries(Py4GW
    PRIVATE
        pybind11::embed
        imgui
        imgui_addons
        minhook
        Python3::Python
        ${PY4GW_SYS_LIBS}
)

if(PY4GW_MINGW)
    # Self-contained DLL: static libgcc/libstdc++/winpthread, and tolerate the
    # duplicate pybind11 embedded-module symbols GCC emits without a PCH.
    target_link_options(Py4GW PRIVATE
        -static-libgcc -static-libstdc++
        -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,-Bdynamic,--no-whole-archive
        -Wl,--allow-multiple-definition
    )
    target_compile_options(Py4GW PRIVATE -fpermissive -Wno-attributes)
endif()


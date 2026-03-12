cmake_minimum_required(VERSION 3.16)
project(DocViewer VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# ─────────────────────────────────────────────────────────────
# CRITICAL: set linker flags BEFORE any find_package / target.
#
# --no-as-needed
#   libmupdf.a is a static archive.  Its internal object files
#   (pdf-js.o, load-jpeg.o, xml.o …) pull in symbols from
#   libmujs.so, libjpeg.so, and libgumbo.so.  The GNU ld default
#   flag --as-needed drops a shared library when *our* compiled
#   .o files don't reference it directly — references from inside
#   static archives are invisible to that test.  So even though
#   libmujs.so etc. appear on the link command line, ld discards
#   them, causing every js_* / jpeg_* / gumbo_* to be unresolved.
#   --no-as-needed disables that pruning globally for this target.
# ─────────────────────────────────────────────────────────────
if(UNIX AND NOT APPLE)
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,--no-as-needed")
endif()

# ── Qt6 ──────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Widgets Qml Quick QuickControls2 QuickDialogs2 Concurrent)
qt_standard_project_setup()

# ── PkgConfig ─────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(MUPDF    REQUIRED IMPORTED_TARGET mupdf)
pkg_check_modules(HARFBUZZ REQUIRED IMPORTED_TARGET harfbuzz)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(OPENJP2           IMPORTED_TARGET libopenjp2)
pkg_check_modules(JBIG2DEC          IMPORTED_TARGET jbig2dec)

find_package(ZLIB    REQUIRED)
find_package(Threads REQUIRED)

# ── MuPDF transitive dependencies ────────────────────────────
find_library(LIB_JPEG    NAMES jpeg)
find_library(LIB_MUJS    NAMES mujs)
find_library(LIB_GUMBO   NAMES gumbo)
find_library(LIB_EXTRACT NAMES mupdf-extract extract)
find_library(LIB_Z            NAMES z zlib)
# Brotli — required by static libfreetype.a for WOFF2 font decompression
find_library(LIB_BROTLIDEC    NAMES brotlidec)
find_library(LIB_BROTLICOMMON NAMES brotlicommon)

message(STATUS "")
message(STATUS "── DocViewer dependency resolution ────────────")
message(STATUS "  MuPDF       : ${MUPDF_VERSION}")
message(STATUS "  LIB_JPEG    : ${LIB_JPEG}")
message(STATUS "  LIB_MUJS    : ${LIB_MUJS}")
message(STATUS "  LIB_GUMBO   : ${LIB_GUMBO}")
message(STATUS "  LIB_EXTRACT : ${LIB_EXTRACT}")
message(STATUS "  LIB_Z       : ${LIB_Z}")
message(STATUS "  LIB_BROTLI  : ${LIB_BROTLIDEC}  ${LIB_BROTLICOMMON}")
message(STATUS "───────────────────────────────────────────────")
message(STATUS "")

foreach(_req LIB_JPEG LIB_MUJS LIB_GUMBO LIB_Z LIB_BROTLIDEC)
    if(NOT ${_req})
        message(FATAL_ERROR
            "${_req} not found. Run:\n"
            "  sudo apt install libjpeg-dev libmujs-dev libgumbo-dev zlib1g-dev\n")
    endif()
endforeach()

# ── Source files ──────────────────────────────────────────────
set(SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.hpp
    file_format.cpp
    file_format.hpp
)

# Stub out extract_* symbols when libmupdf-extract is not installed.
# DOCX export will fail gracefully at runtime; everything else works.
if(NOT LIB_EXTRACT)
    message(STATUS "  ** libmupdf-extract missing — compiling stub (no DOCX export)")
    list(APPEND SOURCES mupdf_extract_stub.c)
endif()

# ── Executable ───────────────────────────────────────────────
qt_add_executable(DocViewer ${SOURCES})

target_include_directories(DocViewer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(DocViewer PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
            -Wall -Wextra -Wpedantic -Wno-deprecated-declarations>>)

# ── Link ──────────────────────────────────────────────────────
# Static archive order rule: libmupdf.a BEFORE its dependencies.
target_link_libraries(DocViewer PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets
    Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::QuickDialogs2
    Qt6::Concurrent

    PkgConfig::MUPDF      # static archive — must be first

    ${LIB_JPEG}           # jpeg_*   (needed by libmupdf.a)
    ${LIB_MUJS}           # js_*     (needed by libmupdf.a)
    ${LIB_GUMBO}          # gumbo_*  (needed by libmupdf.a)
    $<$<BOOL:${LIB_EXTRACT}>:${LIB_EXTRACT}>   # extract_*

    $<$<BOOL:${OPENJP2_FOUND}>:PkgConfig::OPENJP2>
    $<$<BOOL:${JBIG2DEC_FOUND}>:PkgConfig::JBIG2DEC>

    PkgConfig::HARFBUZZ
    PkgConfig::FREETYPE

    # Brotli — needed by static libfreetype.a (WOFF2 fonts)
    ${LIB_BROTLIDEC}
    $<$<BOOL:${LIB_BROTLICOMMON}>:${LIB_BROTLICOMMON}>

    ZLIB::ZLIB
    ${LIB_Z}

    Threads::Threads
)

if(UNIX AND NOT APPLE)
    target_link_libraries(DocViewer PRIVATE m dl)
endif()

install(TARGETS DocViewer RUNTIME DESTINATION bin BUNDLE DESTINATION .)

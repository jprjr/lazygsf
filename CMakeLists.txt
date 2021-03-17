cmake_minimum_required(VERSION 3.1)
cmake_policy(SET CMP0063 NEW)

project(gsfplayer)

set(GSFPLAYER_VERSION_MAJOR 0)
set(GSFPLAYER_VERSION_MINOR 0)
set(GSFPLAYER_VERSION_PATCH 0)
set(GSFPLAYER_VERSION_STRING ${GSFPLAYER_VERSION_MAJOR}.${GSFPLAYER_VERSION_MINOR}.${GSFPLAYER_VERSION_PATCH})

include(CheckFunctionExists)
include(GNUInstallDirs)

set(OS_LIB)

check_function_exists(snprintf_l HAVE_SNPRINTF_L)
check_function_exists(newlocale HAVE_NEWLOCALE)
check_function_exists(freelocale HAVE_FREELOCALE)
check_function_exists(uselocale HAVE_USELOCALE)
check_function_exists(setlocale HAVE_SETLOCALE)
if(HAVE_NEWLOCALE AND HAVE_FREELOCALE AND HAVE_USELOCALE OR APPLE)
	list(APPEND FUNCTION_DEFINES HAVE_LOCALE)
	if (HAVE_SNPRINTF_L)
		list(APPEND FUNCTION_DEFINES HAVE_SNPRINTF_L)
	endif()
endif()

set(MGBA_SOURCES
	mgba/src/arm/arm.c
	mgba/src/arm/decoder-arm.c
	mgba/src/arm/decoder.c
	mgba/src/arm/decoder-thumb.c
	mgba/src/arm/isa-arm.c
	mgba/src/arm/isa-thumb.c
    mgba/src/core/bitmap-cache.c
    mgba/src/core/cache-set.c
    mgba/src/core/cheats.c
    mgba/src/core/config.c
    mgba/src/core/core.c
    mgba/src/core/interface.c
    mgba/src/core/log.c
    mgba/src/core/map-cache.c
    mgba/src/core/mem-search.c
    mgba/src/core/serialize.c
    mgba/src/core/sync.c
    mgba/src/core/tile-cache.c
    mgba/src/core/timing.c
	mgba/src/util/circle-buffer.c
	mgba/src/util/configuration.c
	mgba/src/util/crc32.c
	mgba/src/util/formatting.c
	mgba/src/util/gbk-table.c
	mgba/src/util/hash.c
    mgba/src/util/memory.c
	mgba/src/util/patch.c
	mgba/src/util/patch-ips.c
	mgba/src/util/patch-ups.c
	mgba/src/util/string.c
	mgba/src/util/table.c
	mgba/src/util/vfs.c
	mgba/src/util/vfs/vfs-mem.c
	mgba/src/util/vfs/vfs-fd.c
	mgba/src/third-party/inih/ini.c
	mgba/src/third-party/blip_buf/blip_buf.c
    mgba/src/gb/audio.c
	mgba/src/gba/audio.c
	mgba/src/gba/bios.c
	mgba/src/gba/cheats.c
	mgba/src/gba/cheats/codebreaker.c
	mgba/src/gba/cheats/gameshark.c
	mgba/src/gba/cheats/parv3.c
	mgba/src/gba/core.c
	mgba/src/gba/dma.c
	mgba/src/gba/ereader.c
	mgba/src/gba/gba.c
	mgba/src/gba/hardware.c
	mgba/src/gba/hle-bios.c
	mgba/src/gba/io.c
	mgba/src/gba/matrix.c
	mgba/src/gba/memory.c
	mgba/src/gba/overrides.c
	mgba/src/gba/renderers/cache-set.c
	mgba/src/gba/renderers/common.c
	mgba/src/gba/renderers/software-bg.c
	mgba/src/gba/renderers/software-mode0.c
	mgba/src/gba/renderers/software-obj.c
	mgba/src/gba/renderers/video-software.c
	mgba/src/gba/savedata.c
	mgba/src/gba/serialize.c
	mgba/src/gba/sio.c
	mgba/src/gba/timer.c
	mgba/src/gba/vfame.c
	mgba/src/gba/video.c
)

set(PSFLIB_SOURCES
    psflib/psflib.c
    psflib/psf2fs.c
)

set(MINIZ_SOURCES
    miniz/miniz.c
)

set(GSFPLAYER_SOURCES src/gsfplayer.c)

if(WIN32)
    list(APPEND OS_LIB ws2_32 shlwapi)
elseif(UNIX)
    list(APPEND OS_LIB m)
    #if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    #    add_definitions(-D_GNU_SOURCE)
    #endif()
endif()

set(MINIZ_DEFS)
list(APPEND MINIZ_DEFS MINIZ_NO_STDIO)
list(APPEND MINIZ_DEFS MINIZ_NO_ARCHIVE_APIS)

add_library(gsfplayer ${MGBA_SOURCES} ${PSFLIB_SOURCES} ${MINIZ_SOURCES} ${GSFPLAYER_SOURCES})
target_link_libraries(gsfplayer PUBLIC ${OS_LIB})
target_include_directories(gsfplayer PRIVATE mgba/include mgba/src psflib)
target_include_directories(gsfplayer PUBLIC
  $<INSTALL_INTERFACE:src>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(gsfplayer PROPERTIES COMPILE_DEFINITIONS "${FUNCTION_DEFINES};DISABLE_THREADING;M_CORE_GBA;MINIMAL_CORE=2;${MINIZ_DEFS}")
set_target_properties(gsfplayer PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(gsfplayer PROPERTIES C_VISIBILITY_PRESET hidden)
set_target_properties(gsfplayer PROPERTIES VISIBILITY_INLINES_HIDDEN ON)
set_target_properties(gsfplayer PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(gsfplayer PROPERTIES VERSION ${GSFPLAYER_VERSION_STRING} SOVERSION ${GSFPLAYER_VERSION_MAJOR}.${GSFPLAYER_VERSION_MINOR})

if(MSVC)
    target_compile_options(gsfplayer PRIVATE /W4 /WX)
else()
    target_compile_options(gsfplayer PRIVATE -Wall -Wextra)
endif()

add_executable(gsf2wav gsf2wav.c)
target_link_libraries(gsf2wav PRIVATE gsfplayer)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gsfplayer.pc.in ${CMAKE_BINARY_DIR}/gsfplayer.pc @ONLY)

install(TARGETS gsfplayer gsf2wav
    EXPORT gsfplayer-export
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/gsfplayer.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/gsfplayer.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

cmake_minimum_required(VERSION 3.1)
cmake_policy(SET CMP0063 NEW)

project(lazygsf)

set(LAZYGSF_VERSION_MAJOR 0)
set(LAZYGSF_VERSION_MINOR 0)
set(LAZYGSF_VERSION_PATCH 0)
set(LAZYGSF_VERSION_STRING ${LAZYGSF_VERSION_MAJOR}.${LAZYGSF_VERSION_MINOR}.${LAZYGSF_VERSION_PATCH})

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
	mgba/src/gba/cart/ereader.c
	mgba/src/gba/cart/gpio.c
	mgba/src/gba/cart/matrix.c
	mgba/src/gba/cart/vfame.c
	mgba/src/gba/cheats.c
	mgba/src/gba/cheats/codebreaker.c
	mgba/src/gba/cheats/gameshark.c
	mgba/src/gba/cheats/parv3.c
	mgba/src/gba/core.c
	mgba/src/gba/dma.c
	mgba/src/gba/gba.c
	mgba/src/gba/hle-bios.c
	mgba/src/gba/io.c
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
	mgba/src/gba/sio/gbp.c
	mgba/src/gba/timer.c
	mgba/src/gba/video.c
)

set(LAZYGSF_SOURCES src/lazygsf.c)
if(BUILD_SHARED_LIBS)
    if(MSVC)
        list(APPEND LAZYGSF_SOURCES src/lazygsf.def)
    endif()
endif()

if(WIN32)
    list(APPEND OS_LIB ws2_32 shlwapi)
elseif(UNIX)
    list(APPEND OS_LIB m)
    #if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    #    add_definitions(-D_GNU_SOURCE)
    #endif()
endif()


add_library(lazygsf ${MGBA_SOURCES} ${LAZYGSF_SOURCES})
target_link_libraries(lazygsf PUBLIC ${OS_LIB})
target_include_directories(lazygsf PRIVATE mgba/include mgba/src)
target_include_directories(lazygsf PUBLIC
  $<INSTALL_INTERFACE:src>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)
set_target_properties(lazygsf PROPERTIES COMPILE_DEFINITIONS "${FUNCTION_DEFINES};DISABLE_THREADING;M_CORE_GBA;MINIMAL_CORE=2;BUILD_STATIC;${BUILD_DEF}")
set_target_properties(lazygsf PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(lazygsf PROPERTIES C_VISIBILITY_PRESET hidden)
set_target_properties(lazygsf PROPERTIES VISIBILITY_INLINES_HIDDEN ON)
set_target_properties(lazygsf PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(lazygsf PROPERTIES VERSION ${LAZYGSF_VERSION_STRING} SOVERSION ${LAZYGSF_VERSION_MAJOR}.${LAZYGSF_VERSION_MINOR})

if(MSVC)
    target_compile_options(lazygsf PRIVATE /W3)
else()
    target_compile_options(lazygsf PRIVATE -Wall -Wextra)
endif()

if(NOT BUILD_SHARED_LIBS)
    if(NOT MSVC)
        string(REPLACE " " ";" ORIGINAL_C_FLAGS "${CMAKE_C_FLAGS}")
        add_custom_command(
          TARGET lazygsf
          POST_BUILD
          COMMAND
          ${CMAKE_C_COMPILER} ${ORIGINAL_C_FLAGS} -fPIC
          -nostartfiles -nodefaultlibs -r
          -Wl,--whole-archive $<TARGET_FILE:lazygsf> -Wl,--no-whole-archive
          -o $<TARGET_FILE:lazygsf>.o &&
          ${CMAKE_OBJCOPY} --localize-hidden $<TARGET_FILE:lazygsf>.o &&
          rm -f $<TARGET_FILE:lazygsf> &&
          ${CMAKE_AR} cr $<TARGET_FILE:lazygsf> $<TARGET_FILE:lazygsf>.o &&
          ${CMAKE_RANLIB} $<TARGET_FILE:lazygsf>
          VERBATIM
        )
    endif() # TODO find a way to do this with MSVC tools
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/lazygsf.pc.in ${CMAKE_BINARY_DIR}/lazygsf.pc @ONLY)

install(TARGETS lazygsf
    EXPORT lazygsf-export
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/lazygsf.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lazygsf.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

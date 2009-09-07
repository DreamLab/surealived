cmake_minimum_required(VERSION 2.6)
add_definitions("-Wall -Werror -ggdb")

include(FindPkgConfig)
pkg_search_module(GLIB2 REQUIRED glib-2.0)

message(status "pkg-status: " ${PKG_CONFIG_FOUND})
message(status "pkg-exec: " ${PKG_CONFIG_EXECUTABLE})
message(status "glib2: " ${GLIB2_VERSION})
message(status "glib2: " ${GLIB2_CFLAGS})


if (${GLIB2_FOUND})
	MESSAGE(STATUS "\nglib2.0-dev found. OK\n")
else() 
	MESSAGE(FATAL_ERROR "\nCan't find glib2.0-dev\n")
endif()
add_definitions(${GLIB2_CFLAGS})
include_directories(../common)

set(ipvssyncSrcs ipvssync.c ../common/logging.c maincfg.c config.c ipvsfuncs.c diffs.c)
add_executable(ipvssync ${ipvssyncSrcs})
add_library(ipvs STATIC ../libipvs/libipvs.c)
add_definitions("-DHAVE_NET_IP_VS_H")
target_link_libraries(ipvssync ipvs glib-2.0)
include_directories(/usr/src/linux/include . ../libipvs)

install(TARGETS ipvssync 
  DESTINATION "/usr/sbin")

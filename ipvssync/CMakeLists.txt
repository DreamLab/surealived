cmake_minimum_required(VERSION 2.6)
#add_definitions("-Wall -Werror -ggdb")
add_definitions("-Wall -Werror")

message(STATUS "\n=== configuring ipvssync ===")

include(FindPkgConfig)

pkg_search_module(GLIB2 REQUIRED glib-2.0)
if (NOT ${GLIB2_FOUND})
  MESSAGE(FATAL_ERROR "\nCan't find glib2.0-dev\n")
endif()

add_definitions(${GLIB2_CFLAGS})
include_directories(../common)

set(ipvssyncSrcs 
  ../common/logging.c 
  ipvssync.c 
  maincfg.c 
  config.c 
  ipvsfuncs.c 
  diffs.c
)

add_executable(ipvssync ${ipvssyncSrcs})
add_library(ipvs STATIC ../libipvs/libipvs.c)
add_definitions("-DHAVE_NET_IP_VS_H")
target_link_libraries(ipvssync ipvs glib-2.0)
include_directories(/usr/src/linux/include . ../libipvs)

install(TARGETS ipvssync 
  DESTINATION "/usr/sbin")

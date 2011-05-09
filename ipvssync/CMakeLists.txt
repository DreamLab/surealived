PROJECT(ipvssync C)
cmake_minimum_required(VERSION 2.6)
#add_definitions("-Wall -Werror -ggdb")
add_definitions("-Wall -Werror -Wformat=2")
add_definitions("-D_FILE_OFFSET_BITS=64 -D__EXPORTED_HEADERS__")

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

IF (CMAKE_SYSTEM_PROCESSOR MATCHES "i.86")
    SET (PROCTYPE "x86")
ELSE (CMAKE_SYSTEM_PROCESSOR MATCHES "i.86")
    IF (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        SET (PROCTYPE "x86")
    ELSE (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        SET (PROCTYPE "${CMAKE_SYSTEM_PROCESSOR}")
    ENDIF (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
ENDIF (CMAKE_SYSTEM_PROCESSOR MATCHES "i.86")

include_directories("/lib/modules/${CMAKE_SYSTEM_VERSION}/build/include")
include_directories("/lib/modules/${CMAKE_SYSTEM_VERSION}/build/include/net")
include_directories("/lib/modules/${CMAKE_SYSTEM_VERSION}/build/arch/${PROCTYPE}/include")
include_directories("." "../libipvs")

install(TARGETS ipvssync 
  DESTINATION "/usr/sbin")

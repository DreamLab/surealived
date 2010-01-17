PROJECT(surealived C)
cmake_minimum_required(VERSION 2.6)
#add_definitions("-Werror -Wall -ggdb -pg")
add_definitions("-Werror -Wall -Wformat=2 -ggdb")
add_definitions("-D_FILE_OFFSET_BITS=64")

message(STATUS "\n=== configuring surealived ===")

include(FindPkgConfig)

pkg_search_module(GLIB2 REQUIRED glib-2.0)
if (NOT ${GLIB2_FOUND})
  MESSAGE(FATAL_ERROR "\nCan't find glib2.0-dev\n")
endif()

pkg_search_module(GMODULE2 REQUIRED gmodule-2.0)
if (NOT ${GMODULE2_FOUND})
	MESSAGE(FATAL_ERROR "\nCan't find gmodule2.0\n")
endif()

pkg_search_module(LIBXML2 REQUIRED libxml-2.0)
if (NOT ${LIBXML2_FOUND})
	MESSAGE(FATAL_ERROR "\nCan't find libxml2-dev\n")
endif()

pkg_search_module(OPENSSL REQUIRED openssl)
if (NOT ${OPENSSL_FOUND})
	MESSAGE(FATAL_ERROR "\nCan't find libssl-dev\n")
endif()

# === ip_vs.h ===
FIND_PATH(IPVS 
	net/ip_vs.h
	/lib/modules/${CMAKE_SYSTEM_VERSION}/build/include/
	)
IF (IPVS)
   MESSAGE(STATUS "Kernel net/ip_vs.h header = ${IPVS}/net/ip_vs.h")
ELSE (IPVS) 
   MESSAGE(FATAL_ERROR "Can't find ip_vs.h header")
ENDIF (IPVS)

add_definitions(${GLIB2_CFLAGS})
add_definitions(${GMODULE2_CFLAGS})
add_definitions(${LIBXML2_CFLAGS})
add_definitions(${OPENSSL_CFLAGS})

add_library(sdutil STATIC 
  ../common/logging.c 
  common.c 
  sd_maincfg.c 
  sd_socket.c 
  sd_epoll.c 
  sd_tester.c   
  sd_log.c 
  sd_offline.c 
  sd_override.c 
  sd_ipvssync.c 
  sd_cmd.c
)

set(surealivedSrcs 
  surealived.c 
  modloader.c 
  xmlparser.c
)

add_executable(surealived ${surealivedSrcs})
target_link_libraries(surealived 
  sdutil 
  ${GLIB2_LIBRARIES}
  ${GMODULE2_LIBRARIES}
  ${LIBXML2_LIBRARIES} 
  ${OPENSSL_LIBRARIES}
)

#set_target_properties(surealived PROPERTIES LINK_FLAGS "-pg")

include_directories(.)
include_directories(../common)

add_subdirectory(modules)

install(TARGETS surealived 
  DESTINATION "/usr/bin")

install(FILES surealived.cfg
  DESTINATION "/etc/surealived")

install(DIRECTORY scripts/
  DESTINATION "/usr/lib/surealived/scripts"
  PATTERN ".svn" EXCLUDE
  PATTERN "scripts/*.pl"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  PATTERN "scripts/*.lua"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

install(CODE "MESSAGE(STATUS \"Creating directories /var/log/surealived[/comm]\")")
install(CODE "FILE(MAKE_DIRECTORY /var/log/surealived/comm)")
install(CODE "MESSAGE(STATUS \"Creating directories /var/lib/surealived[/stats|/diffs]\")")
install(CODE "FILE(MAKE_DIRECTORY /var/lib/surealived/stats)")
install(CODE "FILE(MAKE_DIRECTORY /var/lib/surealived/diffs)")

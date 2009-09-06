cmake_minimum_required(VERSION 2.6)
add_definitions("-Werror -Wall -ggdb -pg")

include(FindPkgConfig)
pkg_search_module(GLIB2 REQUIRED glib-2.0)
pkg_search_module(GLIBMODULE2 REQUIRED gmodule-2.0)
pkg_search_module(LIBXML2 REQUIRED libxml-2.0)
pkg_search_module(OPENSSL REQUIRED openssl)

message(status "pkg-status: " ${PKG_CONFIG_FOUND})
message(status "pkg-exec: " ${PKG_CONFIG_EXECUTABLE})
message(status "glib2: " ${GLIB2_VERSION})
message(status "glib2: " ${GLIB2_CFLAGS})
message(status "gmodule2: " ${GMODULE2_VERSION})
message(status "gmodule2: " ${GMODULE2_CFLAGS})
message(status "libxml2: " ${LIBXML2_VERSION})
message(status "libxml2: " ${LIBXML2_CFLAGS})
message(status "openssl: " ${OPENSSL_VERSION})
message(status "openssl: " ${OPENSSL_CFLAGS})

if (${GLIB2_FOUND})
	MESSAGE(STATUS "\nglib2.0-dev found. OK\n")
else() 
	MESSAGE(FATAL_ERROR "\nCan't find glib2.0-dev\n")
endif()
add_definitions(${GLIB2_CFLAGS})
add_definitions(${GMODULE2_CFLAGS})
add_definitions(${LIBXML2_CFLAGS})
add_definitions(${OPENSSL_CFLAGS})

add_library(sdutil STATIC ../common/logging.c sd_socket.c sd_epoll.c sd_tester.c common.c 
  sd_log.c sd_offline.c sd_override.c sd_ipvssync.c sd_cmd.c)

set(surealivedSrcs surealived.c common.c sd_maincfg.c modloader.c xmlparser.c)
add_executable(surealived ${surealivedSrcs})
target_link_libraries(surealived sdutil glib-2.0 gmodule-2.0 ${LIBXML2_LIBRARIES} ${OPENSSL_LIBRARIES})
set_target_properties(surealived PROPERTIES LINK_FLAGS "-pg")

include_directories(.)
include_directories(../common)

add_subdirectory(modules)

install(TARGETS surealived 
  DESTINATION "/usr/bin")

install(FILES surealived.cfg
  DESTINATION "/etc/surealived")
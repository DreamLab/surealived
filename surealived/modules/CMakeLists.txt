cmake_minimum_required(VERSION 2.6)
add_definitions("-Wall -Werror -ggdb")

include(FindPkgConfig)
pkg_search_module(GLIB2 REQUIRED glib-2.0)
pkg_search_module(GLIBMODULE2 REQUIRED gmodule-2.0)

message(status "pkg-status: " ${PKG_CONFIG_FOUND})
message(status "pkg-exec: " ${PKG_CONFIG_EXECUTABLE})
message(status "glib2: " ${GLIB2_VERSION})
message(status "glib2: " ${GLIB2_CFLAGS})
message(status "gmodule2: " ${GMODULE2_VERSION})
message(status "gmodule2: " ${GMODULE2_CFLAGS})

if (${GLIB2_FOUND})
	MESSAGE(STATUS "\nglib2.0-dev found. OK\n")
else() 
	MESSAGE(FATAL_ERROR "\nCan't find glib2.0-dev\n")
endif()
add_definitions(${GLIB2_CFLAGS})
add_definitions(${GMODULE2_CFLAGS})

#add_library(mod_tcp SHARED mod_tcp.c)
#set_target_properties(mod_tcp PROPERTIES PREFIX "")

add_library(mod_http SHARED mod_http.c)
add_library(mod_tcp SHARED mod_tcp.c)
add_library(mod_dns SHARED mod_dns.c)
add_library(mod_exec SHARED mod_exec.c)

set_target_properties(mod_http PROPERTIES PREFIX "")
set_target_properties(mod_tcp PROPERTIES PREFIX "")
set_target_properties(mod_dns PROPERTIES PREFIX "")
set_target_properties(mod_exec PROPERTIES PREFIX "")
include_directories(.)
include_directories(../../common)

install(TARGETS mod_http mod_tcp mod_dns mod_exec
  DESTINATION "/usr/lib/surealived/modules")

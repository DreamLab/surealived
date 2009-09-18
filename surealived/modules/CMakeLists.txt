cmake_minimum_required(VERSION 2.6)
#add_definitions("-Wall -Werror -ggdb")
add_definitions("-Wall -Werror")

message(STATUS "\n=== configuring modules ===")

include(FindPkgConfig)
pkg_search_module(GLIB2 REQUIRED glib-2.0)
if (NOT ${GLIB2_FOUND})
  MESSAGE(FATAL_ERROR "\nCan't find glib2.0-dev\n")
endif()

pkg_search_module(GLIBMODULE2 REQUIRED gmodule-2.0)
if (NOT ${GLIBMODULE2_FOUND})
	MESSAGE(FATAL_ERROR "\nCan't find gmodule2.0\n")
endif()

add_definitions(${GLIB2_CFLAGS})
add_definitions(${GMODULE2_CFLAGS})

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

install(TARGETS 
  mod_http 
  mod_tcp 
  mod_dns 
  mod_exec
  DESTINATION "/usr/lib/surealived/modules")

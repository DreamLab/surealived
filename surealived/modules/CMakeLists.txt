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

include(FindLua51)
MESSAGE(STATUS "LUA_INCLUDE_DIR ${LUA_INCLUDE_DIR}")
MESSAGE(STATUS "LUA_LIBRARIES ${LUA_LIBRARIES}")
if (LUA_INCLUDE_DIR)
  MESSAGE(STATUS "\nlua5.1 found, setting LUA_FOUND to TRUE\n")
  SET (LUA_FOUND "TRUE")
endif()
MESSAGE(STATUS "LUA_FOUND ${LUA_FOUND}")

if (NOT LUA_FOUND)
  MESSAGE(STATUS "\nCan't find lua5.1, skipping building mod_lua\n")
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

if (LUA_FOUND)
  MESSAGE(STATUS "\nCompiling and linking mod_lua (5.1)")
  MESSAGE(STATUS "LUA_INCLUDE_DIR ${LUA_INCLUDE_DIR}")
  MESSAGE(STATUS "LUA_LIBRARIES ${LUA_LIBRARIES}")
  add_library(mod_lua SHARED mod_lua.c)
  set_target_properties(mod_lua PROPERTIES PREFIX "")
  set_target_properties(mod_lua PROPERTIES COMPILE_FLAGS "-I${LUA_INCLUDE_DIR}")
  target_link_libraries(mod_lua lua5.1)
endif()

set_target_properties(mod_exec PROPERTIES PREFIX "")
include_directories(.)
include_directories(../../common)

install(TARGETS 
  mod_http 
  mod_tcp 
  mod_dns 
  mod_exec
  DESTINATION "/usr/lib/surealived/modules")

if (LUA_FOUND)
  install(TARGETS 
  mod_lua
  DESTINATION "/usr/lib/surealived/modules")
endif()

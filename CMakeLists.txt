cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0048 NEW)

PROJECT(SureAliveD
        VERSION "1.0.6"
        LANGUAGES CXX)

add_subdirectory(ipvssync)
add_subdirectory(surealived)

SET(CPACK_PACKAGE_VERSION "1.0.6")
SET(CPACK_PACKAGE_NAME "surealived")

SET(CPACK_GENERATOR "DEB")
SET(CPACK_DEBIAN_PACKAGE_NAME "surealived")
SET(CPACK_DEBIAN_PACKAGE_VERSION "1.0.6")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Dominik Mostowiec <dmostowiec@dreamlab.pl>") #required
SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
SET(CPACK_DEBIAN_PACKAGE_DEPENDS "liblua5.1-0, libssl1.1, libxml2 (>=2.9.3), libglib2.0-0")
SET(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Monitoring tool for LVS real servers.")
SET(CPACK_DEBIAN_PACKAGE_SECTION "admin")
SET(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/DreamLab/surealived")

INCLUDE(CPack)

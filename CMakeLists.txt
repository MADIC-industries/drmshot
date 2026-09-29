cmake_minimum_required(VERSION 3.16)
project(drmshot C)

include(GNUInstallDirs)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDRM REQUIRED IMPORTED_TARGET libdrm)

add_executable(drmshot drmshot.c)
target_link_libraries(drmshot PRIVATE PkgConfig::LIBDRM)

install(TARGETS drmshot RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

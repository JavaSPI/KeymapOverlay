cmake_minimum_required(VERSION 3.10)
project(keymap_overlay)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
# Find required libraries
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(XTST REQUIRED xtst)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(LIBEVDEV REQUIRED libevdev)
pkg_check_modules(RSVG REQUIRED librsvg-2.0)

include_directories(
  ${X11_INCLUDE_DIRS} ${XTST_INCLUDE_DIRS} ${CAIRO_INCLUDE_DIRS}
  ${RSVG_INCLUDE_DIRS} ${LIBEVDEV_INCLUDE_DIRS})

configure_file(${CMAKE_SOURCE_DIR}/keymaps/keymap.svg
               ${CMAKE_BINARY_DIR}/keymaps/keymap.svg COPYONLY)

link_directories(${X11_LIBRARY_DIRS} ${XTST_LIBRARY_DIRS} ${CAIRO_LIBRARY_DIRS}
                 ${RSVG_LIBRARY_DIRS})

add_executable(keymap_overlay keymap_overlay.cpp)

target_link_libraries(
  keymap_overlay
  ${X11_LIBRARIES}
  ${XTST_LIBRARIES}
  ${CAIRO_LIBRARIES}
  ${RSVG_LIBRARIES}
  Xi
  ${LIBEVDEV_LIBRARIES})

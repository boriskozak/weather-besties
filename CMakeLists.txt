cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# LVGL is configured only by components/lv_conf.h (shared with the desktop renderer in sim/).
idf_build_set_property(COMPILE_DEFINITIONS "LV_KCONFIG_IGNORE" APPEND)

# The image's own version (checked by over-the-air updates) comes from WB_VERSION in main/net.h.
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/main/net.h" _wb_ver REGEX "#define WB_VERSION ")
string(REGEX MATCH "\"([^\"]+)\"" _wb_ver_q "${_wb_ver}")
set(PROJECT_VER "${CMAKE_MATCH_1}")
# Re-configure when the version changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/main/net.h")

project(weather_besties)

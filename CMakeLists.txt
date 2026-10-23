cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# LVGL is configured only by components/lv_conf.h (shared with the desktop renderer in sim/).
idf_build_set_property(COMPILE_DEFINITIONS "LV_KCONFIG_IGNORE" APPEND)

project(weather_besties)

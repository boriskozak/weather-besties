# Shared UI, weather parsing and advice logic (also compiled by sim/ on a PC).
include(${CMAKE_CURRENT_LIST_DIR}/assets/assets_list.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/assets/pippa_assets_list.cmake)

idf_component_register(
    SRCS "ui.c" "wx.c" "cat.c" "wb_request.c" "outfit.c" "cal.c" "update.c" ${WB_ASSET_SRCS} ${WB_PIPPA_ASSET_SRCS}
    INCLUDE_DIRS "."
    REQUIRES lvgl json)

target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-format-truncation)

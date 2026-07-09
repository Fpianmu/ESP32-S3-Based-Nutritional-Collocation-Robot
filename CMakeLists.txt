# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

#set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/main/lv_conf.h)
add_compile_options("-DLV_CONF_PATH=\"${CMAKE_SOURCE_DIR}/lv_conf.h\"")
add_compile_options("-Wno-macro-redefined" "-Wno-error=incompatible-pointer-types" "-Wno-error=int-conversion")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(project)

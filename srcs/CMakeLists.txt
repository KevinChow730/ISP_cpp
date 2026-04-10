cmake_minimum_required (VERSION 3.5)

project(modules)

include_directories(../thirdparty/easylog)
include_directories(../thirdparty/json/nlohmann)
include_directories(include)

aux_source_directory(sources ISP_MOD_SRCS_LISTS)

add_library(${PROJECT_NAME} ${ISP_MOD_SRCS_LISTS})

target_link_libraries(${PROJECT_NAME} easylog)
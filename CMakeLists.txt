cmake_minimum_required(VERSION 3.15)

project(IN1061_IoT C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

find_path(CJSON_INCLUDE_DIR
    NAMES cjson/cJSON.h
    PATHS
        C:/msys64/ucrt64/include
        C:/msys64/ucrt64/include/cjson
)

find_library(CJSON_LIBRARY
    NAMES cjson
    PATHS
        C:/msys64/ucrt64/lib
)

if(NOT CJSON_INCLUDE_DIR)
    message(FATAL_ERROR "cJSON não encontrado.")
endif()

if(NOT CJSON_LIBRARY)
    message(FATAL_ERROR "Biblioteca cJSON não encontrada.")
endif()

add_executable(
    device_recife
    devices/recife.c
)

target_include_directories(
    device_recife
    PRIVATE
        ${CURL_INCLUDE_DIRS}
        ${CJSON_INCLUDE_DIR}
)

target_link_libraries(
    device_recife
    PRIVATE
        CURL::libcurl
        ${CJSON_LIBRARY}
)

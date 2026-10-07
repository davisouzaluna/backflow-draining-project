cmake_minimum_required(VERSION 3.15)
project(IN1061_IoT C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

find_path(CJSON_INCLUDE_DIR NAMES cjson/cJSON.h PATHS C:/msys64/ucrt64/include)
find_library(CJSON_LIBRARY NAMES cjson PATHS C:/msys64/ucrt64/lib)

if(NOT CJSON_INCLUDE_DIR OR NOT CJSON_LIBRARY)
    message(FATAL_ERROR "cJSON nao encontrado.")
endif()

find_path(MOSQ_INCLUDE_DIR NAMES mosquitto.h PATHS C:/msys64/ucrt64/include)
find_library(MOSQ_LIBRARY NAMES mosquitto PATHS C:/msys64/ucrt64/lib)

if(NOT MOSQ_INCLUDE_DIR OR NOT MOSQ_LIBRARY)
    message(FATAL_ERROR "libmosquitto nao encontrada.")
endif()

add_library(backflow_core STATIC
    src/tempo_util.c
    src/http_client.c
    src/json_parser.c
    src/chuva.c
    src/mare_tabua.c
    src/estimacao_mare.c
    src/drenagem.c
    src/analisador.c
    src/sensor_virtual.c
    src/relatorio.c
    src/payload.c
    src/mqtt_publisher.c
)

target_include_directories(backflow_core PUBLIC
    include
    ${CJSON_INCLUDE_DIR}
    ${MOSQ_INCLUDE_DIR}
)

target_compile_definitions(backflow_core PUBLIC
    DATA_DIR="${CMAKE_SOURCE_DIR}/data"
)

target_compile_options(backflow_core PRIVATE -Wall -Wextra)

target_link_libraries(backflow_core PUBLIC
    m
    CURL::libcurl
    ${CJSON_LIBRARY}
    ${MOSQ_LIBRARY}
)

add_executable(device_recife src/main.c)
target_link_libraries(device_recife PRIVATE backflow_core)

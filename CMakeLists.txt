cmake_minimum_required(VERSION 3.11)

project(pqcpp CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(nlohmann_json REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(Boost COMPONENTS system REQUIRED)
find_package(spdlog REQUIRED)

SET(CMAKE_FIND_LIBRARY_PREFIXES "")
SET(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".dll")

find_library(LIBPQ_LIB libpq)
find_program(LIBPQ_DLL libpq.dll)
find_path(LIBPQ_INCLUDE libpq-fe.h)

message("LIBPQ_LIB=${LIBPQ_LIB}")
message("LIBPQ_DLL=${LIBPQ_DLL}")
message("LIBPQ_INCLUDE=${LIBPQ_INCLUDE}")

if(MSVC)
    add_compile_options(-await -utf-8)
    add_compile_definitions(
        _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
        _WIN32_WINNT=0x0601
    )
endif(MSVC)

add_library(pq SHARED IMPORTED)
set_target_properties(pq PROPERTIES
    IMPORTED_LOCATION "${LIBPQ_DLL}"
	IMPORTED_IMPLIB "${LIBPQ_LIB}"
	INTERFACE_INCLUDE_DIRECTORIES "${LIBPQ_INCLUDE}"
)

add_library(pqcpp INTERFACE)
target_include_directories(pqcpp
    INTERFACE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(pqcpp INTERFACE 
    ws2_32
    pq
    Boost::system
    spdlog::spdlog
    fmt::fmt
    nlohmann_json::nlohmann_json
)

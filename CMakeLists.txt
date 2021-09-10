cmake_minimum_required(VERSION 3.11)

project(pqcpp CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(nlohmann_json REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(Boost COMPONENTS system REQUIRED)
find_package(spdlog REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SET(CMAKE_FIND_LIBRARY_PREFIXES "")
# SET(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".dll")

find_package(PostgreSQL REQUIRED)

add_library(pqcpp INTERFACE)
target_include_directories(pqcpp
    INTERFACE ${PROJECT_SOURCE_DIR}/include)
if (LIBPQ_INCLUDE)
target_include_directories(pqcpp
    INTERFACE ${LIBPQ_INCLUDE}
)
endif()

target_link_libraries(pqcpp INTERFACE
    PostgreSQL::PostgreSQL
    Boost::system
    spdlog::spdlog
    fmt::fmt
    nlohmann_json::nlohmann_json
)

if (MSVC)
    target_link_libraries(pqcpp INTERFACE ws2_32)
endif()

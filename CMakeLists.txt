cmake_minimum_required(VERSION 3.16)
project(mini_rt_oms LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

include_directories(${CMAKE_SOURCE_DIR}/src)

add_executable(oms
    src/oms/main.cpp
    src/oms/orders.cpp
    src/oms/positions.cpp
    src/oms/risk.cpp
    src/oms/ledger.cpp
    src/common/net.cpp
    src/common/messages.cpp
)

add_executable(venue_sim
    src/venue/main.cpp
    src/common/net.cpp
    src/common/messages.cpp
)

cmake_minimum_required(VERSION 3.31)
project(gavin-faucet-5btc-analysis)

set(CMAKE_CXX_STANDARD 11)

find_package(spdlog REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)

add_subdirectory(bitcoin-api-cpp-modified)

add_executable(gavin-faucet-5btc-analysis main.cpp
        Database.cpp)

target_link_libraries(gavin-faucet-5btc-analysis PRIVATE jsoncpp curl sqlite3 spdlog::spdlog bitcoinapi)

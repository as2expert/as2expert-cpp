cmake_minimum_required(VERSION 3.16)
project(as2expert VERSION 0.1.0 LANGUAGES CXX)

option(AS2EXPERT_BUILD_TESTS "Build unit tests" ON)
option(AS2EXPERT_BUILD_EXAMPLES "Build examples" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

# nlohmann/json: prefer an installed package, else fetch it.
find_package(nlohmann_json 3.9 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(as2expert
    src/base64.cpp
    src/transport.cpp
    src/client.cpp
    src/webhooks.cpp)
add_library(as2expert::as2expert ALIAS as2expert)

target_include_directories(as2expert
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(as2expert
    PUBLIC nlohmann_json::nlohmann_json
    PRIVATE CURL::libcurl OpenSSL::Crypto)

target_compile_options(as2expert PRIVATE -Wall -Wextra)

if(AS2EXPERT_BUILD_TESTS)
    enable_testing()
    add_executable(as2expert_tests tests/test_webhooks.cpp)
    target_link_libraries(as2expert_tests PRIVATE as2expert)
    add_test(NAME as2expert_tests COMMAND as2expert_tests)
endif()

if(AS2EXPERT_BUILD_EXAMPLES)
    add_executable(smoke examples/smoke.cpp)
    target_link_libraries(smoke PRIVATE as2expert)
endif()

include(GNUInstallDirs)
install(TARGETS as2expert EXPORT as2expertTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

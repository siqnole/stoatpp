cmake_minimum_required(VERSION 3.16)
project(stoatpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_definitions(JSON_DIAGNOSTICS=1)

option(STOATPP_BUILD_EXAMPLES "Build example bots" ON)
option(STOATPP_BUILD_CLIENT "Build the chat client application" ON)
option(STOATPP_USE_BOOST_BEAST "Use Boost.Beast instead of ixwebsocket" OFF)

include(FetchContent)

# nlohmann/json
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# cpp-httplib (header-only)
FetchContent_Declare(httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.0
)
FetchContent_MakeAvailable(httplib)

if(NOT STOATPP_USE_BOOST_BEAST)
    # ixwebsocket
    set(USE_TLS ON CACHE BOOL "Enable TLS/SSL support" FORCE)
    FetchContent_Declare(ixwebsocket
        GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
        GIT_TAG        v11.4.5
    )
    FetchContent_MakeAvailable(ixwebsocket)
endif()

find_package(OpenSSL REQUIRED)

# stoat++ library target
add_library(stoatpp
    src/cluster.cpp
    src/rest.cpp
    src/gateway.cpp
    src/event_handler.cpp
    src/utils/ratelimiter.cpp
    src/utils/logger.cpp
)

target_include_directories(stoatpp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(stoatpp PUBLIC
    nlohmann_json::nlohmann_json
    httplib::httplib
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(NOT STOATPP_USE_BOOST_BEAST)
    target_link_libraries(stoatpp PUBLIC ixwebsocket)
    target_compile_definitions(stoatpp PUBLIC STOATPP_USE_IXWEBSOCKET)
else()
    find_package(Boost REQUIRED COMPONENTS system)
    target_link_libraries(stoatpp PUBLIC Boost::system)
    target_compile_definitions(stoatpp PUBLIC STOATPP_USE_BOOST_BEAST)
endif()

# Examples
if(STOATPP_BUILD_EXAMPLES)
    add_subdirectory(examples/ping_bot)
    add_subdirectory(examples/full_bot)
    add_subdirectory(examples/moderation_bot)
    add_subdirectory(examples/reaction_roles_bot)
    add_subdirectory(examples/custom_prefix_bot)
endif()

# Client App
if(STOATPP_BUILD_CLIENT)
    add_subdirectory(client)
endif()


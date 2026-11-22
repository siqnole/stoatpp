cmake_minimum_required(VERSION 3.20)
project(stoatpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(JSON_DIAGNOSTICS=1)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)
if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

option(STOATPP_BUILD_EXAMPLES "Build example bots" ON)
option(STOATPP_BUILD_CLIENT "Build the chat client application" ON)
option(STOATPP_USE_BOOST_BEAST "Use Boost.Beast instead of ixwebsocket" OFF)

include(FetchContent)

# nlohmann/json
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)
# Expose the FetchContent-provided nlohmann_json to any downstream find_package() calls
# (libdatachannel uses find_package when USE_SYSTEM_JSON=ON)
set(nlohmann_json_DIR "${json_BINARY_DIR}")

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

# LiveKit C++ Client SDK (replaces manual libopus + libdatachannel WebRTC signaling)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
if(LIVEKIT_LOCAL_SDK_DIR)
    list(PREPEND CMAKE_PREFIX_PATH "${LIVEKIT_LOCAL_SDK_DIR}")
else()
    include(LiveKitSDK)
    livekit_sdk_setup(
        VERSION "1.13.1"
        SDK_DIR "${CMAKE_BINARY_DIR}/_deps/livekit-sdk"
        GITHUB_TOKEN "$ENV{GITHUB_TOKEN}"
    )
endif()
find_package(LiveKit CONFIG REQUIRED)

find_package(OpenSSL REQUIRED)

set(ENV{PKG_CONFIG_PATH} "/home/linuxbrew/.linuxbrew/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSE REQUIRED libpulse libpulse-simple)

# stoat++ library target
add_library(stoatpp
    src/cluster.cpp
    src/cluster/cache.cpp
    src/cluster/events.cpp
    src/cluster/messaging.cpp
    src/cluster/moderation.cpp
    src/cluster/commands.cpp
    src/cluster/misc.cpp
    src/rest.cpp
    src/gateway.cpp
    src/event_handler.cpp
    src/pulse_voice.cpp
    src/voice_connection.cpp
    src/utils/ratelimiter.cpp
    src/utils/logger.cpp
)

target_include_directories(stoatpp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${PULSE_INCLUDE_DIRS}
)

target_link_directories(stoatpp PUBLIC ${PULSE_LIBRARY_DIRS})

target_link_libraries(stoatpp PUBLIC
    nlohmann_json::nlohmann_json
    httplib::httplib
    OpenSSL::SSL
    OpenSSL::Crypto
    ${PULSE_LIBRARIES}
    LiveKit::livekit
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

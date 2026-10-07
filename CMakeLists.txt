cmake_minimum_required(VERSION 3.25)

project(BassFinisher VERSION 1.0.0)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ProjectWarnings)

# Keep this project self-contained and reproducible.
# Do not use or modify a globally installed VST3 SDK.
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES ON CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR ON CACHE BOOL "" FORCE)

# CI/builds should create the bundle only, not install/link it into the
# current user's VST3 folder. The artifact staging step handles distribution.
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)

include(FetchContent)

FetchContent_Declare(
    vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG v3.8.1_build_84
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(vst3sdk)

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)

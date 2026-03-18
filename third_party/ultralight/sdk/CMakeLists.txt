cmake_minimum_required(VERSION 3.15.0)

project(Ultralight-SDK 
        LANGUAGES C CXX
        VERSION 1.4.0)

get_filename_component(UL_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}" REALPATH)

list(APPEND CMAKE_MODULE_PATH ${UL_SDK_PATH}/cmake)

set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/out" CACHE PATH "install prefix" FORCE)

if (NOT CMAKE_SYSTEM_NAME MATCHES "Windows|Darwin|Linux")
    message(FATAL_ERROR
        "This Ultralight SDK package only contains prebuilt desktop binaries. "
        "Porting to ${CMAKE_SYSTEM_NAME} requires engine source or a vendor-provided "
        "static/freestanding build, neither of which is included here.")
endif ()

add_subdirectory(samples)
add_subdirectory(tools)

# CMake configuration
cmake_minimum_required(VERSION 3.7)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library name
set(LIBRARY_NAME Network)

# Check if project is self-hosted
if (NOT DEFINED PROJECT_NAME)
    project(
        ${LIBRARY_NAME}
        VERSION 1.0.0
        LANGUAGES C CXX
    )
endif()

# Link common function
function(LINK_LIBRARY LIBRARY_INCLUDE_DIR TARGET_NAME LIBRARY_NAME)
    if(NOT EXISTS ${LIBRARY_INCLUDE_DIR})
        message(FATAL_ERROR "Error searching for ${LIBRARY_NAME} (not exist)")
    endif()

    target_link_libraries(
        ${TARGET_NAME} PRIVATE
        ${LIBRARY_NAME}
    )

    target_include_directories(
        ${TARGET_NAME} PUBLIC
        ${LIBRARY_INCLUDE_DIR}
    )
    message(STATUS "Linking target " ${TARGET_NAME} " : added lib " ${LIBRARY_NAME})
endfunction()

# Component add common function
function(LINK_COMPONENT TARGET_NAME COMPONENT_ROOT_DIR COMPONENT_NAME)
    if(NOT EXISTS ${COMPONENT_ROOT_DIR}/Components-${COMPONENT_NAME})
        message(FATAL_ERROR "Error searching for component: ${COMPONENT_NAME} (not exist)")
    endif()

    add_subdirectory(${COMPONENT_ROOT_DIR}/Components-${COMPONENT_NAME})
    LINK_LIBRARY(${COMPONENT_ROOT_DIR}/Components-${COMPONENT_NAME}/include ${TARGET_NAME} ${COMPONENT_NAME})
endfunction()

# Packages
find_package(QT NAMES Qt5 REQUIRED COMPONENTS Core Network)
find_package(Qt5 REQUIRED COMPONENTS Core Network)
set(
    TARGET_LINKED_LIBS
    Qt5::Core
    Qt5::Network
    boost_system
    ssl
    crypto
)

# Qt configuration
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Library target
add_library(${LIBRARY_NAME} SHARED)
target_link_libraries(${LIBRARY_NAME} PRIVATE ${TARGET_LINKED_LIBS} )

LINK_COMPONENT(${LIBRARY_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/depends Logger)
LINK_COMPONENT(${LIBRARY_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/depends Encryption)

# Files
file(GLOB_RECURSE HEADERS "src/*.hpp" "src/*.h")
file(GLOB_RECURSE SOURCES "src/*.cpp")
target_sources(${LIBRARY_NAME} PRIVATE ${HEADERS} ${SOURCES} ${UIFILES})

# Suffixes
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${LIBRARY_NAME} PRIVATE DEBUG_MODE)
    target_compile_options(${LIBRARY_NAME} PRIVATE -g)
    set_target_properties(${LIBRARY_NAME} PROPERTIES SUFFIX ".so.DEBUG")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(${LIBRARY_NAME} PRIVATE NDEBUG)
    target_compile_options(${LIBRARY_NAME} PRIVATE -O3)
    set_target_properties(${LIBRARY_NAME} PROPERTIES SUFFIX ".so")
endif()

# Includes
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src")
    message(FATAL_ERROR "Error configuring library! Not found sources dir: " "${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()

target_include_directories(${LIBRARY_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
message(STATUS "Configured library: " ${LIBRARY_NAME})

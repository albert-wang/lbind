cmake_minimum_required(VERSION 3.0.0)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# Set variables
set(CMAKE_TOOLCHAIN_FILE "/Users/albertwang/Development/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
enable_testing()

# Project
project(luabinding)

# Settings
set(CMAKE_CXX_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Includes
include_directories(include)
include_directories(/Users/albertwang/Dependencies/include)
include_directories(/Users/albertwang/Development/vcpkg/installed/x64-osx/include)
include_directories(../lua/src)

# Source Files
file(GLOB SOURCES "src/*.cpp")

# Libraries
link_directories(/Users/albertwang/Dependencies/lib)
link_directories(/Users/albertwang/Development/vcpkg/installed/x64-osx/lib)
link_libraries(boost_thread boost_system lua fmt)

# Executable
add_executable(luabinding ${SOURCES})

# Test setup
include_directories(test)
file(GLOB TEST_SOURCES "test/*.cpp")

link_libraries(boost_unit_test_framework)

set(CMAKE_CXX_FLAGS "-O3")

add_executable(test_binary ${TEST_SOURCES} ${SOURCES})
add_test(NAME test COMMAND test_binary)

target_compile_definitions(test_binary PRIVATE UNIT_TESTING=1)
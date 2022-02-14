cmake_minimum_required(VERSION 3.5)
set(PROJECT_NAME gtest-csummerize)
project(${PROJECT_NAME})

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)
# Adding a version of library.
set(${PROJECT_NAME}_VERSION_MAJOR 1)
set(${PROJECT_NAME}_VERSION_MINOR 0)
set(${PROJECT_NAME}_VERSION_BUILD 0)

# Find ABSL package.
find_package(absl REQUIRED)
# Project libraries.
set(PROJECT_LIBS ${PROJECT_LIBS} csummerize baalbek mtc morphrus moonycode stemka re2 absl::strings absl::raw_hash_set absl::hash)
set(GTEST_LIBS gtest)
set(THREAD_LIBS pthread)
# Include dependencies.
include_directories(${CMAKE_SOURCE_DIR})
# Collecting a list of sources.
file(GLOB_RECURSE FILES_SRC *.[hc]pp)

add_executable(${PROJECT_NAME} ${FILES_SRC})
target_link_libraries(
    ${PROJECT_NAME}
    ${PROJECT_LIBS}
    ${GTEST_LIBS}
    ${THREAD_LIBS}
)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME} WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

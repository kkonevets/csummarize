cmake_minimum_required(VERSION 3.5)
set(PROJECT_NAME csummerize)
project(${PROJECT_NAME})

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__FILENAME__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")

option(PROFILE_ENABLED "Enabling profiling." OFF)
option(BUILD_STATS "Enabling unistats build." ON)

# Finds modules.
find_package(absl REQUIRED)

set(PROJECT_LIBS baalbek mtc morphrus moonycode)
set(OTHERS_LIBS absl::strings absl::raw_hash_set absl::hash re2)

if (BUILD_STATS)
    add_subdirectory(unistats)
endif (BUILD_STATS)
# Enabling profiler.
if (PROFILE_ENABLED)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
    # Setting macros.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPROFILE_ENABLED")
endif(PROFILE_ENABLED)
# Include files.
include_directories(${PROJECT_SOURCE_DIR})
# Collecting a list of sources.
file(GLOB_RECURSE SOURCE_FILES src/*.[hc]pp)
file(GLOB EXPORT_INCS *.h *.hpp)
message(INFO " ${UNISTATS_LIBS}")

add_library(${PROJECT_NAME} ${SOURCE_FILES})
target_link_libraries(
    ${PROJECT_NAME}
    ${PROJECT_LIBS}
    ${OTHERS_LIBS}
    ${UNISTATS_LIBS}
)
install(TARGETS ${PROJECT_NAME} DESTINATION /usr/local/lib)
install(FILES ${UNISTATS_LIBS} DESTINATION /usr/local/lib)
install(FILES ${EXPORT_INCS} DESTINATION /usr/local/include/${PROJECT_NAME})

if (BUILD_TESTS)
    add_subdirectory(tests)
endif (BUILD_TESTS)

if (BUILD_TOOLS)
    add_subdirectory(tools)
endif (BUILD_TOOLS)

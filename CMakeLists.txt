cmake_minimum_required(VERSION 3.16)
project(rust_hack)

set(CMAKE_CXX_STANDARD 20)

set(VMREAD_SRC
        vmread/wintools.c
        vmread/vmmem.c
        vmread/mem.c
        vmread/pmparser.c
        vmread/hlapi/windll.cpp
        vmread/hlapi/winprocess.cpp
        vmread/hlapi/winprocesslist.cpp src/wrappers.h src/radar.h src/radar.cpp src/utils.h src/utils.cpp)

add_definitions(-DLMODE=MODE_EXTERNAL)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -pthread -march=native -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake/;${CMAKE_MODULE_PATH}")

find_package(PkgConfig REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SFML REQUIRED system window graphics network audio)
pkg_check_modules(FMT REQUIRED IMPORTED_TARGET fmt)

# GLM
add_subdirectory(glm)

include_directories(${SDL2_INCLUDE_DIRS})

file(GLOB_RECURSE HACK_SRC src/*)
set(SRC ${HACK_SRC} ${VMREAD_SRC})

set(HACK_LIBRARY TRUE)
if (${HACK_LIBRARY})
    add_definitions(-DHACK_SHARED_LIBRARY)
    add_library(rust_hack SHARED ${SRC} src/overlay.cpp src/overlay.h)
else()
    add_definitions(-DHACK_EXECUTABLE)
    add_executable(rust_hack ${SRC})
endif()

target_link_libraries(rust_hack glm ${SDL2_LIBRARIES} ${SFML_LIBRARIES} ${SFML_DEPENDENCIES})
target_link_libraries(rust_hack PkgConfig::FMT)

target_include_directories(rust_hack PRIVATE pointer/)

configure_file(arial.ttf arial.ttf COPYONLY)
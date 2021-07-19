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
        vmread/hlapi/winprocesslist.cpp wrappers.h radar.h radar.cpp utils.h utils.cpp)

add_definitions(-DLMODE=MODE_EXTERNAL)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -pthread -march=native -Wall")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake/;${CMAKE_MODULE_PATH}")

find_package(SDL2 REQUIRED)
find_package(SFML REQUIRED system window graphics network audio)

# GLM
add_subdirectory(glm)

include_directories(${SDL2_INCLUDE_DIRS})

set(HACK_SRC main.cpp pointer.hpp csutils.h wrappers.h radar.h radar.cpp utils.h ${VMREAD_SRC})

set(HACK_LIBRARY TRUE)
if (${HACK_LIBRARY})
    add_definitions(-DHACK_SHARED_LIBRARY)
    add_library(rust_hack SHARED ${HACK_SRC} overlay.cpp overlay.h)
else()
    add_definitions(-DHACK_EXECUTABLE)
    add_executable(rust_hack ${HACK_SRC})
endif()

target_link_libraries(rust_hack glm ${SDL2_LIBRARIES} ${SFML_LIBRARIES} ${SFML_DEPENDENCIES})

configure_file(arial.ttf arial.ttf COPYONLY)
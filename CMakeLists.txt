cmake_minimum_required(VERSION 3.16)
project(rust_hack)

set(CMAKE_CXX_STANDARD 17)

set(VMREAD_SRC
        vmread/wintools.c
        vmread/vmmem.c
        vmread/mem.c
        vmread/pmparser.c
        vmread/hlapi/windll.cpp
        vmread/hlapi/winprocess.cpp
        vmread/hlapi/winprocesslist.cpp wrappers.h radar.h radar.cpp utils.h)

add_definitions(-DLMODE=MODE_EXTERNAL)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -pthread")

set (CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake/;${CMAKE_MODULE_PATH}")

find_package (SFML REQUIRED system window graphics network audio)

include_directories(${SFML_INCLUDE_DIR})

add_executable(rust_hack main.cpp pointer.h csutils.h wrappers.h radar.h radar.cpp ${VMREAD_SRC})

target_link_libraries(rust_hack ${SFML_LIBRARIES} ${SFML_DEPENDENCIES})


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
        vmread/hlapi/winprocesslist.cpp wrappers.h radar.h radar.cpp)

add_definitions(-DLMODE=MODE_EXTERNAL)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

add_executable(rust_hack main.cpp pointer.h csutils.h wrappers.h radar.h radar.cpp ${VMREAD_SRC})


find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})
target_link_libraries(rust_hack ${SDL2_LIBRARIES})


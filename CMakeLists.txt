cmake_minimum_required(VERSION 3.22)
project(ld C)

set(CMAKE_C_STANDARD 11)
if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    add_definitions(-DDEBUG)
endif()
add_executable(ld main.c Hash.c Hash.h list.c list.h ProcessSingleFile.c ProcessSingleFile.h SectionDef.c SectionDef.h GlobalDefs.c GlobalDefs.h)

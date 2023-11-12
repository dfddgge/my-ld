cmake_minimum_required(VERSION 3.22)
project(ld C)

set(CMAKE_C_STANDARD 11)
if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    add_definitions(-DDEBUG)
endif()
add_executable(ld main.c utils/Hash.c utils/Hash.h utils/list.c utils/list.h process/ProcessSingleFile.c process/ProcessSingleFile.h common/SectionDef.c common/SectionDef.h common/GlobalDefs.c common/GlobalDefs.h process/LinkAndRelocate.c process/LinkAndRelocate.h)

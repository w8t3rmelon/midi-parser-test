cmake_minimum_required(VERSION 3.16)

project(midi-parser LANGUAGES C)

option(BUILD_TEST_PROGRAM OFF "builds the test program along with the library")

add_library(midi-parser STATIC
    src/mf_structs.h
    src/mf_util.h
    src/mf_construct.h src/mf_construct.c
    src/mf_parse.h src/mf_parse.c)

if(BUILD_TEST_PROGRAM)
    add_executable(midi-parser-testprogram
        src/testprogram/main.c)
    target_link_libraries(midi-parser-testprogram PRIVATE midi-parser)
    configure_file(test.mid . COPYONLY)
endif()
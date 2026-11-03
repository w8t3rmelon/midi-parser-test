cmake_minimum_required(VERSION 3.16)

project(midi-parser-test LANGUAGES C)

add_executable(midi-parser-test
    src/mf_structs.h
    src/mf_util.h
    src/mf_construct.h src/mf_construct.c
    src/mf_parse.h src/mf_parse.c
    src/main.c)

configure_file(test.mid . COPYONLY)
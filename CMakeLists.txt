cmake_minimum_required(VERSION 3.16)

project(midi-parser-test LANGUAGES C)

add_executable(midi-parser-test main.c)

configure_file(test.mid . COPYONLY)
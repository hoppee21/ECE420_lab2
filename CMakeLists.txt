cmake_minimum_required(VERSION 3.27)
project(lab2 C)

set(CMAKE_C_STANDARD 90)

include_directories(.)

add_executable(lab2
        client.c
        common.h
        timer.h
        attacker.c
        server.c
)

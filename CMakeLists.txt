cmake_minimum_required(VERSION 3.27)
project(lab2 C)

set(CMAKE_C_STANDARD 90)

include_directories(.)

# Original executable
add_executable(lab2
        common.h
        timer.h
        main.c
)

# New executable 'attacker' depending on common.h and its own source file 'attacker.c'
add_executable(attacker
        common.h
        attacker.c
)

# If you need to link against the math library, add this line
target_link_libraries(attacker m)

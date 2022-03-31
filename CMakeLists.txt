cmake_minimum_required(VERSION 3.21)
project(pa1 C)

set(CMAKE_C_STANDARD 99)

add_executable(pa1 main.c utils.c utils.h general_structures.h child_process.c child_process.h ipc.c banking.c)

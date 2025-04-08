cmake_minimum_required(VERSION 3.28)
project(measure_complexity C)

set(CMAKE_C_STANDARD 11)

add_executable(measure_complexity
        cJSON/cJSON_Utils.c
        csv/csv.c)

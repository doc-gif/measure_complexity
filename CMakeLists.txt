cmake_minimum_required(VERSION 3.28)
project(measure_complexity C)

set(CMAKE_C_STANDARD 11)

add_executable(measure_complexity
        cJSON/cJSON_Utils.c
        csv/csv.c
        csv/csv_inc_goto.c
        csv/csv-if_else-if_else.c
        cJSON/cJSON_Utils_inc_goto.c
        cJSON/cJSON_Utils.c
        cJSON/cJSON_Utils_if_else-if_else.c
)

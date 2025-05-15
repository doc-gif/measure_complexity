cmake_minimum_required(VERSION 3.28)
project(measure_complexity C)

set(CMAKE_C_STANDARD 11)

add_executable(measure_complexity
#        cJSON/cJSON_Utils.c
#        csv/csv.c
#        csv/csv_inc_goto.c
#        csv/csv-if_else-if_else.c
#        cJSON/cJSON_Utils_inc_goto.c
#        cJSON/cJSON_Utils.c
#        cJSON/cJSON_Utils_if_else-if_else.c
        cJSON/cJSON_Utils_minimize.c
#        cJSON/cJSON_Utils_minimize_inlining.c
        cJSON/cJSON.c
        main.c
        test/test_helper.c
        test/test_helper.h
        test/test_find_pointer.c
        test/test_find_pointer.h
        test/test_create_patches.c
        test/test_create_patches.h
)

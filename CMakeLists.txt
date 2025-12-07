cmake_minimum_required(VERSION 3.28)
project(measure_complexity C)

set(CMAKE_C_STANDARD 11)

add_executable(measure_complexity
#        ini_parser/iniparser_minimize_with_dictionary.c
#        ini_parser/iniparser_minimize_with_dictionary_inlining.c
#        ini_parser/iniparser_minimize_with_dictionary_refactoring_gemini_2-5_pro.c

        cJSON/cJSON.c
        cJSON/cJSON_Utils_minimize_parse_json.c
        cJSON/cJSON_Utils_minimize_parse_json_inlining.c
        cJSON/cJSON_Utils_minimize_parse_json_inlining_refactoring_gemini_2-5_pro.cjs
)

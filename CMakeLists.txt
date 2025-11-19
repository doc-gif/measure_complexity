cmake_minimum_required(VERSION 3.28)
project(measure_complexity C)

set(CMAKE_C_STANDARD 11)

add_executable(measure_complexity
#        ini_parser/dictionary.c
        ini_parser/dictionary_del_func.c
#        ini_parser/iniparser_minimize.c
        ini_parser/iniparser_minimize_with_dictionary.c
#        ini_parser/iniparser_minimize_inlining.c
#        ini_parser/iniparser_minimize_inlining_refactoring_gemini_2-5_pro.c
#        ini_parser/iniparser_minimize_inlining_refactoring_gemini_2-5_pro_v3.c

        cJSON/cJSON.c
#        cJSON/cJSON_Utils_minimize_createJsonPatch.c
#        cJSON/cJSON_Utils_minimize_createJsonPatch_inlining.c
#        cJSON/cJSON_Utils_minimize_createJsonPatch_inlining_refactoring_gemini_2-5_pro.c
#        cJSON/cJSON_Utils_minimize_createJsonPatch_inlining_refactoring_gemini_2-5_pro_v3.c
)

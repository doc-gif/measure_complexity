#include <stdio.h>

#include "test/test_create_patches.h"
#include "test/test_find_pointer.h"
#include "test/test_csv_parser.h"

// Include headers for other test suites here when they are added
// #include "test_another_function.h"

int main() {
    int total_tests = 0;
    int passed_tests = 0;

    printf("Running all test suites...\n\n");

    // Run the test suite for cJSONUtils_FindPointerFromObjectTo
    run_all_find_pointer_tests(&total_tests, &passed_tests);

    // Add calls to other test suites here when implemented
    // run_all_another_function_tests(&total_tests, &passed_tests);

    run_all_create_patches_tests(&total_tests, &passed_tests);

    // --- Final Summary ---
    printf("--- Overall Test Summary ---\n");
    printf("Total tests run: %d\n", total_tests);
    printf("Passed tests: %d\n", passed_tests);
    printf("Failed tests: %d\n", total_tests - passed_tests);

    printf("Running all test suites...\n\n");

    // cJSON Utils Tests (もしあれば呼び出し)
    // run_all_cjson_utils_tests(&total_tests, &passed_tests);

    // CSV Parser Tests
    run_all_csv_search_lf_tests(&total_tests, &passed_tests);
    run_all_csv_read_next_row_tests(&total_tests, &passed_tests);


    // Add calls to other test suites here when implemented

    // --- Final Summary ---
    printf("--- Overall Test Summary ---\n");
    printf("Total tests run: %d\n", total_tests);
    printf("Passed tests: %d\n", passed_tests);
    printf("Failed tests: %d\n", total_tests - passed_tests);

    return (total_tests == passed_tests) ? 0 : 1; // Return non-zero on failure
}

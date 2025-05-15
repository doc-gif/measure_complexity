#ifndef TEST_FIND_POINTER_H_
#define TEST_FIND_POINTER_H_

// Include necessary headers for function signatures
#include "../cJSON/cJSON.h"
#include "test_helper.h" // For run_test signature

// Declare the function that runs all tests for cJSONUtils_FindPointerFromObjectTo
// It takes pointers to total and passed counts to update them.
void run_all_find_pointer_tests(int* total, int* passed);

#endif // TEST_FIND_POINTER_H_
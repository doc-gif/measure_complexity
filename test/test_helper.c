#include "test_helper.h"
// cJSONUtils_FindPointerFromObjectTo 関数の宣言のために必要
#include "../cJSON/cJSON_Utils_minimize_inlining.h"
// 実際の cJSON API の実装を使うために cJSON.h も必要 (test_helper.h でインクルード済み)
// #include "cJSON/cJSON.h" // test_helper.h でインクルードされているので不要だが、明示的に書いても良い
#include <stdio.h>
#include <stdlib.h> // For NULL
#include <string.h> // For strcmp
#include <stdbool.h> // For bool type

// --- テスト用 cJSON 構造体構築ヘルパー (実際の cJSON API を使用) ---

cJSON* create_json_value_null(void) {
    return cJSON_CreateNull();
}

cJSON* create_json_value_bool(cJSON_bool boolean) {
    return cJSON_CreateBool(boolean);
}

cJSON* create_json_value_number(double num) {
    return cJSON_CreateNumber(num);
}

cJSON* create_json_value_string(const char *string) {
    return cJSON_CreateString(string);
}

cJSON* create_json_container_array(void) {
    return cJSON_CreateArray();
}

cJSON* create_json_container_object(void) {
    return cJSON_CreateObject();
}


// --- テスト用 cJSON アイテム追加ヘルパー (実際の cJSON API を使用) ---
// 実際の cJSON_AddItem* 関数は、追加されるアイテムの所有権を取得し、
// 内部的にリンクとキー文字列のコピーを行います。

void add_item_to_object(cJSON* parent, const char* key, cJSON* item) {
    if (!parent || !key || !item) return;
    // 実際の cJSON_AddItemToObject 関数を使用
    // キー文字列(key)はここで渡され、ライブラリが内部コピーします。
    // item->string は cJSON_AddItemToObject によって設定/管理されます。
    cJSON_AddItemToObject(parent, key, item);
}

void add_item_to_array(cJSON* parent, cJSON* item) {
     if (!parent || !item) return;
     // 実際の cJSON_AddItemToArray 関数を使用
     cJSON_AddItemToArray(parent, item);
}


// --- テスト用 cJSON 構造体解放ヘルパー (実際の cJSON API を使用) ---

void free_json_tree(cJSON* item) {
    // 実際の cJSON_Delete 関数を使用。
    // これが再帰的に子や内部で割り当てられたメモリを解放します。
    cJSON_Delete(item);
}

// --- Test cJSON Array Clear Helper ---
// 配列の全子要素を解放し、配列を空にする
void clear_json_array(cJSON* array) {
    if (!array || !cJSON_IsArray(array)) return;

    cJSON* child = array->child;
    while (child != NULL) {
        cJSON* next_child = child->next;
        // 子をリストから切り離してから解放する
        // これを行わないと、解放後に child = child->next と進む際に不正なメモリにアクセスする可能性がある
        if (child->prev) {
            child->prev->next = NULL;
        }
        if (child->next) {
            child->next->prev = NULL;
        }
        child->prev = NULL; // 子自身の prev/next ポインタもクリア
        child->next = NULL;

        // 子ノードとその子孫（今回はパッチオブジェクト自体）を解放
        free_json_tree(child);
        child = next_child;
    }
    array->child = NULL; // 配列が空になったことを示すために child ポインタを NULL にする
}


// --- テスト実行ヘルパー ---
// テスト対象関数を呼び出し、結果を検証します。

int run_test_find_pointer(const char* test_name, cJSON* object, const cJSON* target, const char* expected_pointer) {
    printf("Running test: %s\n", test_name);
    // テスト対象の実際の関数を呼び出す
    char* actual_pointer = cJSONUtils_FindPointerFromObjectTo(object, target);
    int success = 0;

    if (expected_pointer == NULL) {
        if (actual_pointer == NULL) {
            success = 1;
            printf("  Result: PASS (Expected NULL, Got NULL)\n");
        } else {
            success = 0;
            printf("  Result: FAIL (Expected NULL, Got \"%s\")\n", actual_pointer);
        }
    } else {
        if (actual_pointer != NULL && strcmp(actual_pointer, expected_pointer) == 0) {
            success = 1;
            printf("  Result: PASS (Expected \"%s\", Got \"%s\")\n", expected_pointer, actual_pointer);
        } else {
            success = 0;
            printf("  Result: FAIL (Expected \"%s\", Got %s)\n",
                   expected_pointer, actual_pointer ? actual_pointer : "NULL");
        }
    }

    // cJSONUtils_FindPointerFromObjectTo によって割り当てられたポインタを解放する
    // 実際の cJSON_free 関数を使用
    if (actual_pointer) {
       cJSON_free(actual_pointer);
    }

    // run_test 関数自体はテスト構造体を解放しません。
    // 構造体の解放は、この関数を呼び出したテストケース関数が行う責任です
    // (free_json_tree を呼び出すことによって)。

    printf("---\n");
    return success;
}

// --- パッチテスト用アサーションヘルパー ---

cJSON* create_expected_patch(const char* op, const char* final_patch_path, const cJSON* value) {
    cJSON* patch_op = create_json_container_object();
    if (!patch_op) return NULL;

    add_item_to_object(patch_op, "op", create_json_value_string(op));
    add_item_to_object(patch_op, "path", create_json_value_string(final_patch_path)); // Use the pre-calculated final path

    if (((strcmp(op, "add") == 0) || (strcmp(op, "replace") == 0)) && value != NULL)
    {
        cJSON* value_copy = cJSON_Duplicate(value, cJSON_True); // Deep copy
        if (value_copy) {
            add_item_to_object(patch_op, "value", value_copy);
        } else {
            free_json_tree(patch_op);
            return NULL;
        }
    }
    return patch_op;
}


// compare_patch_arrays: Compares two patch arrays (actual vs expected)
// Assumes both arrays contain standard RFC 6902 patch objects.
cJSON_bool compare_patch_arrays(cJSON* actual, cJSON* expected) {
    if (actual == expected) return cJSON_True;
    if (!actual || !expected || !cJSON_IsArray(actual) || !cJSON_IsArray(expected)) return cJSON_False;

    int actual_size = cJSON_GetArraySize(actual); // Assume this function exists in cJSON.h
    int expected_size = cJSON_GetArraySize(expected); // Assume this function exists

    if (actual_size != expected_size) {
        fprintf(stdout, "Patch array size mismatch. Actual: %d, Expected: %d\n", actual_size, expected_size);
        return cJSON_False;
    }

    // Simple comparison: Check if each patch in actual has a match in expected.
    // This is not robust if there are duplicate identical patches.
    // A better approach would be to sort both arrays and compare element by element,
    // or mark matched items. Sorting requires deep comparison for sorting criteria.
    // Let's use a simple "find a match" approach for simplicity in the test helper.
    // This requires create_expected_patch to be ordered the same way create_patches generates them.

    cJSON* actual_patch = actual->child;
    cJSON* expected_patch = expected->child;

    while (actual_patch != NULL && expected_patch != NULL) {
        // Compare "op"
        cJSON* actual_op_item = cJSON_GetObjectItemCaseSensitive(actual_patch, "op"); // Assume exists
        cJSON* expected_op_item = cJSON_GetObjectItemCaseSensitive(expected_patch, "op"); // Assume exists
        if (!actual_op_item || !expected_op_item || !cJSON_IsString(actual_op_item) || !cJSON_IsString(expected_op_item) ||
            strcmp(actual_op_item->valuestring, expected_op_item->valuestring) != 0) {
            fprintf(stdout, "Patch op mismatch or missing\n");
            return cJSON_False;
            }

        // Compare "path"
        cJSON* actual_path_item = cJSON_GetObjectItemCaseSensitive(actual_patch, "path");
        cJSON* expected_path_item = cJSON_GetObjectItemCaseSensitive(expected_patch, "path");
        if (!actual_path_item || !expected_path_item || !cJSON_IsString(actual_path_item) || !cJSON_IsString(expected_path_item) ||
            strcmp(actual_path_item->valuestring, expected_path_item->valuestring) != 0) {
            fprintf(stdout, "Patch path mismatch or missing\n");
            return cJSON_False;
            }

        // Compare "value" (if present in expected)
        cJSON* actual_value_item = cJSON_GetObjectItemCaseSensitive(actual_patch, "value");
        cJSON* expected_value_item = cJSON_GetObjectItemCaseSensitive(expected_patch, "value");

        if (expected_value_item != NULL) {
            if (actual_value_item == NULL) {
                fprintf(stdout, "Patch value missing in actual, but present in expected\n");
                return cJSON_False;
            }
            // Deep compare the value items
            // Assume cJSON_Compare functions exist for deep comparison
            // If not, a manual recursive comparison is needed.
            // Let's assume cJSON_Compare is available for simplicity here.
            // If not, you need to implement a helper like compare_cjson_trees(item1, item2, case_sensitive).
            // Assuming cJSON_Compare(item1, item2, case_sensitive) exists and works for values.
            // Note: cJSON_Compare might need case_sensitive flag handling for strings/keys inside the value.
            if (!cJSON_Compare(actual_value_item, expected_value_item, cJSON_True)) // Assume case-sensitive compare for values
            {
                fprintf(stdout, "Patch value mismatch\n");
                // Optional: print structures to help debug
                // char* actual_val_str = cJSON_PrintUnformatted(actual_value_item);
                // char* expected_val_str = cJSON_PrintUnformatted(expected_value_item);
                // fprintf(stderr, "Actual value: %s\n", actual_val_str ? actual_val_str : "NULL");
                // fprintf(stderr, "Expected value: %s\n", expected_val_str ? expected_val_str : "NULL");
                // if(actual_val_str) cJSON_free(actual_val_str);
                // if(expected_val_str) cJSON_free(expected_val_str);
                return cJSON_False;
            }
        } else {
            // Expected does not have a value field, actual should not have one either
            if (actual_value_item != NULL) {
                fprintf(stdout, "Patch value present in actual, but not expected\n");
                return cJSON_False;
            }
        }
        // Add checks for "from" if compose_patch mock creates it and create_expected_patch expects it.
        // Based on revised mock, we expect standard RFC 6902 patches without "from" for add/replace/remove.


        actual_patch = actual_patch->next;
        expected_patch = expected_patch->next;
    }

    // If we reached the end, sizes matched and all elements matched.
    return cJSON_True;
}


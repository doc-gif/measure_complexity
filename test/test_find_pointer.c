#include "test_find_pointer.h" // Include header for this test file
#include "test_helper.h"      // Include test helper functions and mocks

// cJSONUtils_FindPointerFromObjectTo 関数の宣言のために必要
// test_helper.c に run_test_find_pointer があり、それがこの関数を呼び出すため、
// こちらでも宣言が必要です。test_helper.h が cJSON/cJSON_Utils.h を
// インクルードしていれば、間接的に宣言が入りますが、明示的に含めるのが安全です。
#include "../cJSON/cJSON_Utils_minimize_inlining.h"

#include <stdio.h>
#include <stdlib.h> // For NULL

// --- Individual Test Case Functions for cJSONUtils_FindPointerFromObjectTo ---
// 各関数は特定のテストシナリオを実行し、run_test_find_pointer ヘルパーを使って結果を検証します。
// 構造体の構築と解放は、各テスト関数内で行います。

static void test_find_pointer_null_inputs(int* total, int* passed) {
    printf("Running test: Test 1 & 2: NULL Inputs\n"); // Combine NULL tests for clarity

    // Test 1: Object is NULL
    (*total)++;
    // Dummy target needed as run_test_find_pointer expects a target pointer even if object is NULL
    cJSON* dummy_target = create_json_value_number(1.0);
    (*passed) += run_test_find_pointer("Test 1: Object is NULL", NULL, dummy_target, NULL);
    free_json_tree(dummy_target); // Free the dummy item

    // Test 2: Target is NULL
    (*total)++;
    // Dummy object needed as run_test_find_pointer_find_pointer expects an object pointer even if target is NULL
    cJSON* dummy_object = create_json_container_object();
    (*passed) += run_test_find_pointer("Test 2: Target is NULL", dummy_object, NULL, NULL);
    free_json_tree(dummy_object); // Free the dummy item

    printf("---\n"); // Separator after combined test
}

static void test_find_pointer_root(int* total, int* passed) {
    (*total)++;
    // 新しいヘルパーを使用: オブジェクトコンテナを作成
    cJSON *root = create_json_container_object();
    (*passed) += run_test_find_pointer("Test 3: Target is root", root, root, "");
    // 構造体を解放する (root を解放すれば全体が解放される)
    free_json_tree(root);
}

static void test_find_pointer_direct_array_child(int* total, int* passed) {
    (*total)++;
    // 新しいヘルパーを使用: 配列コンテナを作成
    cJSON *root = create_json_container_array();
    // 新しいヘルパーを使用: 値アイテムを作成
    cJSON *item1_value = create_json_value_number(10.0);
    cJSON *item2_value = create_json_value_string("hello"); // ターゲットアイテム

    // 新しいヘルパーを使用: アイテムを配列に追加 (所有権は root に移る)
    add_item_to_array(root, item1_value);
    add_item_to_array(root, item2_value);

    (*passed) += run_test_find_pointer("Test 4: Direct Array child (index 1)", root, item2_value, "/1");
    // 構造体を解放する (root を解放すれば item1_value, item2_value も解放される)
    free_json_tree(root);
}

static void test_find_pointer_direct_object_child(int* total, int* passed) {
    (*total)++;
    // 新しいヘルパーを使用: オブジェクトコンテナを作成
    cJSON *root = create_json_container_object();
    // 新しいヘルパーを使用: 値アイテムを作成
    cJSON *item1_value = create_json_value_number(10.0);
    cJSON *item2_value = create_json_value_string("world"); // ターゲットアイテム

    // 新しいヘルパーを使用: アイテムをオブジェクトに追加 (キーを指定)
    add_item_to_object(root, "key1", item1_value);
    add_item_to_object(root, "key2", item2_value); // item2_value は key "key2" で追加される

    (*passed) += run_test_find_pointer("Test 5: Direct Object child (\"key2\")", root, item2_value, "/key2");
     // 構造体を解放する (root を解放すれば item1_value, item2_value も解放される)
    free_json_tree(root);
}

static void test_find_pointer_nested_array_object(int* total, int* passed) {
    (*total)++;
    // 構造: {"data": [1, {"value": 99}]}

    cJSON *root = create_json_container_object(); // {}
    cJSON* array_node = create_json_container_array(); // []
    cJSON* item1_value = create_json_value_number(1.0); // 1
    cJSON* object_node = create_json_container_object(); // {}
    cJSON* item2_value = create_json_value_number(99.0); // 99 (TARGET)

    add_item_to_object(root, "data", array_node); // {"data": []}
    add_item_to_array(array_node, item1_value);   // {"data": [1]}
    add_item_to_array(array_node, object_node);  // {"data": [1, {}]}
    add_item_to_object(object_node, "value", item2_value); // {"value": 99} -> {"data": [1, {"value": 99}]}

    (*passed) += run_test_find_pointer("Test 6: Nested Array/Object child (/data/1/value)", root, item2_value, "/data/1/value");
     // 構造体を解放する
    free_json_tree(root);
}

static void test_find_pointer_nested_object_array(int* total, int* passed) {
    (*total)++;
    // 構造: [{"nested": {"name": "test"}}, 42]

    cJSON *root = create_json_container_array(); // []
    cJSON* obj_node1 = create_json_container_object(); // {}
    cJSON* obj_node2 = create_json_container_object(); // {}
    cJSON* item1_value = create_json_value_string("test"); // "test" (TARGET)
    cJSON* item2_value = create_json_value_number(42.0); // 42

    add_item_to_array(root, obj_node1); // [{}]
    add_item_to_array(root, item2_value); // [{}, 42]
    add_item_to_object(obj_node1, "nested", obj_node2); // {"nested": {}} -> [{"nested": {}}, 42]
    add_item_to_object(obj_node2, "name", item1_value); // {"name": "test"} -> [{"nested": {"name": "test"}}, 42]

    (*passed) += run_test_find_pointer("Test 7: Nested Object/Array child (/0/nested/name)", root, item1_value, "/0/nested/name");
     // 構造体を解放する
    free_json_tree(root);
}

static void test_find_pointer_not_found_different_tree(int* total, int* passed) {
    (*total)++;
    // メインの構造体
    cJSON *root = create_json_container_object(); // {"a": 1}
    cJSON *item1_value = create_json_value_number(1.0);
    add_item_to_object(root, "a", item1_value);

    // メインの構造体とは関係ないターゲット
    cJSON* target_in_other_tree = create_json_value_string("other"); // Not linked to root

    (*passed) += run_test_find_pointer("Test 8: Target not found (different tree)", root, target_in_other_tree, NULL);

    // 両方の構造体を解放する
    free_json_tree(root);
    free_json_tree(target_in_other_tree); // これはメインツリーに含まれていないので別途解放が必要
}

static void test_find_pointer_encoded_key(int* total, int* passed) {
    (*total)++;
    // 新しいヘルパーを使用: オブジェクトコンテナを作成
    cJSON *root = create_json_container_object();
    // 新しいヘルパーを使用: 値アイテムを作成
    cJSON *item1_value = create_json_value_number(123.0); // ターゲットアイテム

    // 新しいヘルパーを使用: アイテムをオブジェクトに追加 (特殊文字を含むキー)
    // Note: test_helper.c の encode_string_as_pointer スタブに依存します。
    add_item_to_object(root, "key/with~special", item1_value);

    // expected_pointer は test_helper.c の encode_string_as_pointer の動作に合わせる
    (*passed) += run_test_find_pointer("Test 9: Object key with special chars (mocked encoding)", root, item1_value, "/key/with~special");
     // 構造体を解放する
    free_json_tree(root);
}


// --- Function to run all cJSONUtils_FindPointerFromObjectTo tests ---

void run_all_find_pointer_tests(int* total, int* passed) {
    printf("--- Running cJSONUtils_FindPointerFromObjectTo Tests ---\n");

    // 各テスト関数を呼び出す
    test_find_pointer_null_inputs(total, passed);
    test_find_pointer_root(total, passed);
    test_find_pointer_direct_array_child(total, passed);
    test_find_pointer_direct_object_child(total, passed);
    test_find_pointer_nested_array_object(total, passed); // Renamed from nested_array_child for clarity
    test_find_pointer_nested_object_array(total, passed); // Renamed from nested_object_child for clarity
    test_find_pointer_not_found_different_tree(total, passed);
    test_find_pointer_encoded_key(total, passed);

    printf("--- Finished cJSONUtils_FindPointerFromObjectTo Tests ---\n\n");
}
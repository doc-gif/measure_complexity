#ifndef TEST_HELPER_H_
#define TEST_HELPER_H_

#include "../cJSON/cJSON.h" // 実際のcJSONライブラリのヘッダー

// --- テスト用 cJSON 構造体構築ヘルパー (実際の cJSON API を使用) ---
cJSON* create_json_value_null(void);
cJSON* create_json_value_bool(cJSON_bool boolean);
cJSON* create_json_value_number(double num);
cJSON* create_json_value_string(const char *string);
// Raw型が必要なら追加: cJSON* create_json_value_raw(const char *raw); // create_patches は Raw 値を比較しない点に注意

cJSON* create_json_container_array(void);
cJSON* create_json_container_object(void);

// --- テスト用 cJSON アイテム追加ヘルパー (実際の cJSON API を使用) ---
void add_item_to_object(cJSON* parent, const char* key, cJSON* item);
void add_item_to_array(cJSON* parent, cJSON* item);

// --- テスト用 cJSON 構造体解放ヘルパー (実際の cJSON API を使用) ---
void free_json_tree(cJSON* item);

// --- テスト用 cJSON 配列クリアヘルパー ---
// 配列の全子要素を解放し、配列を空にする
void clear_json_array(cJSON* array);

// --- テスト実行ヘルパー ---
// Test runner for cJSONUtils_FindPointerFromObjectTo (uses create_json_find_pointer_test_data)
int run_test_find_pointer(const char* test_name, cJSON* object, const cJSON* target, const char* expected_pointer);

// --- パッチテスト用アサーションヘルパー ---
// 期待されるパッチオブジェクトを作成する (値は Deep Copy される)
cJSON* create_expected_patch(const char* op, const char* final_patch_path, const cJSON* value);

// 実際のパッチ配列と期待されるパッチ配列を比較する (Deep Comparison)
cJSON_bool compare_patch_arrays(cJSON* actual, cJSON* expected);


#endif // TEST_HELPER_H_
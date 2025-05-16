/*
  Copyright (c) 2009-2017 Dave Gamble and cJSON contributors

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

/* disable warnings about old C89 functions in MSVC */
#if !defined(_CRT_SECURE_NO_DEPRECATE) && defined(_MSC_VER)
#define _CRT_SECURE_NO_DEPRECATE
#endif

#ifdef __GNUCC__
#pragma GCC visibility push(default)
#endif
#if defined(_MSC_VER)
#pragma warning (push)
/* disable warning about single line comments in system headers */
#pragma warning (disable : 4001)
#endif

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <float.h>
#include <math.h>

#if defined(_MSC_VER)
#pragma warning (pop)
#endif
#ifdef __GNUCC__
#pragma GCC visibility pop
#endif

#include "cJSON_Utils.h"

/* define our own boolean type */
#ifdef true
#undef true
#endif
#define true ((cJSON_bool)1)

#ifdef false
#undef false
#endif
#define false ((cJSON_bool)0)

/* string comparison which doesn't consider NULL pointers equal */
static int compare_strings(const unsigned char *string1, const unsigned char *string2, const cJSON_bool case_sensitive)
{
    if ((string1 == NULL) || (string2 == NULL))
    {
        return 1;
    }

    if (string1 == string2)
    {
        return 0;
    }

    if (case_sensitive)
    {
        return strcmp((const char*)string1, (const char*)string2);
    }

    for(; tolower(*string1) == tolower(*string2); (void)string1++, string2++)
    {
        if (*string1 == '\0')
        {
            return 0;
        }
    }

    return tolower(*string1) - tolower(*string2);
}

/* copy a string while escaping '~' and '/' with ~0 and ~1 JSON pointer escape codes */
static void encode_string_as_pointer(unsigned char *destination, const unsigned char *source)
{
    for (; source[0] != '\0'; (void)source++, destination++)
    {
        if (source[0] == '/')
        {
            destination[0] = '~';
            destination[1] = '1';
            destination++;
        }
        else if (source[0] == '~')
        {
            destination[0] = '~';
            destination[1] = '0';
            destination++;
        }
        else
        {
            destination[0] = source[0];
        }
    }

    destination[0] = '\0';
}

/* 1. 初期条件とルートでの発見のハンドリングを抽出 */
static char *handle_initial_conditions(const cJSON * const object, const cJSON * const target)
{
    unsigned char *copy;
    size_t length;

    if ((object == NULL) || (target == NULL))
    {
        return NULL;
    }

    if (object == target)
    {
        /* found at the root */
        const unsigned char *string = (const unsigned char*)"";
        length = strlen((const char*)string) + sizeof(""); // Includes null terminator size

        copy = (unsigned char*) cJSON_malloc(length);
        if (copy == NULL)
        {
            return NULL; // Allocation failed
        }
        memcpy(copy, string, length);

        return (char*)copy;
    }

    return NULL; // Not found at initial conditions
}

/* 2. ターゲット発見後のパス構築 (配列) を抽出 */
static char *build_array_path_segment(size_t child_index, char *target_pointer_from_child)
{
    char *full_pointer;
    /* reserve enough memory for a 64 bit integer + '/' and '\0' */
    full_pointer = (char*)cJSON_malloc(strlen(target_pointer_from_child) + 20 + 1 + 1); // path_len + index_max_len + '/' + '\0'
    if (full_pointer == NULL)
    {
        cJSON_free(target_pointer_from_child); // Free the pointer from the child
        return NULL; // Allocation failed
    }

    /* check if conversion to unsigned long is valid */
    if (child_index > ULONG_MAX)
    {
        cJSON_free(target_pointer_from_child);
        cJSON_free(full_pointer);
        return NULL;
    }

    sprintf(full_pointer, "/%lu%s", (unsigned long)child_index, target_pointer_from_child); /* /<array_index><path> */
    cJSON_free(target_pointer_from_child); // Free the pointer from the child after using it

    return full_pointer;
}

/* 3. ターゲット発見後のパス構築 (オブジェクト) を抽出 */
static char *build_object_path_segment(const cJSON * const current_child, char *target_pointer_from_child)
{
    char *full_pointer;
    const unsigned char *string;
    size_t pointer_encoded_length = 0; // Initialize

    string = (unsigned char*)current_child->string;
    if (string != NULL) // Add NULL check for child string
    {
        for (pointer_encoded_length = 0; *string != '\0'; (void)string++, pointer_encoded_length++)
        {
            /* character needs to be escaped? */
            if ((*string == '~') || (*string == '/'))
            {
                pointer_encoded_length++;
            }
        }
    }
    else
    {
        /* Object key is NULL? This is an invalid state for a JSON object */
         cJSON_free(target_pointer_from_child);
        return NULL;
    }


    full_pointer = (char*)cJSON_malloc(strlen(target_pointer_from_child) + pointer_encoded_length + 1 + 1); // path_len + encoded_key_len + '/' + '\0'
    if (full_pointer == NULL)
    {
        cJSON_free(target_pointer_from_child); // Free the pointer from the child
        return NULL; // Allocation failed
    }
    full_pointer[0] = '/';
    encode_string_as_pointer((unsigned char*)full_pointer + 1, (unsigned char*)current_child->string); // current_child->string is non-NULL here
    strcat(full_pointer, target_pointer_from_child);
    cJSON_free(target_pointer_from_child); // Free the pointer from the child after using it

    return full_pointer;
}


/* 4. 子要素の再帰的探索を分離 */
static char *find_pointer_in_children(const cJSON * const object, const cJSON * const target)
{
    cJSON *current_child = NULL;
    size_t child_index = 0;

    for (current_child = object->child; current_child != NULL; (void)(current_child = current_child->next), child_index++)
    {
        char *target_pointer = (char*)cJSONUtils_FindPointerFromObjectTo(current_child, target);

        /* found the target in a child's subtree? */
        if (target_pointer != NULL)
        {
            if (cJSON_IsArray(object))
            {
                /* Build path segment for array child and prepend */
                return build_array_path_segment(child_index, target_pointer);
            }

            if (cJSON_IsObject(object))
            {
                /* Build path segment for object child (key) and prepend */
                /* Check if the child has a string key */
                if (current_child->string == NULL)
                {
                    /* This child is in an object but has no key string - invalid JSON */
                    cJSON_free(target_pointer);
                    return NULL;
                }
                return build_object_path_segment(current_child, target_pointer);
            }

            /* Target found in a child, but parent is neither Array nor Object.
             * This implies the target is in a primitive type's child, which is impossible.
             * This case should ideally not be reached if cJSON structure is valid. */
             cJSON_free(target_pointer); // Free the result from recursion
            return NULL;
        }
    }

    /* Target not found in any child */
    return NULL;
}


CJSON_PUBLIC(char *) cJSONUtils_FindPointerFromObjectTo(const cJSON * const object, const cJSON * const target)
{
    /* Handle initial NULL inputs or target being the object itself */
    char *result = handle_initial_conditions(object, target);
    if (result != NULL)
    {
        return result;
    }

    /* If not found at the root, recursively search children if the object is a container */
    if (cJSON_IsArray(object) || cJSON_IsObject(object))
    {
         return find_pointer_in_children(object, target);
    }

    /* Object is a primitive type and not the target, and has no children to search */
    return NULL;
}

/* sort lists using mergesort */
static cJSON *sort_list(cJSON *list, const cJSON_bool case_sensitive)
{
    cJSON *smaller;
    cJSON *first = list;
    cJSON *second = list;
    cJSON *current_item = list;
    cJSON *result = list;
    cJSON *result_tail = NULL;

    if ((list == NULL) || (list->next == NULL))
    {
        /* One entry is sorted already. */
        return result;
    }

    while ((current_item != NULL) && (current_item->next != NULL) && (compare_strings((unsigned char*)current_item->string, (unsigned char*)current_item->next->string, case_sensitive) < 0))
    {
        /* Test for list sorted. */
        current_item = current_item->next;
    }
    if ((current_item == NULL) || (current_item->next == NULL))
    {
        /* Leave sorted lists unmodified. */
        return result;
    }

    /* reset pointer to the beginning */
    current_item = list;
    while (current_item != NULL)
    {
        /* Walk two pointers to find the middle. */
        second = second->next;
        current_item = current_item->next;
        /* advances current_item two steps at a time */
        if (current_item != NULL)
        {
            current_item = current_item->next;
        }
    }
    if ((second != NULL) && (second->prev != NULL))
    {
        /* Split the lists */
        second->prev->next = NULL;
        second->prev = NULL;
    }

    /* Recursively sort the sub-lists. */
    first = sort_list(first, case_sensitive);
    second = sort_list(second, case_sensitive);
    result = NULL;

    /* Merge the sub-lists */
    while ((first != NULL) && (second != NULL))
    {
        smaller = NULL;
        if (compare_strings((unsigned char*)first->string, (unsigned char*)second->string, case_sensitive) < 0)
        {
            smaller = first;
        }
        else
        {
            smaller = second;
        }

        if (result == NULL)
        {
            /* start merged list with the smaller element */
            result_tail = smaller;
            result = smaller;
        }
        else
        {
            /* add smaller element to the list */
            result_tail->next = smaller;
            smaller->prev = result_tail;
            result_tail = smaller;
        }

        if (first == smaller)
        {
            first = first->next;
        }
        else
        {
            second = second->next;
        }
    }

    if (first != NULL)
    {
        /* Append rest of first list. */
        if (result == NULL)
        {
            return first;
        }
        result_tail->next = first;
        first->prev = result_tail;
    }
    if (second != NULL)
    {
        /* Append rest of second list */
        if (result == NULL)
        {
            return second;
        }
        result_tail->next = second;
        second->prev = result_tail;
    }

    return result;
}

/* 1. 基本パッチオブジェクト（opフィールド付き）の作成を抽出 */
static cJSON *create_base_patch_object(const unsigned char * const operation)
{
    cJSON *patch = cJSON_CreateObject();
    if (patch == NULL)
    {
        return NULL;
    }
    cJSON_AddItemToObject(patch, "op", cJSON_CreateString((const char*)operation));
    return patch;
}

/* 2. サフィックス付きフルパスの構築を抽出 */
static char *build_full_path_with_suffix(const unsigned char * const path, const unsigned char *suffix)
{
    unsigned char *full_path;
    const unsigned char *string;
    size_t suffix_length, path_length;
    size_t pointer_encoded_length;

    string = suffix;
    for (pointer_encoded_length = 0; *string != '\0'; (void)string++, pointer_encoded_length++)
    {
        /* character needs to be escaped? */
        if ((*string == '~') || (*string == '/'))
        {
            pointer_encoded_length++;
        }
    }

    path_length = strlen((const char*)path);
    // Allocate for path, '/', encoded_suffix, and null terminator
    full_path = (unsigned char*)cJSON_malloc(path_length + 1 + pointer_encoded_length + 1); // +1 for '/', +1 for '\0'
     if (full_path == NULL)
    {
        return NULL;
    }

    sprintf((char*)full_path, "%s/", (const char*)path); // Copy path and add '/'
    // Encode suffix starting after '/'
    encode_string_as_pointer(full_path + path_length + 1, suffix);
    // The encode_string_as_pointer is assumed to handle null termination

    return (char*)full_path; // Caller must free this
}


static void compose_patch(cJSON * const patches, const unsigned char * const operation, const unsigned char * const path, const unsigned char *suffix, const cJSON * const value)
{
    cJSON *patch = NULL;
    char *path_string = NULL; // To hold the path string for cJSON_CreateString

    if ((patches == NULL) || (operation == NULL) || (path == NULL))
    {
        return;
    }

    patch = create_base_patch_object(operation);
    if (patch == NULL)
    {
        return;
    }

    if (suffix == NULL)
    {
        path_string = (char*)path; // Use the original path directly
    }
    else
    {
        // Build the complex path with suffix and encoding
        path_string = build_full_path_with_suffix(path, suffix);
        if (path_string == NULL)
        {
            cJSON_Delete(patch); // Assume cJSON_Delete exists and frees patch
            return;
        }
    }

    cJSON_AddItemToObject(patch, "path", cJSON_CreateString(path_string));

    // Free the allocated path string if it was built with a suffix
    if (suffix != NULL && path_string != NULL)
    {
        cJSON_free(path_string);
    }

    if (value != NULL)
    {
        cJSON_AddItemToObject(patch, "value", cJSON_Duplicate(value, 1));
    }

    cJSON_AddItemToArray(patches, patch);
}

/* create_patches 関数の前方宣言 (再帰呼び出しのために必要) */
void create_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive);


/* === オブジェクト比較に関するヘルパー関数 === */

/* オブジェクトキーのパスセグメント '/encoded_key' を構築 */
static char *build_object_key_path_segment(const unsigned char * const path, const unsigned char *key_string)
{
    unsigned char *full_path;
    const unsigned char *string;
    size_t path_length, key_encoded_length;

    string = key_string;
    for (key_encoded_length = 0; *string != '\0'; (void)string++, key_encoded_length++)
    {
        /* character needs to be escaped? */
        if ((*string == '~') || (*string == '/'))
        {
            key_encoded_length++;
        }
    }

    path_length = strlen((const char*)path);
    // Allocate for path, '/', encoded_key, and null terminator
    full_path = (unsigned char*)cJSON_malloc(path_length + 1 + key_encoded_length + 1);
     if (full_path == NULL)
    {
        return NULL;
    }

    sprintf((char*)full_path, "%s/", (const char*)path); // Copy path and add '/'
    // Encode key starting after '/'
    encode_string_as_pointer(full_path + path_length + 1, key_string);
    // The encode_string_as_pointer is assumed to handle null termination

    return (char*)full_path; // Caller must free this
}

/* オブジェクト比較ループ内でキーが一致した場合の処理 */
static void handle_matching_object_key(cJSON * const patches, const unsigned char * const path, cJSON *from_child, cJSON *to_child, const cJSON_bool case_sensitive)
{
    char *new_path = build_object_key_path_segment(path, (unsigned char*)from_child->string);
    if (new_path == NULL)
    {
        return; // Allocation failed
    }

    /* create a patch for the element (recursive call) */
    create_patches(patches, (unsigned char*)new_path, from_child, to_child, case_sensitive);
    cJSON_free(new_path);
}

/* オブジェクト比較ループ内で from にのみキーが存在する場合 (削除) の処理 */
static void handle_removed_object_key(cJSON * const patches, const unsigned char * const path, cJSON *from_child)
{
    /* object element doesn't exist in 'to' --> remove it */
    compose_patch(patches, (const unsigned char*)"remove", path, (unsigned char*)from_child->string, NULL);
}

/* オブジェクト比較ループ内で to にのみキーが存在する場合 (追加) の処理 */
static void handle_added_object_key(cJSON * const patches, const unsigned char * const path, cJSON *to_child)
{
    /* object element doesn't exist in 'from' --> add it */
    compose_patch(patches, (const unsigned char*)"add", path, (unsigned char*)to_child->string, to_child);
}

/* オブジェクトの子要素を比較するメインループの処理 */
static void compare_object_children_loop(cJSON * const patches, const unsigned char * const path, cJSON *from_child, cJSON *to_child, const cJSON_bool case_sensitive)
{
     while ((from_child != NULL) || (to_child != NULL))
    {
        int diff;
        if (from_child == NULL)
        {
            diff = 1; // to_child is not NULL
        }
        else if (to_child == NULL)
        {
            diff = -1; // from_child is not NULL
        }
        else
        {
            diff = compare_strings((unsigned char*)from_child->string, (unsigned char*)to_child->string, case_sensitive);
        }

        if (diff == 0)
        {
            /* both object keys are the same */
            handle_matching_object_key(patches, path, from_child, to_child, case_sensitive);
            from_child = from_child->next;
            to_child = to_child->next;
        }
        else if (diff < 0)
        {
            /* object element doesn't exist in 'to' --> remove it */
            handle_removed_object_key(patches, path, from_child);
            from_child = from_child->next;
        }
        else /* diff > 0 */
        {
            /* object element doesn't exist in 'from' --> add it */
            handle_added_object_key(patches, path, to_child);
            to_child = to_child->next;
        }
    }
}

/* オブジェクトタイプの場合の差分計算とパッチ生成 */
static void compare_objects_and_patch(cJSON * const patches, const unsigned char * const path, cJSON *from, cJSON *to, const cJSON_bool case_sensitive)
{
     cJSON *from_child = NULL;
     cJSON *to_child = NULL;

    if (from != NULL) {
        from->child = sort_list(from->child, case_sensitive);
    }
    if (to != NULL) {
        to->child = sort_list(to->child, case_sensitive);
    }

    from_child = from->child;
    to_child = to->child;

    compare_object_children_loop(patches, path, from_child, to_child, case_sensitive);
}


/* === 配列比較に関するヘルパー関数 === */

/* 配列比較ループ内で共通要素の差分を生成 */
static void compare_common_array_elements_and_patch(cJSON * const patches, unsigned char *new_path_buffer, const unsigned char * const path, cJSON *from_child, cJSON *to_child, size_t *index, const cJSON_bool case_sensitive)
{
     /* generate patches for all array elements that exist in both "from" and "to" */
    for (; (from_child != NULL) && (to_child != NULL); (void)(from_child = from_child->next), (void)(to_child = to_child->next), (*index)++)
    {
        /* check if conversion to unsigned long is valid */
        if (*index > ULONG_MAX)
        {
            /* Error case: index exceeds ULONG_MAX. Original code returns early. */
             break; // Exit loop, buffer will be freed later
        }
        sprintf((char*)new_path_buffer, "%s/%lu", path, (unsigned long)*index); /* path of the current array element */
        create_patches(patches, new_path_buffer, from_child, to_child, case_sensitive);
    }
}

/* 配列比較ループ内で from にのみ要素が存在する場合 (削除) の処理 */
static void handle_removed_array_elements_and_patch(cJSON * const patches, unsigned char *new_path_buffer, const unsigned char * const path, cJSON *from_child, size_t *index)
{
    /* remove leftover elements from 'from' that are not in 'to' */
    for (; (from_child != NULL); (void)(from_child = from_child->next), (*index)++)
    {
        /* check if conversion to unsigned long is valid */
        if (*index > ULONG_MAX)
        {
             /* Error case: index exceeds ULONG_MAX. Original code returns early. */
             break; // Exit loop, buffer will be freed later
        }
         /* NOTE: Original code used index as suffix *without* a preceding '/', which is likely not standard JSON Pointer.
            Sticking to original logic for extraction, but this might be a bug. Standard remove path is '/index'.
            If corrected, the sprintf should be '%lu' not path/lu. Let's keep original sprintf for the suffix part.
         */
        sprintf((char*)new_path_buffer, "%lu", (unsigned long)*index); // Constructs the suffix "index"
        compose_patch(patches, (const unsigned char*)"remove", path, new_path_buffer, NULL); // Uses path + "index"
    }
}

/* 配列比較ループ内で to にのみ要素が存在する場合 (追加) の処理 */
static void handle_added_array_elements_and_patch(cJSON * const patches, const unsigned char * const path, cJSON *to_child)
{
     /* add new elements in 'to' that were not in 'from' */
    for (; (to_child != NULL); (void)(to_child = to_child->next)/* Original code increments index here too, but it's unused */)
    {
        compose_patch(patches, (const unsigned char*)"add", path, (const unsigned char*)"-", to_child);
    }
}


/* 配列タイプの場合の差分計算とパッチ生成 */
static void compare_arrays_and_patch(cJSON * const patches, const unsigned char * const path, cJSON *from, cJSON *to, const cJSON_bool case_sensitive)
{
    size_t index = 0;
    cJSON *from_child = from->child;
    cJSON *to_child = to->child;
    unsigned char *new_path_buffer = (unsigned char*)cJSON_malloc(strlen((const char*)path) + 20 + sizeof("/")); /* Allow space for 64bit int + '/' + '\0' */

    if (new_path_buffer == NULL)
    {
        return; // Allocation failed
    }

    compare_common_array_elements_and_patch(patches, new_path_buffer, path, from_child, to_child, &index, case_sensitive);

    /* After the common elements loop, from_child/to_child point to the first differing element or are NULL */
    handle_removed_array_elements_and_patch(patches, new_path_buffer, path, from_child, &index);

    /* After the remove loop, from_child is NULL. to_child points to the first added element or is NULL */
    /* NOTE: The original code increments index in this loop too, but it's not used in compose_patch("-") */
    handle_added_array_elements_and_patch(patches, path, to_child);

    cJSON_free(new_path_buffer);
}


/* === プリミティブタイプ比較に関するヘルパー関数 === */

/* 数値タイプの場合の差分計算とパッチ生成 */
static void compare_numbers_and_patch(cJSON * const patches, const unsigned char * const path, const cJSON * const from, const cJSON * const to)
{
    double a = from->valuedouble;
    double b = to->valuedouble;

    /* Compare integers first for exact match on int values */
    if (from->valueint != to->valueint)
    {
         compose_patch(patches, (const unsigned char*)"replace", path, NULL, to);
         return;
    }

    /* Then compare doubles with tolerance */
    double maxVal = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    cJSON_bool compared_double = (fabs(a - b) <= maxVal * DBL_EPSILON);

    if (!compared_double)
    {
        compose_patch(patches, (const unsigned char*)"replace", path, NULL, to);
    }
}

/* 文字列タイプの場合の差分計算とパッチ生成 */
static void compare_strings_and_patch(cJSON * const patches, const unsigned char * const path, const cJSON * const from, const cJSON * const to)
{
    if (strcmp(from->valuestring, to->valuestring) != 0)
    {
        compose_patch(patches, (const unsigned char*)"replace", path, NULL, to);
    }
}


/* === 初期チェックとタイプ不一致のハンドリング === */

/* 初期 NULL チェックと異なるタイプの場合の処理 */
/* 戻り値: 0で処理を継続、非0で処理完了(パッチ生成含む)またはエラー */
static int handle_initial_checks_and_type_mismatch(cJSON * const patches, const unsigned char * const path, const cJSON * const from, const cJSON * const to)
{
    if ((from == NULL) || (to == NULL))
    {
        return 1; // Indicate processing should stop
    }

    if ((from->type & 0xFF) != (to->type & 0xFF))
    {
        compose_patch(patches, (const unsigned char*)"replace", path, NULL, to);
        return 1; // Indicate processing should stop
    }

    return 0; // Indicate processing should continue
}


/* === メイン関数 === */

/* 差分計算とパッチ生成の再帰関数 */
void create_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive)
{
    if (handle_initial_checks_and_type_mismatch(patches, path, from, to) != 0)
    {
        return;
    }

    switch (from->type & 0xFF)
    {
        case cJSON_Number:
            compare_numbers_and_patch(patches, path, from, to);
            break; // Use break instead of return to simplify switch cases

        case cJSON_String:
            compare_strings_and_patch(patches, path, from, to);
            break; // Use break

        case cJSON_Array:
            compare_arrays_and_patch(patches, path, from, to, case_sensitive);
            break; // Use break

        case cJSON_Object:
            compare_objects_and_patch(patches, path, from, to, case_sensitive);
            break; // Use break

        default:
            /* Primitive types NULL, FALSE, TRUE are handled by type mismatch or compare_primitives_and_patch if extracted */
            /* If they reach here, it means no specific comparison needed or type is unsupported. */
            /* Original code did nothing for default. */
            break;
    }
    /* For types like NULL, FALSE, TRUE, comparison logic could be extracted similarly to number/string if needed */
}

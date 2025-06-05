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
/* math.h is already included above, no need to include it twice */
/* #include <math.h> */


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

/** リファクタリング後に、関数宣言前に他の関数で再帰呼び出ししエラーになっていたため、手動で追加 **/
void create_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive);

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

/** リファクタリングにより新たに定義された **/
/* Calculate the length of a string after JSON pointer encoding */
static size_t calculate_json_pointer_encoded_string_length(const unsigned char *string_to_encode)
{
    size_t encoded_length = 0;
    const unsigned char *current_char = string_to_encode;

    if (string_to_encode == NULL)
    {
        return 0;
    }

    for (encoded_length = 0; *current_char != '\0'; (void)current_char++, encoded_length++)
    {
        /* character needs to be escaped? */
        if ((*current_char == '~') || (*current_char == '/'))
        {
            encoded_length++;
        }
    }
    return encoded_length;
}

/** リファクタリングにより新たに定義された **/
/* Create a JSON pointer for an array element */
static unsigned char* create_json_pointer_for_array_element(const unsigned char *base_json_pointer, size_t array_index, const unsigned char *existing_suffix_json_pointer)
{
    unsigned char *full_pointer = NULL;
    size_t base_len = 0;
    size_t suffix_len = 0;

    base_len = strlen((const char*)base_json_pointer); /* Should be empty for the initial call from FindPointerFromObjectTo */
    suffix_len = strlen((const char*)existing_suffix_json_pointer);

    /* reserve enough memory for a 64 bit integer + '/' and '\0' and base and suffix */
    /* Max len of 64-bit unsigned long is 20 chars. Add 1 for '/', 1 for '\0'. */
    full_pointer = (unsigned char*)cJSON_malloc(base_len + suffix_len + 20 + 2);
    if (full_pointer == NULL)
    {
        return NULL;
    }

    /* check if conversion to unsigned long is valid
     * This should be eliminated at compile time by dead code elimination
     * if size_t is an alias of unsigned long, or if it is bigger */
    if (array_index > ULONG_MAX)
    {
        cJSON_free(full_pointer);
        return NULL;
    }
    sprintf((char*)full_pointer, "%s/%lu%s", base_json_pointer, (unsigned long)array_index, existing_suffix_json_pointer); /* /<array_index><path> */

    return full_pointer;
}

/** リファクタリングにより新たに定義された **/
/* Create a JSON pointer for an object member */
static unsigned char* create_json_pointer_for_object_member(const unsigned char *base_json_pointer, const unsigned char *member_key, const unsigned char *existing_suffix_json_pointer)
{
    unsigned char *full_pointer = NULL;
    size_t pointer_encoded_length = 0;
    size_t base_len = 0;
    size_t suffix_len = 0;

    base_len = strlen((const char*)base_json_pointer);
    suffix_len = strlen((const char*)existing_suffix_json_pointer);
    pointer_encoded_length = calculate_json_pointer_encoded_string_length(member_key);

    /* Add 1 for '/', 1 for '\0' */
    full_pointer = (unsigned char*)cJSON_malloc(base_len + pointer_encoded_length + suffix_len + 2);
    if (full_pointer == NULL)
    {
        return NULL;
    }

    sprintf((char*)full_pointer, "%s/", base_json_pointer);
    encode_string_as_pointer(full_pointer + base_len + 1, member_key);
    strcat((char*)full_pointer, (const char*)existing_suffix_json_pointer);

    return full_pointer;
}


CJSON_PUBLIC(char *) cJSONUtils_FindPointerFromObjectTo(const cJSON * const object, const cJSON * const target)
{
    unsigned char *copy = NULL;
    unsigned char *target_pointer_str = NULL;
    unsigned char *full_pointer_str = NULL;
    const unsigned char *found_path_segment = NULL;
    size_t length = 0;
    size_t child_index = 0;
    cJSON *current_child = NULL;

    if ((object == NULL) || (target == NULL))
    {
        return NULL;
    }

    if (object == target)
    {
        /* found */
        found_path_segment = (const unsigned char *)""; /* Empty path for self */
        length = strlen((const char*)found_path_segment) + 1; /* +1 for '\0' */
        copy = (unsigned char*) cJSON_malloc(length);
        if (copy == NULL)
        {
            return NULL;
        }
        memcpy(copy, found_path_segment, length);

        return (char*)copy;
    }

    /* recursively search all children of the object or array */
    for (current_child = object->child; current_child != NULL; (void)(current_child = current_child->next), child_index++)
    {
        target_pointer_str = (unsigned char*)cJSONUtils_FindPointerFromObjectTo(current_child, target);
        /* found the target? */
        if (target_pointer_str != NULL)
        {
            if (cJSON_IsArray(object))
            {
                full_pointer_str = create_json_pointer_for_array_element((const unsigned char*)"", child_index, target_pointer_str);
                cJSON_free(target_pointer_str); /* original suffix is now part of full_pointer_str */
                if (full_pointer_str == NULL) return NULL; /* Allocation or other error in helper */
                return (char*)full_pointer_str;
            }

            if (cJSON_IsObject(object))
            {
                full_pointer_str = create_json_pointer_for_object_member((const unsigned char*)"", (unsigned char*)current_child->string, target_pointer_str);
                cJSON_free(target_pointer_str); /* original suffix is now part of full_pointer_str */
                if (full_pointer_str == NULL) return NULL; /* Allocation or other error in helper */
                return (char*)full_pointer_str;
            }

            /* Should not happen if object is Array or Object and has children */
            /* If current_child is part of some other structure not handled, or if object->type is unexpected */
            cJSON_free(target_pointer_str);
            return NULL; /* Or handle error appropriately */
        }
    }

    /* not found */
    return NULL;
}

/** リファクタリングにより新たに定義された **/
/* Find the middle node of a cJSON list */
static cJSON* find_middle_node_of_list(cJSON *list_head)
{
    cJSON *slow_ptr = list_head;
    cJSON *fast_ptr = list_head;

    if (list_head == NULL)
    {
        return NULL;
    }

    while (fast_ptr != NULL && fast_ptr->next != NULL)
    {
        slow_ptr = slow_ptr->next;
        fast_ptr = fast_ptr->next;
        if (fast_ptr != NULL) /* Advance fast_ptr a second step */
        {
            fast_ptr = fast_ptr->next;
        }
    }
    return slow_ptr; /* slow_ptr is at or just before the middle */
}

/** リファクタリングにより新たに定義された **/
/* Merge two sorted cJSON item lists */
static cJSON* merge_sorted_json_item_lists(cJSON *first_list, cJSON *second_list, const cJSON_bool case_sensitive)
{
    cJSON *merged_result_head = NULL;
    cJSON *merged_result_tail = NULL;
    cJSON *smaller_item = NULL;

    if (first_list == NULL) return second_list;
    if (second_list == NULL) return first_list;

    while ((first_list != NULL) && (second_list != NULL))
    {
        smaller_item = NULL;
        if (compare_strings((unsigned char*)first_list->string, (unsigned char*)second_list->string, case_sensitive) < 0)
        {
            smaller_item = first_list;
            first_list = first_list->next;
        }
        else
        {
            smaller_item = second_list;
            second_list = second_list->next;
        }

        if (merged_result_head == NULL)
        {
            merged_result_head = smaller_item;
            merged_result_tail = smaller_item;
        }
        else
        {
            merged_result_tail->next = smaller_item;
            smaller_item->prev = merged_result_tail;
            merged_result_tail = smaller_item;
        }
    }

    /* Append remaining elements of first_list or second_list */
    if (first_list != NULL)
    {
        merged_result_tail->next = first_list;
        first_list->prev = merged_result_tail;
    }
    else if (second_list != NULL)
    {
        merged_result_tail->next = second_list;
        second_list->prev = merged_result_tail;
    }
    
    /* Ensure the merged list is properly terminated at the new tail if it exists */
    if (merged_result_tail != NULL && merged_result_tail->next == NULL) {
        /* This is fine, tail already points to the last element. */
    } else if (merged_result_tail != NULL) {
        /* Find the true tail if lists were merged in a way that merged_result_tail isn't the absolute end. */
        while(merged_result_tail->next) merged_result_tail = merged_result_tail->next;
    }


    return merged_result_head;
}


/* sort lists using mergesort */
static cJSON *sort_list(cJSON *list, const cJSON_bool case_sensitive)
{
    cJSON *first_half = list;
    cJSON *second_half = NULL;
    cJSON *middle_node = NULL;
    cJSON *current_item_check_sorted = list; /* For checking if already sorted */


    if ((list == NULL) || (list->next == NULL))
    {
        /* One entry is sorted already. */
        return list;
    }

    /* Test for list already sorted. */
    while ((current_item_check_sorted != NULL) && (current_item_check_sorted->next != NULL) &&
           (compare_strings((unsigned char*)current_item_check_sorted->string, (unsigned char*)current_item_check_sorted->next->string, case_sensitive) <= 0)) /* allow equals */
    {
        current_item_check_sorted = current_item_check_sorted->next;
    }
    if ((current_item_check_sorted == NULL) || (current_item_check_sorted->next == NULL))
    {
        /* Leave sorted lists unmodified. */
        return list;
    }

    middle_node = find_middle_node_of_list(list);
    if (middle_node != NULL && middle_node->prev != NULL)
    {
        /* Split the lists */
        second_half = middle_node;
        middle_node->prev->next = NULL; /* Terminate first half */
        middle_node->prev = NULL;       /* Detach second half */
    } else if (middle_node != NULL && middle_node == list && list->next != NULL) {
        /* This case can happen if find_middle_node_of_list returns the head for a 2-element list,
           or if the list is very short. We need to ensure `second_half` is the element after head.
        */
        second_half = list->next;
        list->next = NULL; /* Terminate first half (which is just list) */
        if(second_half) second_half->prev = NULL; /* Detach second half */
    } else {
        /* Failsafe or list is too small to split further (e.g. 1 element, already handled) */
        /* Or find_middle_node_of_list returned NULL on a non-empty list (should not happen) */
        return list; /* Cannot split, or already sorted */
    }


    /* Recursively sort the sub-lists. */
    first_half = sort_list(first_half, case_sensitive);
    second_half = sort_list(second_half, case_sensitive);

    /* Merge the sub-lists */
    return merge_sorted_json_item_lists(first_half, second_half, case_sensitive);
}

/** リファクタリングにより新たに定義された **/
/* Build a JSON patch path by appending an encoded suffix to a base path. */
static unsigned char* build_json_patch_path_with_encoded_suffix(const unsigned char *base_path, const unsigned char *suffix_to_encode_and_append)
{
    unsigned char *full_path = NULL;
    size_t encoded_suffix_len = 0;
    size_t base_path_len = 0;

    if (base_path == NULL || suffix_to_encode_and_append == NULL) return NULL;

    base_path_len = strlen((const char*)base_path);
    encoded_suffix_len = calculate_json_pointer_encoded_string_length(suffix_to_encode_and_append);

    /* +1 for '/' separator, +1 for '\0' terminator */
    full_path = (unsigned char*)cJSON_malloc(base_path_len + 1 + encoded_suffix_len + 1);
    if (full_path == NULL)
    {
        return NULL;
    }

    if (base_path_len == 0 && strcmp((const char*)base_path, "") == 0) {
        /* If base_path is empty string "", path should start with "/" only if suffix is not empty.
           JSON Patch paths for root operations might be "" for "replace" op, for example.
           If suffix is like "foo", path becomes "/foo".
           If base_path is already "/obj", suffix "foo", path becomes "/obj/foo".
           The sprintf below handles the "/" correctly if base_path is empty or not.
        */
         sprintf((char*)full_path, "/"); /* Start with / if base_path is truly empty */
         encode_string_as_pointer(full_path + 1, suffix_to_encode_and_append);
    } else {
        sprintf((char*)full_path, "%s/", (const char*)base_path);
        encode_string_as_pointer(full_path + base_path_len + 1, suffix_to_encode_and_append);
    }


    return full_path;
}


static void compose_patch(cJSON * const patches, const unsigned char * const operation, const unsigned char * const path, const unsigned char *suffix, const cJSON * const value)
{
    unsigned char *final_path_str = NULL;
    cJSON *patch_item = NULL;
    cJSON *path_item_json = NULL;

    if ((patches == NULL) || (operation == NULL) || (path == NULL))
    {
        return;
    }

    patch_item = cJSON_CreateObject();
    if (patch_item == NULL)
    {
        return;
    }
    cJSON_AddItemToObject(patch_item, "op", cJSON_CreateString((const char*)operation));

    if (suffix == NULL) /* No suffix, use path directly */
    {
        path_item_json = cJSON_CreateString((const char*)path);
    }
    else /* Suffix provided, build combined path */
    {
        final_path_str = build_json_patch_path_with_encoded_suffix(path, suffix);
        if (final_path_str == NULL)
        {
            cJSON_Delete(patch_item);
            return;
        }
        path_item_json = cJSON_CreateString((const char*)final_path_str);
        cJSON_free(final_path_str);
    }
    
    if (path_item_json == NULL) {
        cJSON_Delete(patch_item);
        return;
    }
    cJSON_AddItemToObject(patch_item, "path", path_item_json);


    if (value != NULL)
    {
        cJSON_AddItemToObject(patch_item, "value", cJSON_Duplicate(value, 1));
    }
    cJSON_AddItemToArray(patches, patch_item);
}

/** リファクタリングにより新たに定義された **/
static void process_common_array_elements_for_patch(cJSON *patches_array, const unsigned char *current_path, cJSON **from_array_item_ptr, cJSON **to_array_item_ptr, size_t *index_ptr, const cJSON_bool case_sensitive)
{
    unsigned char *element_path = NULL;
    cJSON *from_child_local = *from_array_item_ptr;
    cJSON *to_child_local = *to_array_item_ptr;
    size_t current_index = *index_ptr;

    element_path = (unsigned char*)cJSON_malloc(strlen((const char*)current_path) + 20 + 2); /* Allow space for 64bit int + '/' + '\0' */
    if (element_path == NULL) return; /* Early exit on allocation failure */

    for (; (from_child_local != NULL) && (to_child_local != NULL);
         (void)(from_child_local = from_child_local->next), (void)(to_child_local = to_child_local->next), current_index++)
    {
        if (current_index > ULONG_MAX)
        {
            /* Potentially free element_path if it's always allocated once outside loop */
            break; /* or return error */
        }
        sprintf((char*)element_path, "%s/%lu", current_path, (unsigned long)current_index);
        create_patches(patches_array, element_path, from_child_local, to_child_local, case_sensitive);
    }

    cJSON_free(element_path);
    *from_array_item_ptr = from_child_local;
    *to_array_item_ptr = to_child_local;
    *index_ptr = current_index;
}

/** リファクタリングにより新たに定義された **/
static void process_removed_array_elements_for_patch(cJSON *patches_array, const unsigned char *current_path, cJSON **from_array_item_ptr, size_t *index_ptr)
{
    unsigned char *suffix_str = NULL;
    cJSON *from_child_local = *from_array_item_ptr;
    size_t current_index = *index_ptr; /* This index is for the *original* position of removal */

    suffix_str = (unsigned char*)cJSON_malloc(20 + 1); /* Allow space for 64bit int + '\0' for suffix */
    if (suffix_str == NULL) return;

    for (; (from_child_local != NULL); (void)(from_child_local = from_child_local->next)) /* Do not increment index here, path refers to shrinking array */
    {
        if (current_index > ULONG_MAX) /* This index indicates the element to remove from path */
        {
            break;
        }
        sprintf((char*)suffix_str, "%lu", (unsigned long)current_index);
        compose_patch(patches_array, (const unsigned char*)"remove", current_path, suffix_str, NULL);
        /* Note: For array removal, the index usually refers to the state *before* previous removals in the same patch operation sequence.
           However, JSON Patch RFC 6902 specifies indices refer to current state of array for each op.
           If multiple removals are generated, removing element at `index` shifts subsequent elements.
           This simple loop removes from the *current* `index` each time, effectively always removing the element *at that position*.
           This is correct if `compose_patch` generates one op at a time for `current_path` + `suffix_str(index)`.
        */
    }
    cJSON_free(suffix_str);
    *from_array_item_ptr = from_child_local;
    /* index_ptr is not updated further here as it represented the start of removals */
}

/** リファクタリングにより新たに定義された **/
static void process_added_array_elements_for_patch(cJSON *patches_array, const unsigned char *current_path, cJSON **to_array_item_ptr, size_t *unused_index_ptr)
{
    cJSON *to_child_local = *to_array_item_ptr;
    /* size_t current_index = *unused_index_ptr; // Index not strictly needed for "add to end" */

    for (; (to_child_local != NULL); (void)(to_child_local = to_child_local->next) /*, current_index++ */)
    {
        compose_patch(patches_array, (const unsigned char*)"add", current_path, (const unsigned char*)"-", to_child_local);
    }

    *to_array_item_ptr = to_child_local;
    /* *unused_index_ptr = current_index; // If index was tracked */
}

/** リファクタリングにより新たに定義された **/
static void generate_patches_for_array_diff(cJSON *patches_array, const unsigned char *current_path, cJSON *from_array_node, cJSON *to_array_node, const cJSON_bool case_sensitive)
{
    cJSON *from_child_iter = NULL;
    cJSON *to_child_iter = NULL;
    size_t current_element_index = 0;
    /* new_path buffer is managed inside helper functions now */

    from_child_iter = from_array_node->child;
    to_child_iter = to_array_node->child;

    process_common_array_elements_for_patch(patches_array, current_path, &from_child_iter, &to_child_iter, &current_element_index, case_sensitive);
    process_removed_array_elements_for_patch(patches_array, current_path, &from_child_iter, &current_element_index);
    process_added_array_elements_for_patch(patches_array, current_path, &to_child_iter, &current_element_index); /* Index for add is usually '-' path */
}

/** リファクタリングにより新たに定義された **/
static void process_object_member_match_for_patch(cJSON *patches_array, const unsigned char *current_path, cJSON *from_object_child, cJSON *to_object_child, const cJSON_bool case_sensitive)
{
    unsigned char *member_path_str = NULL;
    
    member_path_str = build_json_patch_path_with_encoded_suffix(current_path, (unsigned char*)from_object_child->string);
    if (member_path_str == NULL)
    {
        return; /* Allocation failed */
    }

    create_patches(patches_array, member_path_str, from_object_child, to_object_child, case_sensitive);
    cJSON_free(member_path_str);
}

/** リファクタリングにより新たに定義された **/
static void process_object_member_removal_for_patch(cJSON *patches_array, const unsigned char *current_path, cJSON *from_object_child)
{
    compose_patch(patches_array, (const unsigned char*)"remove", current_path, (unsigned char*)from_object_child->string, NULL);
}

/** リファクタリングにより新たに定義された **/
static void process_object_member_addition_for_patch(cJSON *patches_array, const unsigned char *current_path, cJSON *to_object_child)
{
    compose_patch(patches_array, (const unsigned char*)"add", current_path, (unsigned char*)to_object_child->string, to_object_child);
}

/** リファクタリングにより新たに定義された **/
static void generate_patches_for_object_diff(cJSON *patches_array, const unsigned char *current_path, cJSON *from_object_node, cJSON *to_object_node, const cJSON_bool case_sensitive)
{
    cJSON *from_child_iter = NULL;
    cJSON *to_child_iter = NULL;
    int key_comparison_diff = 0;

    /* Sort children by key to allow linear comparison */
    if (from_object_node != NULL) { /* from_object_node is 'from' in create_patches, should not be NULL here */
        from_object_node->child = sort_list(from_object_node->child, case_sensitive);
    }
    if (to_object_node != NULL) { /* to_object_node is 'to' in create_patches, should not be NULL here */
        to_object_node->child = sort_list(to_object_node->child, case_sensitive);
    }

    from_child_iter = from_object_node->child;
    to_child_iter = to_object_node->child;

    while ((from_child_iter != NULL) || (to_child_iter != NULL))
    {
        if (from_child_iter == NULL)
        {
            key_comparison_diff = 1; /* from is exhausted, treat as to_child_iter->string > NULL */
        }
        else if (to_child_iter == NULL)
        {
            key_comparison_diff = -1; /* to is exhausted, treat as from_child_iter->string < NULL */
        }
        else
        {
            key_comparison_diff = compare_strings((unsigned char*)from_child_iter->string, (unsigned char*)to_child_iter->string, case_sensitive);
        }

        if (key_comparison_diff == 0) /* Keys match */
        {
            process_object_member_match_for_patch(patches_array, current_path, from_child_iter, to_child_iter, case_sensitive);
            from_child_iter = from_child_iter->next;
            to_child_iter = to_child_iter->next;
        }
        else if (key_comparison_diff < 0) /* Key in from_child_iter is smaller, so it's not in to_object_node */
        {
            process_object_member_removal_for_patch(patches_array, current_path, from_child_iter);
            from_child_iter = from_child_iter->next;
        }
        else /* Key in to_child_iter is smaller, so it's not in from_object_node */
        {
            process_object_member_addition_for_patch(patches_array, current_path, to_child_iter);
            to_child_iter = to_child_iter->next;
        }
    }
}


void create_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive)
{
    /* Variable declarations at the beginning */
    double a_val, b_val, max_val_double;
    cJSON_bool doubles_are_close_enough;


    if ((from == NULL) || (to == NULL))
    {
        return;
    }

    if ((from->type & 0xFF) != (to->type & 0xFF))
    {
        compose_patch(patches, (const unsigned char*)"replace", path, 0, to);
        return;
    }

    switch (from->type & 0xFF)
    {
        case cJSON_Number:
            a_val = from->valuedouble;
            b_val = to->valuedouble;

            max_val_double = fabs(a_val) > fabs(b_val) ? fabs(a_val) : fabs(b_val);
            /* Check if doubles are "close enough" */
            if (max_val_double < DBL_MIN) { /* Avoid division by zero if max_val_double is zero or too small */
                 doubles_are_close_enough = (fabs(a_val - b_val) <= DBL_EPSILON);
            } else {
                 doubles_are_close_enough = (fabs(a_val - b_val) <= max_val_double * DBL_EPSILON);
            }

            if ((from->valueint != to->valueint) || !doubles_are_close_enough)
            {
                compose_patch(patches, (const unsigned char*)"replace", path, NULL, to);
            }
            return;

        case cJSON_String:
            if (strcmp(from->valuestring, to->valuestring) != 0)
            {
                compose_patch(patches, (const unsigned char*)"replace", path, NULL, to);
            }
            return;

        case cJSON_Array:
            generate_patches_for_array_diff(patches, path, from, to, case_sensitive);
            return;

        case cJSON_Object:
            generate_patches_for_object_diff(patches, path, from, to, case_sensitive);
            return;

        default:
            /* cJSON_NULL, cJSON_True, cJSON_False: if types are same, they are equal. No patch needed. */
            /* cJSON_Raw: Not straightforward to compare, typically treated as strings if compared.
                          If types match and it's not String/Number/Array/Object, assume equal if types are same.
            */
            break;
    }
}
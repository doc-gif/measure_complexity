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
/* math.h already includes float.h in most implementations */

#if defined(_MSC_VER)
#pragma warning (pop)
#endif
#ifdef __GNUCC__
#pragma GCC visibility pop
#endif

#include "cJSON_Utils.h" /* Assuming cJSON.h is included within this or directly */
#include "cJSON.h" /* For cJSON types and functions like cJSON_IsArray, cJSON_malloc etc. */


/* define our own boolean type */
#ifdef true
#undef true
#endif
#define true ((cJSON_bool)1)

#ifdef false
#undef false
#endif
#define false ((cJSON_bool)0)

/* Forward declarations for static helper functions */
static size_t calculate_encoded_string_length(const unsigned char *input_string);
static char *create_empty_path_string(void);
static char *build_array_item_pointer_and_free_target(unsigned char *target_pointer_to_free, size_t child_index_param);
static char *build_object_property_pointer_and_free_target(unsigned char *target_pointer_to_free, const char *child_key);
static cJSON_bool is_list_sorted_or_trivial(cJSON *list_to_check, const cJSON_bool case_sensitive_param);
static cJSON *find_and_detach_second_half(cJSON *list_param);
static cJSON *merge_sorted_sublists(cJSON *first_list, cJSON *second_list, const cJSON_bool case_sensitive_param);
static cJSON *sort_list(cJSON *list, const cJSON_bool case_sensitive);
static unsigned char *build_suffixed_path(const unsigned char *base_path, const unsigned char *suffix_to_encode);
static void compose_patch(cJSON * const patches, const unsigned char * const operation, const unsigned char * const path, const unsigned char *suffix, const cJSON * const value);
static cJSON_bool generate_replace_patch_if_types_mismatch(cJSON *patches_array, const unsigned char *current_path, const cJSON *from_node, const cJSON *to_node);
static void generate_number_diff_patch(cJSON *patches_array, const unsigned char *current_path, const cJSON *from_num, const cJSON *to_num);
static void generate_string_diff_patch(cJSON *patches_array, const unsigned char *current_path, const cJSON *from_str, const cJSON *to_str);
static void generate_array_diff_patches(cJSON *patches_array, const unsigned char *base_path, cJSON *from_array, cJSON *to_array, const cJSON_bool case_sensitive_param);
static void process_object_common_key_patch(cJSON *patches_array, const unsigned char *base_path, cJSON *from_child_node, cJSON *to_child_node, const cJSON_bool case_sensitive_param);
static void process_object_removal_patch(cJSON *patches_array, const unsigned char *base_path, cJSON *from_child_node_to_remove);
static void process_object_addition_patch(cJSON *patches_array, const unsigned char *base_path, cJSON *to_child_node_to_add);
static void generate_object_diff_patches(cJSON *patches_array, const unsigned char *base_path, cJSON *from_object, cJSON *to_object, const cJSON_bool case_sensitive_param);


/* string comparison which doesn't consider NULL pointers equal */
static int compare_strings(const unsigned char *string1, const unsigned char *string2, const cJSON_bool case_sensitive)
{
    if ((string1 == NULL) || (string2 == NULL))
    {
        return 1; /* Consider them different if one is NULL */
    }

    if (string1 == string2)
    {
        return 0; /* Identical pointers mean identical strings */
    }

    if (case_sensitive)
    {
        return strcmp((const char*)string1, (const char*)string2);
    }

    for(; tolower(*string1) == tolower(*string2); (void)string1++, string2++)
    {
        if (*string1 == '\0') /* End of both strings, and all chars matched */
        {
            return 0;
        }
    }
    /* Difference found or end of one string reached */
    return tolower(*string1) - tolower(*string2);
}

/* copy a string while escaping '~' and '/' with ~0 and ~1 JSON pointer escape codes */
static void encode_string_as_pointer(unsigned char *destination, const unsigned char *source)
{
    if (destination == NULL || source == NULL) return;

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

/* Calculates the length of a string after JSON pointer encoding ('~' -> '~0', '/' -> '~1') */
static size_t calculate_encoded_string_length(const unsigned char *input_string)
{
    size_t length = 0;
    const unsigned char *temp_string = input_string;

    if (temp_string == NULL) {
        return 0;
    }

    for (length = 0; *temp_string != '\0'; (void)temp_string++, length++)
    {
        if ((*temp_string == '~') || (*temp_string == '/'))
        {
            length++; /* Additional character for escape sequence */
        }
    }
    return length;
}

/* Creates an empty JSON pointer string (""). Caller must free. */
static char *create_empty_path_string(void)
{
    unsigned char *copy = NULL;
    size_t length = 0;

    length = sizeof(""); /* Just space for the null terminator */
    copy = (unsigned char*)cJSON_malloc(length);
    if (copy == NULL)
    {
        return NULL;
    }
    copy[0] = '\0'; /* Create an empty string */
    return (char*)copy;
}

/* Builds a JSON pointer segment for an array item and prepends it to target_pointer_to_free.
 * Frees target_pointer_to_free. Caller must free the returned string. */
static char *build_array_item_pointer_and_free_target(unsigned char *target_pointer_to_free, size_t child_index_param)
{
    unsigned char *full_pointer_local = NULL;
    char index_buffer[22]; /* Room for 64-bit unsigned int as string (approx 20 digits) + '/' + '\0' */
    size_t target_len = 0;
    size_t index_str_len = 0;

    if (target_pointer_to_free == NULL) return NULL; /* Should ideally not happen if called correctly */

    /* check if conversion to unsigned long is valid (original check) */
    if (child_index_param > ULONG_MAX)
    {
        cJSON_free(target_pointer_to_free);
        return NULL;
    }

    sprintf(index_buffer, "/%lu", (unsigned long)child_index_param);
    index_str_len = strlen(index_buffer);
    target_len = strlen((char*)target_pointer_to_free);

    full_pointer_local = (unsigned char*)cJSON_malloc(index_str_len + target_len + 1); /* +1 for null terminator */
    if (full_pointer_local == NULL)
    {
        cJSON_free(target_pointer_to_free);
        return NULL;
    }

    memcpy(full_pointer_local, index_buffer, index_str_len);
    memcpy(full_pointer_local + index_str_len, target_pointer_to_free, target_len + 1); /* +1 to copy null terminator */

    cJSON_free(target_pointer_to_free);
    return (char*)full_pointer_local;
}

/* Builds a JSON pointer segment for an object property and prepends it to target_pointer_to_free.
 * Frees target_pointer_to_free. Caller must free the returned string. */
static char *build_object_property_pointer_and_free_target(unsigned char *target_pointer_to_free, const char *child_key)
{
    unsigned char *full_pointer_local = NULL;
    size_t encoded_key_len = 0;
    size_t target_len = 0;

    if (target_pointer_to_free == NULL || child_key == NULL) {
         if(target_pointer_to_free) cJSON_free(target_pointer_to_free);
         return NULL;
    }

    encoded_key_len = calculate_encoded_string_length((const unsigned char *)child_key);
    target_len = strlen((char*)target_pointer_to_free);

    /* '/' + encoded_key + target_path + '\0' */
    full_pointer_local = (unsigned char*)cJSON_malloc(1 + encoded_key_len + target_len + 1);
    if (full_pointer_local == NULL)
    {
        cJSON_free(target_pointer_to_free);
        return NULL;
    }

    full_pointer_local[0] = '/';
    encode_string_as_pointer(full_pointer_local + 1, (const unsigned char *)child_key);
    /* encode_string_as_pointer writes a null terminator. We'll overwrite it with strcat. */
    /* Or, more robustly, copy directly: */
    memcpy(full_pointer_local + 1 + encoded_key_len, target_pointer_to_free, target_len + 1);

    cJSON_free(target_pointer_to_free);
    return (char*)full_pointer_local;
}


CJSON_PUBLIC(char *) cJSONUtils_FindPointerFromObjectTo(const cJSON * const object, const cJSON * const target)
{
    unsigned char *target_pointer = NULL; /* Store result from recursive call */
    cJSON *current_child = NULL;
    size_t child_index = 0; /* Used for array indexing */
    /* Variables like copy, string, length, pointer_encoded_length, full_pointer from original are now in helpers or not needed here */

    if ((object == NULL) || (target == NULL))
    {
        return NULL;
    }

    if (object == target)
    {
        return create_empty_path_string();
    }

    /* Recursively search all children of the object or array */
    /* child_index is only used if object is an array, but incremented for all children for consistency if needed elsewhere (not here) */
    for (current_child = object->child; current_child != NULL; (void)(current_child = current_child->next), child_index++)
    {
        target_pointer = (unsigned char*)cJSONUtils_FindPointerFromObjectTo(current_child, target);
        /* Found the target? */
        if (target_pointer != NULL)
        {
            if (cJSON_IsArray(object))
            {
                return build_array_item_pointer_and_free_target(target_pointer, child_index);
            }

            /* This 'if' should ideally be 'else if' if types were mutually exclusive and order mattered,
               but original was two separate 'if's. If an item could be both (not standard cJSON),
               the first one (array) would take precedence. Standard cJSON items are one type.
            */
            if (cJSON_IsObject(object))
            {
                 /* current_child->string is the key for this child in the parent object */
                return build_object_property_pointer_and_free_target(target_pointer, current_child->string);
            }

            /*
             * If target_pointer is not NULL, it means the target was found under current_child.
             * If 'object' is neither an Array nor an Object, it cannot be a parent in a JSON path structure in a standard way.
             * This path implies 'object' has children (loop is running) but is not Array/Object.
             * This case should ideally not be reached with standard cJSON structures.
             * Free the found sub-pointer and indicate path cannot be completed through this 'object'.
             */
            cJSON_free(target_pointer);
            return NULL;
        }
    }

    /* Not found */
    return NULL;
}

/* Checks if a list is already sorted or has 0 or 1 elements. */
static cJSON_bool is_list_sorted_or_trivial(cJSON *list_to_check, const cJSON_bool case_sensitive_param)
{
    cJSON *current = NULL;

    if ((list_to_check == NULL) || (list_to_check->next == NULL))
    {
        return true; /* Zero or one entry is sorted. */
    }

    current = list_to_check;
    while ((current->next != NULL) && /* current must have a next to compare */
           (compare_strings((unsigned char*)current->string, (unsigned char*)current->next->string, case_sensitive_param) <= 0)) /* Allow equal for sorted */
    {
        current = current->next;
    }
    /* If loop finished because current->next is NULL, the list is sorted. */
    return (current->next == NULL);
}

/* Finds the middle of the list and detaches the second half.
 * Modifies the first list (list_param) by terminating it.
 * Returns a pointer to the head of the new (second) list.
 */
static cJSON *find_and_detach_second_half(cJSON *list_param)
{
    cJSON *head_of_second_list = list_param; /* Corresponds to 'second' in original logic */
    cJSON *fast_ptr = list_param;           /* Corresponds to 'current_item' in original logic */

    /* If list is NULL or has only one element, no second half to detach.
       This should be caught by is_list_sorted_or_trivial before calling this.
       Adding a safeguard here.
    */
    if (list_param == NULL || list_param->next == NULL) {
        return NULL;
    }

    /* Advance pointers to find the split point.
       'head_of_second_list' moves one step for (roughly) every two steps of 'fast_ptr'.
    */
    while (fast_ptr != NULL)
    {
        head_of_second_list = head_of_second_list->next;
        fast_ptr = fast_ptr->next;
        if (fast_ptr != NULL) /* Advance fast_ptr a second time if possible */
        {
            fast_ptr = fast_ptr->next;
        }
    }

    /* Now, head_of_second_list points to the start of the second list.
       The first list (list_param) must be terminated at head_of_second_list->prev.
    */
    if ((head_of_second_list != NULL) && (head_of_second_list->prev != NULL))
    {
        head_of_second_list->prev->next = NULL; /* Terminate the first list */
        head_of_second_list->prev = NULL;       /* Second list's prev pointer is NULL */
    }
    /* Else: head_of_second_list might be NULL (e.g. if original list was very short and this logic leads to it)
             or head_of_second_list->prev is NULL (head_of_second_list is the original head, meaning first list is empty - should not happen here).
       This function assumes list_param has at least two elements.
    */
    return head_of_second_list;
}


/* Merges two sorted cJSON lists. */
static cJSON *merge_sorted_sublists(cJSON *first_list, cJSON *second_list, const cJSON_bool case_sensitive_param)
{
    cJSON *merged_result_head = NULL;
    cJSON *merged_result_tail = NULL;
    cJSON *smaller_node = NULL;

    /* Handle cases where one or both lists are empty */
    if (first_list == NULL) return second_list;
    if (second_list == NULL) return first_list;

    /* Merge the lists by picking the smaller element */
    while ((first_list != NULL) && (second_list != NULL))
    {
        if (compare_strings((unsigned char*)first_list->string, (unsigned char*)second_list->string, case_sensitive_param) < 0)
        {
            smaller_node = first_list;
            first_list = first_list->next;
        }
        else
        {
            smaller_node = second_list;
            second_list = second_list->next;
        }

        if (merged_result_head == NULL)
        {
            merged_result_head = smaller_node;
            merged_result_tail = smaller_node;
            smaller_node->prev = NULL; /* First element's prev is NULL */
        }
        else
        {
            merged_result_tail->next = smaller_node;
            smaller_node->prev = merged_result_tail;
            merged_result_tail = smaller_node;
        }
    }

    /* Append the rest of the non-empty list */
    if (first_list != NULL)
    {
        merged_result_tail->next = first_list;
        first_list->prev = merged_result_tail;
        /* Update tail to the end of the appended list - not strictly needed if only head is returned and list is traversed by next */
    }
    else if (second_list != NULL) /* second_list is the one with remaining elements */
    {
        merged_result_tail->next = second_list;
        second_list->prev = merged_result_tail;
    }

    /* Ensure the final merged list is properly terminated if it's not empty */
    if (merged_result_tail != NULL) {
        merged_result_tail->next = NULL;
    }


    return merged_result_head;
}

/* Sort lists using mergesort. Modifies the list in place. Returns the new head of the sorted list. */
static cJSON *sort_list(cJSON *list, const cJSON_bool case_sensitive)
{
    cJSON *first_half = list;
    cJSON *second_half = NULL;
    /* Other variables like smaller, current_item, result, result_tail moved to helper functions */

    if (is_list_sorted_or_trivial(list, case_sensitive))
    {
        return list; /* Already sorted or too short to sort */
    }

    /* Split the list into two halves */
    /* 'first_half' will point to the head of the (now shorter) first list. */
    /* 'second_half' will point to the head of the second list. */
    second_half = find_and_detach_second_half(first_half);

    /* Recursively sort the sub-lists */
    first_half = sort_list(first_half, case_sensitive);
    second_half = sort_list(second_half, case_sensitive);

    /* Merge the sorted sub-lists */
    return merge_sorted_sublists(first_half, second_half, case_sensitive);
}

/* Builds a new path string by appending an encoded suffix to a base path.
 * e.g., base_path + "/" + encoded_suffix. Caller must free.
 */
static unsigned char *build_suffixed_path(const unsigned char *base_path, const unsigned char *suffix_to_encode)
{
    unsigned char *new_full_path = NULL;
    size_t encoded_suffix_len = 0;
    size_t base_path_len = 0;

    if (base_path == NULL || suffix_to_encode == NULL) return NULL;

    encoded_suffix_len = calculate_encoded_string_length(suffix_to_encode);
    base_path_len = strlen((const char*)base_path);

    /* Memory for: base_path + '/' + encoded_suffix + '\0' */
    new_full_path = (unsigned char*)cJSON_malloc(base_path_len + 1 + encoded_suffix_len + 1);
    if (new_full_path == NULL)
    {
        return NULL;
    }

    memcpy(new_full_path, base_path, base_path_len);
    new_full_path[base_path_len] = '/';
    encode_string_as_pointer(new_full_path + base_path_len + 1, suffix_to_encode);
    /* encode_string_as_pointer adds its own null terminator. */

    return new_full_path;
}


static void compose_patch(cJSON * const patches, const unsigned char * const operation, const unsigned char * const path, const unsigned char *suffix, const cJSON * const value)
{
    cJSON *patch = NULL;
    unsigned char *full_path_with_suffix = NULL; /* Used if suffix is not NULL */
    /* Other variables (string, suffix_length, path_length) are now in build_suffixed_path or not needed */

    if ((patches == NULL) || (operation == NULL) || (path == NULL))
    {
        return;
    }

    patch = cJSON_CreateObject();
    if (patch == NULL)
    {
        return;
    }
    cJSON_AddItemToObject(patch, "op", cJSON_CreateString((const char*)operation));

    if (suffix == NULL)
    {
        cJSON_AddItemToObject(patch, "path", cJSON_CreateString((const char*)path));
    }
    else
    {
        full_path_with_suffix = build_suffixed_path(path, suffix);
        if (full_path_with_suffix == NULL) {
            cJSON_Delete(patch); /* Clean up allocated patch object */
            return; /* Allocation failed for full_path_with_suffix */
        }
        cJSON_AddItemToObject(patch, "path", cJSON_CreateString((const char*)full_path_with_suffix));
        cJSON_free(full_path_with_suffix); /* cJSON_CreateString makes a copy */
    }

    if (value != NULL)
    {
        cJSON_AddItemToObject(patch, "value", cJSON_Duplicate(value, 1));
    }
    cJSON_AddItemToArray(patches, patch);
}

/* Generates a "replace" patch if the types of from_node and to_node differ.
 * Returns true if a patch was generated, false otherwise.
 */
static cJSON_bool generate_replace_patch_if_types_mismatch(cJSON *patches_array, const unsigned char *current_path, const cJSON *from_node, const cJSON *to_node)
{
    if ((from_node->type & 0xFF) != (to_node->type & 0xFF))
    {
        compose_patch(patches_array, (const unsigned char*)"replace", current_path, NULL, to_node);
        return true; /* Mismatch handled */
    }
    return false; /* Types match */
}

/* Generates a "replace" patch for cJSON_Number if values differ significantly. */
static void generate_number_diff_patch(cJSON *patches_array, const unsigned char *current_path, const cJSON *from_num, const cJSON *to_num)
{
    double val_a = 0.0;
    double val_b = 0.0;
    double max_abs_val = 0.0;
    cJSON_bool doubles_are_close_enough = false;

    val_a = from_num->valuedouble;
    val_b = to_num->valuedouble;

    max_abs_val = fabs(val_a) > fabs(val_b) ? fabs(val_a) : fabs(val_b);
    /* DBL_EPSILON comparison for floating point numbers */
    doubles_are_close_enough = (fabs(val_a - val_b) <= max_abs_val * DBL_EPSILON);

    /* If integer parts differ OR floating point parts differ beyond epsilon */
    if ((from_num->valueint != to_num->valueint) || !doubles_are_close_enough)
    {
        compose_patch(patches_array, (const unsigned char*)"replace", current_path, NULL, to_num);
    }
}

/* Generates a "replace" patch for cJSON_String if values differ. */
static void generate_string_diff_patch(cJSON *patches_array, const unsigned char *current_path, const cJSON *from_str, const cJSON *to_str)
{
    if (strcmp(from_str->valuestring, to_str->valuestring) != 0)
    {
        compose_patch(patches_array, (const unsigned char*)"replace", current_path, NULL, to_str);
    }
}

/* Generates patches for differences between two cJSON_Array nodes. */
static void generate_array_diff_patches(cJSON *patches_array, const unsigned char *base_path, cJSON *from_array, cJSON *to_array, const cJSON_bool case_sensitive_param)
{
    size_t current_idx = 0;
    cJSON *current_from_child = NULL;
    cJSON *current_to_child = NULL;
    unsigned char *child_element_path = NULL; /* Path for array elements, e.g., /0, /1 */
    char index_as_string_suffix[22]; /* Buffer for index as string (suffix for remove) */

    if (from_array == NULL || to_array == NULL) return; /* Should be cJSON_Array type */

    current_from_child = from_array->child;
    current_to_child = to_array->child;

    /* Max length for child_element_path: strlen(base_path) + '/' + 20_digits_for_index + '\0' */
    child_element_path = (unsigned char*)cJSON_malloc(strlen((const char*)base_path) + 1 + 20 + 1);
    if (child_element_path == NULL) {
        return; /* Allocation failure */
    }

    /* Iterate through common elements and generate patches recursively */
    for (current_idx = 0; (current_from_child != NULL) && (current_to_child != NULL);
         (void)(current_from_child = current_from_child->next), (void)(current_to_child = current_to_child->next), current_idx++)
    {
        if (current_idx > ULONG_MAX) /* Original check */
        {
            cJSON_free(child_element_path);
            return;
        }
        sprintf((char*)child_element_path, "%s/%lu", base_path, (unsigned long)current_idx);
        create_patches(patches_array, child_element_path, current_from_child, current_to_child, case_sensitive_param);
    }

    /* Remove leftover elements from 'from_array' (elements not in 'to_array') */
    /* 'current_idx' is now the index at which removals should effectively happen.
       Original code implies removing at this fixed index repeatedly as array shrinks.
    */
    for (; (current_from_child != NULL); (void)(current_from_child = current_from_child->next))
    {
        if (current_idx > ULONG_MAX) /* Original check */
        {
            cJSON_free(child_element_path);
            return;
        }
        /* The suffix for "remove" is the index of the element to remove from base_path */
        sprintf(index_as_string_suffix, "%lu", (unsigned long)current_idx);
        compose_patch(patches_array, (const unsigned char*)"remove", base_path, (const unsigned char*)index_as_string_suffix, NULL);
        /* Note: 'current_idx' does NOT increment here, as per original logic of repeatedly removing at the same logical start of the tail. */
    }

    /* Add new elements from 'to_array' (elements not in 'from_array') */
    /* 'current_idx' continues from where common elements (or removals) left off, effectively for appends if using "-" */
    for (; (current_to_child != NULL); (void)(current_to_child = current_to_child->next), current_idx++) /* current_idx increments for add */
    {
        /* For "add" to an array, suffix "-" appends to the end of the array at 'base_path'. */
        compose_patch(patches_array, (const unsigned char*)"add", base_path, (const unsigned char*)"-", current_to_child);
    }
    cJSON_free(child_element_path);
}

/* Processes a common key in two objects, generating patches for its value. */
static void process_object_common_key_patch(cJSON *patches_array, const unsigned char *base_path, cJSON *from_child_node, cJSON *to_child_node, const cJSON_bool case_sensitive_param)
{
    unsigned char *child_property_path = NULL;
    size_t base_path_len = 0;
    size_t encoded_key_len = 0;
    const unsigned char *key_str = NULL;

    if (from_child_node == NULL || from_child_node->string == NULL) return;
    key_str = (unsigned char*)from_child_node->string;

    base_path_len = strlen((const char*)base_path);
    encoded_key_len = calculate_encoded_string_length(key_str);

    /* Path: base_path + "/" + encoded_key_str + '\0' */
    child_property_path = (unsigned char*)cJSON_malloc(base_path_len + 1 + encoded_key_len + 1);
    if (child_property_path == NULL) {
        return; /* Allocation failure */
    }

    /* Construct the full path to the child property */
    memcpy(child_property_path, base_path, base_path_len);
    child_property_path[base_path_len] = '/';
    encode_string_as_pointer(child_property_path + base_path_len + 1, key_str);

    /* Recursively create patches for the values of this common key */
    create_patches(patches_array, child_property_path, from_child_node, to_child_node, case_sensitive_param);
    cJSON_free(child_property_path);
}

/* Generates a "remove" patch for an object property. */
static void process_object_removal_patch(cJSON *patches_array, const unsigned char *base_path, cJSON *from_child_node_to_remove)
{
    if (from_child_node_to_remove == NULL || from_child_node_to_remove->string == NULL) return;

    /* For "remove" from an object, 'base_path' is the object's path,
     * and 'suffix' is the (pointer encoded) key of the property to remove.
     */
    compose_patch(patches_array, (const unsigned char*)"remove", base_path, (unsigned char*)from_child_node_to_remove->string, NULL);
}

/* Generates an "add" patch for an object property. */
static void process_object_addition_patch(cJSON *patches_array, const unsigned char *base_path, cJSON *to_child_node_to_add)
{
    if (to_child_node_to_add == NULL || to_child_node_to_add->string == NULL) return;
    /* For "add" to an object, 'base_path' is the object's path,
     * 'suffix' is the (pointer encoded) key, and 'value' is the node to add.
     */
    compose_patch(patches_array, (const unsigned char*)"add", base_path, (unsigned char*)to_child_node_to_add->string, to_child_node_to_add);
}


/* Generates patches for differences between two cJSON_Object nodes. */
static void generate_object_diff_patches(cJSON *patches_array, const unsigned char *base_path, cJSON *from_object, cJSON *to_object, const cJSON_bool case_sensitive_param)
{
    cJSON *current_from_child = NULL;
    cJSON *current_to_child = NULL;
    int key_comparison_result = 0;
    /* Variables for path construction (child_path_buffer, etc.) moved into process_object_common_key_patch */

    if (from_object == NULL || to_object == NULL) return; /* Should be cJSON_Object type */

    /* Sort children of both objects by key for comparison */
    /* Original code sorts from->child and to->child directly.
       This is fine as sort_list handles NULL input.
    */
    from_object->child = sort_list(from_object->child, case_sensitive_param);
    to_object->child = sort_list(to_object->child, case_sensitive_param);

    current_from_child = from_object->child;
    current_to_child = to_object->child;

    /* Iterate through sorted keys of both objects */
    while ((current_from_child != NULL) || (current_to_child != NULL))
    {
        if (current_from_child == NULL) {
            key_comparison_result = 1; /* 'from' exhausted, 'to' has more keys -> add */
        } else if (current_to_child == NULL) {
            key_comparison_result = -1; /* 'to' exhausted, 'from' has more keys -> remove */
        } else {
            key_comparison_result = compare_strings(
                (unsigned char*)current_from_child->string, /* Key from 'from_object' */
                (unsigned char*)current_to_child->string,   /* Key from 'to_object' */
                case_sensitive_param);
        }

        if (key_comparison_result == 0) {
            /* Keys are the same, compare their values recursively */
            process_object_common_key_patch(patches_array, base_path, current_from_child, current_to_child, case_sensitive_param);
            current_from_child = current_from_child->next;
            current_to_child = current_to_child->next;
        } else if (key_comparison_result < 0) {
            /* Key in 'from_object' only (comes before key in 'to_object' or 'to_object' exhausted for this key) -> remove */
            process_object_removal_patch(patches_array, base_path, current_from_child);
            current_from_child = current_from_child->next;
        } else { /* key_comparison_result > 0 */
            /* Key in 'to_object' only (comes before key in 'from_object' or 'from_object' exhausted for this key) -> add */
            process_object_addition_patch(patches_array, base_path, current_to_child);
            current_to_child = current_to_child->next;
        }
    }
}


/* Main function to create JSON Patches between two cJSON structures. */
CJSON_PUBLIC(void) create_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive)
{
    /* Most local variables (diff, a, b, new_path, etc.) have been moved to helper functions */
    /* from_child, to_child are also primarily used within helpers now. */

    if ((from == NULL) || (to == NULL) || (patches == NULL) || (path == NULL) )
    {
        return;
    }

    if (generate_replace_patch_if_types_mismatch(patches, path, from, to)) {
        return; /* Types differed, "replace" patch generated, no further comparison needed */
    }

    /* Types are the same, compare values based on type */
    switch (from->type & 0xFF) /* Mask out cJSON_IsReference and cJSON_StringIsConst */
    {
        case cJSON_Number:
            generate_number_diff_patch(patches, path, from, to);
            return; /* Processed number */

        case cJSON_String:
            generate_string_diff_patch(patches, path, from, to);
            return; /* Processed string */

        case cJSON_Array:
            generate_array_diff_patches(patches, path, from, to, case_sensitive);
            return; /* Processed array */

        case cJSON_Object:
            generate_object_diff_patches(patches, path, from, to, case_sensitive);
            return; /* Processed object */

        case cJSON_NULL:        /* Fallthrough */
        case cJSON_True:        /* Fallthrough */
        case cJSON_False:       /* Fallthrough */
        case cJSON_Raw:         /* Fallthrough */
        /* cJSON_Invalid / cJSON_IsReference / cJSON_StringIsConst are masked out or imply structural issues not patch diffs */
            /* For cJSON_NULL, True, False, Raw: if types are same (checked above), values are implicitly same. No patch needed. */
            /* If their 'value' fields could differ in a meaningful way not captured by type (e.g. Raw string content),
               a specific handler like generate_string_diff_patch would be needed for cJSON_Raw.
               Original code has no specific diff for these beyond type match.
            */
            break; /* No difference if types matched and no specific value to compare for these simple types */

        default:
            /* Unknown or unhandled type. Or cJSON_Invalid. */
            /* Original code had no explicit default handling here after type switch. */
            break;
    }
}

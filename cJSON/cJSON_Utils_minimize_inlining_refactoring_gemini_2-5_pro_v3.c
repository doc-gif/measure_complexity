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
/* math.h is already included above, no need for a second one */

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

/* Forward declarations for static functions */
static int compare_strings(const unsigned char *string1, const unsigned char *string2, const cJSON_bool case_sensitive);
static size_t calculate_json_pointer_encoded_length(const unsigned char *source_string);
static void encode_string_as_pointer(unsigned char *destination, const unsigned char *source);
static char* create_pointer_for_array_element(unsigned char *target_pointer_str, size_t child_index);
static char* create_pointer_for_object_element(unsigned char *target_pointer_str, const cJSON *current_child);
static cJSON* merge_sorted_lists(cJSON *first, cJSON *second, const cJSON_bool case_sensitive);
static unsigned char* build_patch_path_with_suffix(const unsigned char *base_path, const unsigned char *suffix_key);
static void generate_patches_for_number(cJSON *patches, const unsigned char *path, const cJSON *from, const cJSON *to);
static void generate_patches_for_string(cJSON *patches, const unsigned char *path, const cJSON *from, const cJSON *to);
static void generate_patches_for_array(cJSON *patches, const unsigned char *path, cJSON *from_array, cJSON *to_array, const cJSON_bool case_sensitive);
static void generate_patches_for_object(cJSON *patches, const unsigned char *path, cJSON *from_object, cJSON *to_object, const cJSON_bool case_sensitive);
static void compose_patch(cJSON * const patches, const unsigned char * const operation, const unsigned char * const path, const unsigned char *suffix, const cJSON * const value);
static cJSON *sort_list(cJSON *list, const cJSON_bool case_sensitive);
void create_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive);


/* string comparison which doesn't consider NULL pointers equal */
static int compare_strings(const unsigned char *string1, const unsigned char *string2, const cJSON_bool case_sensitive)
{
    const unsigned char *p1 = string1;
    const unsigned char *p2 = string2;

    if ((p1 == NULL) || (p2 == NULL))
    {
        return 1; /* Ensure NULLs are not considered equal to anything, or each other */
    }

    if (p1 == p2)
    {
        return 0;
    }

    if (case_sensitive)
    {
        return strcmp((const char*)p1, (const char*)p2);
    }

    for(; tolower(*p1) == tolower(*p2); (void)p1++, p2++)
    {
        if (*p1 == '\0')
        {
            return 0;
        }
    }

    return tolower(*p1) - tolower(*p2);
}

/* Calculate the length of a string after encoding '~' and '/' for JSON pointer */
static size_t calculate_json_pointer_encoded_length(const unsigned char *source_string)
{
    size_t length = 0;
    const unsigned char *current = source_string;

    if (source_string == NULL) {
        return 0;
    }

    for (length = 0; *current != '\0'; (void)current++, length++)
    {
        /* character needs to be escaped? */
        if ((*current == '~') || (*current == '/'))
        {
            length++;
        }
    }
    return length;
}


/* copy a string while escaping '~' and '/' with ~0 and ~1 JSON pointer escape codes */
static void encode_string_as_pointer(unsigned char *destination, const unsigned char *source)
{
    unsigned char *dest_ptr = destination;
    const unsigned char *src_ptr = source;

    for (; src_ptr[0] != '\0'; (void)src_ptr++, dest_ptr++)
    {
        if (src_ptr[0] == '/')
        {
            dest_ptr[0] = '~';
            dest_ptr[1] = '1';
            dest_ptr++;
        }
        else if (src_ptr[0] == '~')
        {
            dest_ptr[0] = '~';
            dest_ptr[1] = '0';
            dest_ptr++;
        }
        else
        {
            dest_ptr[0] = src_ptr[0];
        }
    }

    dest_ptr[0] = '\0';
}

/* Helper to create a JSON pointer string for an array element */
static char* create_pointer_for_array_element(unsigned char *target_pointer_str, size_t child_index)
{
    unsigned char *full_pointer;

    /* reserve enough memory for a 64 bit integer + '/' and '\0' */
    /* Max ULONG_MAX for %lu is 20 chars for 64-bit, + '/' + existing_len + '\0' */
    full_pointer = (unsigned char*)cJSON_malloc(strlen((char*)target_pointer_str) + 20 + sizeof("/"));
    if (full_pointer == NULL)
    {
        return NULL;
    }

    /* check if conversion to unsigned long is valid */
    if (child_index > ULONG_MAX)
    {
        cJSON_free(full_pointer);
        return NULL;
    }
    sprintf((char*)full_pointer, "/%lu%s", (unsigned long)child_index, target_pointer_str); /* /<array_index><path> */

    return (char*)full_pointer;
}

/* Helper to create a JSON pointer string for an object element */
static char* create_pointer_for_object_element(unsigned char *target_pointer_str, const cJSON *current_child)
{
    unsigned char *full_pointer;
    size_t pointer_encoded_length;

    pointer_encoded_length = calculate_json_pointer_encoded_length((unsigned char*)current_child->string);

    full_pointer = (unsigned char*)cJSON_malloc(strlen((char*)target_pointer_str) + pointer_encoded_length + 2); /* +1 for '/', +1 for '\0' */
    if (full_pointer == NULL)
    {
        return NULL;
    }
    full_pointer[0] = '/';
    encode_string_as_pointer(full_pointer + 1, (unsigned char*)current_child->string);
    strcat((char*)full_pointer, (char*)target_pointer_str);

    return (char*)full_pointer;
}


CJSON_PUBLIC(char *) cJSONUtils_FindPointerFromObjectTo(const cJSON * const object, const cJSON * const target)
{
    unsigned char *copy_of_empty_string;
    unsigned char *target_pointer;
    unsigned char *full_pointer;
    size_t child_index;
    cJSON *current_child;

    /* Initialize variables */
    copy_of_empty_string = NULL;
    target_pointer = NULL;
    full_pointer = NULL;
    child_index = 0;
    current_child = NULL;


    if ((object == NULL) || (target == NULL))
    {
        return NULL;
    }

    if (object == target)
    {
        /* found: return an empty string for the pointer */
        /* Allocate memory for an empty string "" which is 1 byte for '\0' */
        copy_of_empty_string = (unsigned char*) cJSON_malloc(sizeof(""));
        if (copy_of_empty_string == NULL)
        {
            return NULL;
        }
        memcpy(copy_of_empty_string, "", sizeof(""));
        return (char*)copy_of_empty_string;
    }

    /* recursively search all children of the object or array */
    current_child = object->child;
    while(current_child != NULL)
    {
        target_pointer = (unsigned char*)cJSONUtils_FindPointerFromObjectTo(current_child, target);
        /* found the target? */
        if (target_pointer != NULL)
        {
            if (cJSON_IsArray(object))
            {
                full_pointer = (unsigned char*)create_pointer_for_array_element(target_pointer, child_index);
                cJSON_free(target_pointer); /* Original target_pointer is consumed or freed */
                if (full_pointer == NULL)
                {
                    /* Error in create_pointer_for_array_element, potentially ULONG_MAX exceeded or malloc failed */
                    return NULL;
                }
                return (char*)full_pointer;
            }

            if (cJSON_IsObject(object))
            {
                full_pointer = (unsigned char*)create_pointer_for_object_element(target_pointer, current_child);
                cJSON_free(target_pointer); /* Original target_pointer is consumed or freed */
                 if (full_pointer == NULL)
                {
                    /* Error in create_pointer_for_object_element, malloc failed */
                    return NULL;
                }
                return (char*)full_pointer;
            }

            /* Should not be reached if object is Array or Object, implies structure error or leaf found unexpectedly */
            cJSON_free(target_pointer);
            return NULL; /* Safety: if not array/object, but has children and found target_pointer implies error */
        }
        current_child = current_child->next;
        child_index++;
    }

    /* not found */
    return NULL;
}

/* Helper function to merge two sorted cJSON lists */
static cJSON* merge_sorted_lists(cJSON *first, cJSON *second, const cJSON_bool case_sensitive)
{
    cJSON *result_head;
    cJSON *result_tail;
    cJSON *smaller_item;

    result_head = NULL;
    result_tail = NULL;
    smaller_item = NULL;

    /* Merge the sub-lists */
    while ((first != NULL) && (second != NULL))
    {
        if (compare_strings((unsigned char*)first->string, (unsigned char*)second->string, case_sensitive) < 0)
        {
            smaller_item = first;
            first = first->next;
        }
        else
        {
            smaller_item = second;
            second = second->next;
        }

        if (result_head == NULL)
        {
            /* start merged list with the smaller element */
            result_head = smaller_item;
            result_tail = smaller_item;
            smaller_item->prev = NULL; /* Ensure the new head has no prev */
        }
        else
        {
            /* add smaller element to the list */
            result_tail->next = smaller_item;
            smaller_item->prev = result_tail;
            result_tail = smaller_item;
        }
    }

    /* Append rest of the remaining list (first or second) */
    if (first != NULL)
    {
        if (result_head == NULL)
        {
            return first;
        }
        result_tail->next = first;
        first->prev = result_tail;
    }
    else if (second != NULL) /* Use else if to be explicit that second might be NULL if first was also NULL */
    {
        if (result_head == NULL)
        {
            return second;
        }
        result_tail->next = second;
        second->prev = result_tail;
    }

    if (result_tail != NULL) { /* Ensure tail's next is NULL if it's the end */
        result_tail->next = NULL;
    }


    return result_head;
}


/* sort lists using mergesort */
static cJSON *sort_list(cJSON *list, const cJSON_bool case_sensitive)
{
    cJSON *first_half;
    cJSON *second_half;
    cJSON *slow_ptr;
    cJSON *fast_ptr;
    cJSON *sorted_list;

    first_half = list;
    slow_ptr = list; /* 'second' in original, renamed for clarity in split */
    fast_ptr = list; /* 'current_item' in original for finding middle */
    sorted_list = list; /* 'result' in original */


    if ((list == NULL) || (list->next == NULL))
    {
        /* One entry is sorted already. */
        return sorted_list;
    }

    /* Test for already sorted list. */
    /* fast_ptr is used as current_item here to check sortedness */
    fast_ptr = list;
    while ((fast_ptr->next != NULL) && (compare_strings((unsigned char*)fast_ptr->string, (unsigned char*)fast_ptr->next->string, case_sensitive) <= 0))
    {
        fast_ptr = fast_ptr->next;
    }
    if (fast_ptr->next == NULL) /* Reached end, list is sorted */
    {
        return sorted_list;
    }


    /* Split the list into two halves */
    slow_ptr = list;      /* Pointer for the first element of the second half */
    fast_ptr = list;      /* Pointer to traverse the list, two steps at a time */

    while (fast_ptr != NULL && fast_ptr->next != NULL)
    {
        fast_ptr = fast_ptr->next->next;
        if (fast_ptr != NULL) /* Only advance slow_ptr if fast_ptr could advance twice (or once to non-NULL) */
        {
             if (slow_ptr->next == NULL) /* Should not happen in a list of size >=2 if fast_ptr advanced */
                 break;
            slow_ptr = slow_ptr->next;
        }
    }

    /* slow_ptr should now be roughly at the middle.
       The node before slow_ptr will be the end of the first list.
       slow_ptr will be the start of the second list.
    */
    second_half = slow_ptr->next; /* slow_ptr is the last element of first_half or middle element if odd length */
    slow_ptr->next = NULL; /* Terminate the first list */

    if (second_half != NULL) { /* If there is a second half */
        second_half->prev = NULL; /* Second list starts here, so no prev */
    }


    /* Recursively sort the sub-lists. */
    first_half = sort_list(first_half, case_sensitive);
    second_half = sort_list(second_half, case_sensitive);

    /* Merge the sorted sub-lists. */
    return merge_sorted_lists(first_half, second_half, case_sensitive);
}

/* Helper to build a JSON patch path string with an encoded suffix key */
static unsigned char* build_patch_path_with_suffix(const unsigned char *base_path, const unsigned char *suffix_key)
{
    unsigned char *full_path;
    size_t encoded_suffix_length;
    size_t base_path_length;

    encoded_suffix_length = 0; /* Initialize */
    base_path_length = 0;      /* Initialize */


    if (suffix_key == NULL) /* Should not happen if this function is called for suffixes */
    {
        return NULL;
    }

    encoded_suffix_length = calculate_json_pointer_encoded_length(suffix_key);
    base_path_length = strlen((const char*)base_path);

    /* Allocate memory: base_path + '/' + encoded_suffix + '\0' */
    full_path = (unsigned char*)cJSON_malloc(base_path_length + encoded_suffix_length + sizeof("/")); /* sizeof("/") is 2 for '/' and '\0' */
    if (full_path == NULL)
    {
        return NULL;
    }

    sprintf((char*)full_path, "%s/", (const char*)base_path);
    encode_string_as_pointer(full_path + base_path_length + 1, suffix_key);

    return full_path;
}


static void compose_patch(cJSON * const patches, const unsigned char * const operation, const unsigned char * const path, const unsigned char *suffix, const cJSON * const value)
{
    unsigned char *full_path_str;
    cJSON *patch;

    full_path_str = NULL;
    patch = NULL;

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
        full_path_str = build_patch_path_with_suffix(path, suffix);
        if (full_path_str == NULL)
        {
            cJSON_Delete(patch);
            return;
        }
        cJSON_AddItemToObject(patch, "path", cJSON_CreateString((const char*)full_path_str));
        cJSON_free(full_path_str);
    }

    if (value != NULL)
    {
        cJSON_AddItemToObject(patch, "value", cJSON_Duplicate(value, 1));
    }
    cJSON_AddItemToArray(patches, patch);
}

/* Generates a "replace" patch if numeric values differ */
static void generate_patches_for_number(cJSON *patches, const unsigned char *path, const cJSON *from, const cJSON *to)
{
    double a;
    double b;
    double max_val;
    cJSON_bool doubles_are_close;

    a = from->valuedouble;
    b = to->valuedouble;

    max_val = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    /* Compare doubles with a relative epsilon */
    doubles_are_close = (fabs(a - b) <= max_val * DBL_EPSILON);

    /* If integer parts differ or floating point parts (if relevant) differ significantly */
    if ((from->valueint != to->valueint) || !doubles_are_close)
    {
        compose_patch(patches, (const unsigned char*)"replace", path, NULL, to);
    }
}

/* Generates a "replace" patch if string values differ */
static void generate_patches_for_string(cJSON *patches, const unsigned char *path, const cJSON *from, const cJSON *to)
{
    if (strcmp(from->valuestring, to->valuestring) != 0)
    {
        compose_patch(patches, (const unsigned char*)"replace", path, NULL, to);
    }
}

/* Generates patches for differences between two arrays */
static void generate_patches_for_array(cJSON *patches, const unsigned char *path, cJSON *from_array, cJSON *to_array, const cJSON_bool case_sensitive)
{
    size_t index;
    cJSON *from_child;
    cJSON *to_child;
    unsigned char *new_path_buffer; /* Buffer for constructing sub-paths */

    index = 0;
    from_child = from_array->child;
    to_child = to_array->child;
    /* Allow space for 64bit int (log10(2^64) approx 20) + path len + '/' + '\0' */
    new_path_buffer = (unsigned char*)cJSON_malloc(strlen((const char*)path) + 20 + sizeof("/"));
    if (new_path_buffer == NULL)
    {
        return; /* Cannot proceed without memory for paths */
    }


    /* Generate patches for all array elements that exist in both "from" and "to" */
    while((from_child != NULL) && (to_child != NULL))
    {
        if (index > ULONG_MAX) /* Check before sprintf */
        {
            cJSON_free(new_path_buffer);
            return;
        }
        sprintf((char*)new_path_buffer, "%s/%lu", path, (unsigned long)index);
        create_patches(patches, new_path_buffer, from_child, to_child, case_sensitive);

        from_child = from_child->next;
        to_child = to_child->next;
        index++;
    }

    /* Remove leftover elements from 'from' that are not in 'to' */
    /* The 'index' here correctly refers to the position for removal in the original array context */
    while(from_child != NULL)
    {
        if (index > ULONG_MAX) /* Check before sprintf for the suffix */
        {
            cJSON_free(new_path_buffer);
            return;
        }
        /* Suffix for remove is just the index */
        sprintf((char*)new_path_buffer, "%lu", (unsigned long)index);
        compose_patch(patches, (const unsigned char*)"remove", path, new_path_buffer, NULL);
        /* Do not increment index here, as 'remove' shifts subsequent elements */
        from_child = from_child->next;
    }

    /* Add new elements in 'to' that were not in 'from' */
    /* Suffix for add to end of array is "-" */
    while(to_child != NULL)
    {
        compose_patch(patches, (const unsigned char*)"add", path, (const unsigned char*)"-", to_child);
        to_child = to_child->next;
        /* Index 'index' could be used if adding by index, but "-" is for append, so index not strictly needed for compose_patch here */
        /* index++; // if we were tracking for a specific position, but "-" handles it */
    }

    cJSON_free(new_path_buffer);
}

/* Generates patches for differences between two objects */
static void generate_patches_for_object(cJSON *patches, const unsigned char *path, cJSON *from_object, cJSON *to_object, const cJSON_bool case_sensitive)
{
    int diff_result;
    unsigned char *current_child_path;
    size_t path_len;
    size_t child_key_encoded_len;
    cJSON *from_child_item;
    cJSON *to_child_item;

    current_child_path = NULL;
    path_len = 0;
    child_key_encoded_len = 0;


    /* Sort children by key for consistent comparison */
    if (from_object != NULL) { /* from_object->child could be NULL */
        from_object->child = sort_list(from_object->child, case_sensitive);
    }
    if (to_object != NULL) { /* to_object->child could be NULL */
        to_object->child = sort_list(to_object->child, case_sensitive);
    }

    from_child_item = (from_object != NULL) ? from_object->child : NULL;
    to_child_item = (to_object != NULL) ? to_object->child : NULL;

    while ((from_child_item != NULL) || (to_child_item != NULL))
    {
        if (from_child_item == NULL)
        {
            diff_result = 1; /* 'from' is exhausted, treat as 'to_child_item' key is greater */
        }
        else if (to_child_item == NULL)
        {
            diff_result = -1; /* 'to' is exhausted, treat as 'from_child_item' key is smaller */
        }
        else
        {
            diff_result = compare_strings((unsigned char*)from_child_item->string, (unsigned char*)to_child_item->string, case_sensitive);
        }

        if (diff_result == 0) /* Keys are the same, compare values */
        {
            path_len = strlen((const char*)path);
            child_key_encoded_len = calculate_json_pointer_encoded_length((unsigned char*)from_child_item->string);

            current_child_path = (unsigned char*)cJSON_malloc(path_len + child_key_encoded_len + sizeof("/")); /* for '/', and '\0' */
            if (current_child_path == NULL) return; /* Malloc failed */

            sprintf((char*)current_child_path, "%s/", path);
            encode_string_as_pointer(current_child_path + path_len + 1, (unsigned char*)from_child_item->string);

            create_patches(patches, current_child_path, from_child_item, to_child_item, case_sensitive);
            cJSON_free(current_child_path);
            current_child_path = NULL; /* Avoid double free if loop breaks early */

            from_child_item = from_child_item->next;
            to_child_item = to_child_item->next;
        }
        else if (diff_result < 0) /* Key in 'from' is not in 'to' (or 'from' key is smaller) -> remove it */
        {
            compose_patch(patches, (const unsigned char*)"remove", path, (unsigned char*)from_child_item->string, NULL);
            from_child_item = from_child_item->next;
        }
        else /* Key in 'to' is not in 'from' (or 'to' key is smaller) -> add it */
        {
            compose_patch(patches, (const unsigned char*)"add", path, (unsigned char*)to_child_item->string, to_child_item);
            to_child_item = to_child_item->next;
        }
    }
}


void create_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive)
{
    /* Ensure `from` and `to` are not NULL before dereferencing `type` */
    if ((from == NULL) || (to == NULL))
    {
        /* If one is NULL and not the other, it implies an add or remove at this path,
           but create_patches is for diffing two existing items.
           If both are NULL, no operation.
           If types are different, it's a replace.
           This function expects 'from' and 'to' to be valid items for comparison.
           A "replace" patch if one is NULL and other not should be handled by caller or by type difference.
           However, if path itself represents a missing item, a "replace" is generated.
        */
        if (from != to) { /* If one is NULL and the other isn't, or they are different valid items */
             compose_patch(patches, (const unsigned char*)"replace", path, NULL, to);
        }
        return;
    }

    if ((from->type & 0xFF) != (to->type & 0xFF))
    {
        /* Different types mean a "replace" operation */
        compose_patch(patches, (const unsigned char*)"replace", path, NULL, to);
        return;
    }

    switch (from->type & 0xFF)
    {
        case cJSON_NULL: /* cJSON_NULL, cJSON_True, cJSON_False, cJSON_Raw */
        case cJSON_True:
        case cJSON_False:
        case cJSON_Raw: /* For these types, if types are same, they are considered equal. No patch needed unless value changed (not applicable for Null/True/False, Raw string compared as string) */
            if (cJSON_IsRaw(from)) { /* Raw types are compared as strings */
                 if (from->valuestring != NULL && to->valuestring != NULL) { /* Both must be valid strings */
                    if (strcmp(from->valuestring, to->valuestring) != 0) {
                        compose_patch(patches, (const unsigned char*)"replace", path, NULL, to);
                    }
                 } else if (from->valuestring != to->valuestring) { /* One is NULL, other is not */
                    compose_patch(patches, (const unsigned char*)"replace", path, NULL, to);
                 }
            }
            /* For cJSON_NULL, cJSON_True, cJSON_False, if types match, values are inherently same. */
            break;

        case cJSON_Number:
            generate_patches_for_number(patches, path, from, to);
            break;

        case cJSON_String:
            generate_patches_for_string(patches, path, from, to);
            break;

        case cJSON_Array:
            generate_patches_for_array(patches, path, from, to, case_sensitive);
            break;

        case cJSON_Object:
            generate_patches_for_object(patches, path, from, to, case_sensitive);
            break;

        default:
            /* Unknown type or cJSON_Invalid, should not happen with valid cJSON objects */
            break;
    }
}
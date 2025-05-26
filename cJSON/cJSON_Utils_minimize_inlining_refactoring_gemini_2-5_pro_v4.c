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
#include <math.h> /* Redundant include, already above */

#if defined(_MSC_VER)
#pragma warning (pop)
#endif
#ifdef __GNUCC__
#pragma GCC visibility pop
#endif

#include "cJSON_Utils.h" /* Assuming cJSON.h is included via this or defines cJSON types */
#ifndef CJSON_PUBLIC /* Might be defined in cJSON.h or cJSON_Utils.h */
#define CJSON_PUBLIC
#endif
#ifndef cJSON_bool /* Might be defined in cJSON.h */
typedef int cJSON_bool;
#endif
/* Forward declarations for cJSON types if not fully included by cJSON_Utils.h */
/* These might be needed if cJSON_Utils.h doesn't pull in cJSON.h definitions */
/*
typedef struct cJSON cJSON;
struct cJSON {
    struct cJSON *next,*prev;
    struct cJSON *child;
    int type;
    char *valuestring;
    long long valueint; // Changed from int to long long to match usage with DBL_EPSILON comparison for larger integers
    double valuedouble;
    char *string;
};

// cJSON types (replace with actual values from cJSON.h)
#define cJSON_Invalid (0)
#define cJSON_False  (1 << 0)
#define cJSON_True   (1 << 1)
#define cJSON_NULL   (1 << 2)
#define cJSON_Number (1 << 3)
#define cJSON_String (1 << 4)
#define cJSON_Array  (1 << 5)
#define cJSON_Object (1 << 6)
#define cJSON_Raw    (1 << 7) // Not used in this file but part of cJSON types

// cJSON_ASHooks struct and cJSON_Hooks might be needed if cJSON_malloc/cJSON_free are not directly visible
// For simplicity, assuming cJSON_malloc and cJSON_free are available (e.g., macros to malloc/free)
extern void* (*cJSON_malloc)(size_t sz);
extern void (*cJSON_free)(void *ptr);
extern cJSON *cJSON_CreateObject(void);
extern cJSON *cJSON_CreateString(const char *string);
extern void cJSON_AddItemToObject(cJSON *object,const char *string,cJSON *item);
extern cJSON *cJSON_Duplicate(const cJSON *item, cJSON_bool recurse);
extern void cJSON_AddItemToArray(cJSON *array, cJSON *item);
extern cJSON_bool cJSON_IsArray(const cJSON * const item);
extern cJSON_bool cJSON_IsObject(const cJSON * const item);
*/

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

/* Helper function to prepend an array index and build a new path string. */
/* Returns a newly allocated string or NULL on failure. Caller must free. */
static unsigned char* prepend_array_index_and_build_path(size_t index_val, const unsigned char *existing_suffix_path) {
    unsigned char *full_pointer = NULL;
    size_t existing_suffix_path_length = 0;

    if (existing_suffix_path == NULL) {
        return NULL;
    }

    existing_suffix_path_length = strlen((const char*)existing_suffix_path);
    /* reserve enough memory for a 64 bit integer (approx 20 chars) + '/' and existing path and '\0' */
    full_pointer = (unsigned char*)cJSON_malloc(1 + 20 + existing_suffix_path_length + 1);
    if (full_pointer == NULL) {
        return NULL;
    }

    /* check if conversion to unsigned long is valid for index_val (size_t) */
    /* This check was in the original caller for child_index, assuming index_val is safe here or checked by caller */
    if (index_val > ULONG_MAX) { /* Defensive check, though child_index was checked before */
        cJSON_free(full_pointer);
        return NULL;
    }
    sprintf((char*)full_pointer, "/%lu%s", (unsigned long)index_val, (const char*)existing_suffix_path); /* /<array_index><path> */
    return full_pointer;
}

/* Helper function to prepend an encoded object key and build a new path string. */
/* Returns a newly allocated string or NULL on failure. Caller must free. */
static unsigned char* prepend_encoded_object_key_and_build_path(const unsigned char *key, const unsigned char *existing_suffix_path) {
    const unsigned char *string_iterator = NULL;
    size_t key_encoded_length = 0;
    size_t existing_suffix_path_length = 0;
    unsigned char *full_pointer = NULL;

    if (key == NULL || existing_suffix_path == NULL) {
        return NULL;
    }

    string_iterator = key;
    for (key_encoded_length = 0; *string_iterator != '\0'; (void)string_iterator++, key_encoded_length++)
    {
        /* character needs to be escaped? */
        if ((*string_iterator == '~') || (*string_iterator == '/'))
        {
            key_encoded_length++;
        }
    }

    existing_suffix_path_length = strlen((const char*)existing_suffix_path);
    /* Allocate memory for "/" + encoded_key + existing_suffix_path + "\0" */
    full_pointer = (unsigned char*)cJSON_malloc(1 + key_encoded_length + existing_suffix_path_length + 1);
    if (full_pointer == NULL) {
        return NULL;
    }

    full_pointer[0] = '/';
    encode_string_as_pointer(full_pointer + 1, key);
    strcat((char*)full_pointer, (const char*)existing_suffix_path);

    return full_pointer;
}


CJSON_PUBLIC(char *) cJSONUtils_FindPointerFromObjectTo(const cJSON * const object, const cJSON * const target)
{
    unsigned char *copy = NULL;
    unsigned char *target_pointer = NULL;
    unsigned char *full_pointer = NULL;
    /* const unsigned char *string used by original logic, now encapsulated or directly used */
    size_t child_index = 0;
    cJSON *current_child = NULL;

    if ((object == NULL) || (target == NULL))
    {
        return NULL;
    }

    if (object == target)
    {
        /* found, return empty string "" as pointer */
        copy = (unsigned char*) cJSON_malloc(sizeof("")); /* Allocate 1 byte for null terminator */
        if (copy == NULL)
        {
            return NULL;
        }
        copy[0] = '\0'; /* Empty string */
        return (char*)copy;
    }

    /* recursively search all children of the object or array */
    for (current_child = object->child; current_child != NULL; (void)(current_child = current_child->next), child_index++)
    {
        target_pointer = (unsigned char*)cJSONUtils_FindPointerFromObjectTo(current_child, target);
        /* found the target? */
        if (target_pointer != NULL)
        {
            if (cJSON_IsArray(object))
            {
                /* check if conversion to unsigned long is valid
                 * This should be eliminated at compile time by dead code elimination
                 * if size_t is an alias of unsigned long, or if it is bigger */
                if (child_index > ULONG_MAX)
                {
                    cJSON_free(target_pointer);
                    return NULL; /* Index out of bounds for ULONG */
                }
                full_pointer = prepend_array_index_and_build_path(child_index, target_pointer);
                cJSON_free(target_pointer); /* target_pointer was allocated by recursive call */
                if (full_pointer == NULL)
                {
                    return NULL; /* Allocation failed in helper */
                }
                return (char*)full_pointer;
            }

            if (cJSON_IsObject(object))
            {
                full_pointer = prepend_encoded_object_key_and_build_path((unsigned char*)current_child->string, target_pointer);
                cJSON_free(target_pointer); /* target_pointer was allocated by recursive call */
                if (full_pointer == NULL)
                {
                    return NULL; /* Allocation failed in helper */
                }
                return (char*)full_pointer;
            }

            /* Should not be reached if object is Array or Object, but as a safeguard */
            /* If object is not array or object, it cannot have children searched this way. */
            /* This implies current_child would be NULL or object type is primitive. */
            cJSON_free(target_pointer);
            return NULL; /* Path not valid through this node type */
        }
    }

    /* not found */
    return NULL;
}

/* sort lists using mergesort */
static cJSON *sort_list(cJSON *list, const cJSON_bool case_sensitive)
{
    cJSON *smaller = NULL;
    cJSON *first = list;
    cJSON *second = list;
    cJSON *current_item = list;
    cJSON *result = list; /* Initialize result with list for early exit cases */
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
    result = NULL; /* Reset result for merging */
    result_tail = NULL; /* Reset result_tail for merging */

    /* Merge the sub-lists */
    while ((first != NULL) && (second != NULL))
    {
        smaller = NULL; /* Re-initialize for clarity in loop */
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

/* Helper function to build a new path string by appending an encoded object key. */
/* Returns a newly allocated string or NULL on failure. Caller must free. */
static unsigned char* build_path_with_encoded_object_key(const unsigned char *base_path, const unsigned char *key) {
    const unsigned char *string_iterator = NULL;
    size_t key_encoded_length = 0;
    size_t base_path_length = 0;
    unsigned char *new_path = NULL;

    if (key == NULL || base_path == NULL) {
        return NULL;
    }

    /* Calculate encoded length for the key */
    string_iterator = key;
    for (key_encoded_length = 0; *string_iterator != '\0'; (void)string_iterator++, key_encoded_length++) {
        if ((*string_iterator == '~') || (*string_iterator == '/')) {
            key_encoded_length++; /* Additional char for escape */
        }
    }
    /* Note: The loop counts original chars, then adds for escapes. The total encoded length for key itself. */
    /* If key is "a/b", loop gives 3. After loop, string_iterator is past 'b'. key_encoded_length should be 3 + 1 (for '/') = 4. */
    /* Correct calculation for encoded key length itself: */
    key_encoded_length = 0; /* Reset for correct calculation */
    string_iterator = key;
    while(*string_iterator != '\0') {
        if ((*string_iterator == '~') || (*string_iterator == '/')) {
            key_encoded_length += 2; /* ~0 or ~1 */
        } else {
            key_encoded_length += 1;
        }
        string_iterator++;
    }


    base_path_length = strlen((const char*)base_path);
    /* Allocate memory for base_path + "/" + encoded_key + "\0" */
    new_path = (unsigned char*)cJSON_malloc(base_path_length + 1 + key_encoded_length + 1);
    if (new_path == NULL) {
        return NULL;
    }

    /* Construct the new path */
    sprintf((char*)new_path, "%s/", (const char*)base_path);
    encode_string_as_pointer(new_path + base_path_length + 1, key);

    return new_path;
}

static void compose_patch(cJSON * const patches, const unsigned char * const operation, const unsigned char * const path, const unsigned char *suffix, const cJSON * const value)
{
    unsigned char *full_path = NULL;
    /* const unsigned char *string; Not needed directly due to helper */
    /* size_t suffix_length, path_length; Not needed directly due to helper */
    cJSON *patch = NULL;

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
        full_path = build_path_with_encoded_object_key(path, suffix);
        if (full_path == NULL) {
            /* Could not allocate path, clean up patch and return */
            /* This behavior is an addition for robustness, original didn't explicitly handle malloc fail here */
            cJSON_Delete(patch); /* Assuming cJSON_Delete correctly handles NULL children */
            return;
        }
        cJSON_AddItemToObject(patch, "path", cJSON_CreateString((const char*)full_path));
        cJSON_free(full_path);
    }

    if (value != NULL)
    {
        cJSON_AddItemToObject(patch, "value", cJSON_Duplicate(value, 1));
    }
    cJSON_AddItemToArray(patches, patch);
}

/* Helper function to format an array-indexed path segment into a pre-allocated buffer. */
static void format_array_indexed_path_segment(unsigned char *output_buffer, const unsigned char *base_path, size_t index_val) {
    /* Caller ensures output_buffer is large enough and index_val is valid (<= ULONG_MAX) */
    sprintf((char*)output_buffer, "%s/%lu", (const char*)base_path, (unsigned long)index_val); /* path of the current array element */
}


void create_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive)
{
    int diff = 0;
    double a = 0.0, b = 0.0, maxVal = 0.0;
    unsigned char *new_path_allocated = NULL; /* For object key paths that need allocation */
    unsigned char *current_path_segment_buffer = NULL; /* For array index paths, re-used buffer */
    /* const unsigned char *string; Not needed directly */
    size_t index = 0;
    /* size_t path_length, from_child_name_length; Not needed directly */
    cJSON *from_child = NULL;
    cJSON *to_child = NULL;
    cJSON_bool compared_double = false;


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
            a = from->valuedouble;
            b = to->valuedouble;

            maxVal = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
            /* Updated comparison for doubles to handle potential precision issues */
            if (maxVal < DBL_MAX * DBL_EPSILON) { /* Check if maxVal is extremely small or zero */
                 compared_double = (fabs(a - b) <= DBL_EPSILON);
            } else {
                 compared_double = (fabs(a - b) <= maxVal * DBL_EPSILON);
            }

            if ((from->valueint != to->valueint) || !compared_double)
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
        {
            index = 0;
            from_child = from->child;
            to_child = to->child;
            /* Allow space for base_path + "/" + 64bit int (approx 20 chars) + "\0" */
            current_path_segment_buffer = (unsigned char*)cJSON_malloc(strlen((const char*)path) + 1 + 20 + 1);
            if (current_path_segment_buffer == NULL) {
                /* Cannot allocate buffer for path segments, cannot proceed */
                return;
            }


            /* generate patches for all array elements that exist in both "from" and "to" */
            for (index = 0; (from_child != NULL) && (to_child != NULL); (void)(from_child = from_child->next), (void)(to_child = to_child->next), index++)
            {
                if (index > ULONG_MAX) /* check before sprintf */
                {
                    cJSON_free(current_path_segment_buffer);
                    return;
                }
                format_array_indexed_path_segment(current_path_segment_buffer, path, index);
                create_patches(patches, current_path_segment_buffer, from_child, to_child, case_sensitive);
            }

            /* remove leftover elements from 'from' that are not in 'to' */
            /* index variable continues from the previous loop for correct path construction */
            for (; (from_child != NULL); (void)(from_child = from_child->next))
            {
                if (index > ULONG_MAX) /* check before sprintf */
                {
                     cJSON_free(current_path_segment_buffer);
                    return;
                }
                /* For "remove", the path suffix is just the index */
                sprintf((char*)current_path_segment_buffer, "%lu", (unsigned long)index);
                compose_patch(patches, (const unsigned char*)"remove", path, current_path_segment_buffer, NULL);
                /* index increment for remove is not needed as per RFC6902 diff logic: path points to element to remove. */
                /* However, the original code implies the index in `path/<index>` for removal, let's keep original intent of path to index */
            }
            /* add new elements in 'to' that were not in 'from' */
            /* Index for "add" operation with "-" suffix refers to appending to array, index value itself not used in suffix for "-" */
            for (; (to_child != NULL); (void)(to_child = to_child->next), index++) /* index increments to reflect growing conceptual array size */
            {
                /* For "add" to the end of an array, the suffix is "-" */
                compose_patch(patches, (const unsigned char*)"add", path, (const unsigned char*)"-", to_child);
            }
            cJSON_free(current_path_segment_buffer);
            return;
        }

        case cJSON_Object:
        {
            from_child = NULL; /* Initialize to NULL */
            to_child = NULL;   /* Initialize to NULL */

            if (from != NULL) { /* from is guaranteed not NULL by initial check */
                from->child = sort_list(from->child, case_sensitive);
            }

            if (to != NULL) { /* to is guaranteed not NULL by initial check */
                to->child = sort_list(to->child, case_sensitive);
            }

            from_child = from->child;
            to_child = to->child;
            /* for all object values in the object with more of them */
            while ((from_child != NULL) || (to_child != NULL))
            {
                if (from_child == NULL)
                {
                    diff = 1; /* from is exhausted, elements in to are additions */
                }
                else if (to_child == NULL)
                {
                    diff = -1; /* to is exhausted, elements in from are removals */
                }
                else
                {
                    diff = compare_strings((unsigned char*)from_child->string, (unsigned char*)to_child->string, case_sensitive);
                }

                if (diff == 0)
                {
                    /* both object keys are the same */
                    new_path_allocated = build_path_with_encoded_object_key(path, (unsigned char*)from_child->string);
                    if (new_path_allocated == NULL) {
                        /* Allocation failure, cannot proceed with this branch */
                        /* Potentially log error or handle more gracefully if required */
                        if (from_child) from_child = from_child->next; /* try to advance to prevent infinite loop if no return */
                        if (to_child) to_child = to_child->next;
                        continue;
                    }

                    create_patches(patches, new_path_allocated, from_child, to_child, case_sensitive);
                    cJSON_free(new_path_allocated);
                    new_path_allocated = NULL; /* Avoid double free if loop continues on error */

                    from_child = from_child->next;
                    to_child = to_child->next;
                }
                else if (diff < 0)
                {
                    /* object element doesn't exist in 'to' --> remove it */
                    compose_patch(patches, (const unsigned char*)"remove", path, (unsigned char*)from_child->string, NULL);
                    from_child = from_child->next;
                }
                else /* diff > 0 */
                {
                    /* object element doesn't exist in 'from' --> add it */
                    compose_patch(patches, (const unsigned char*)"add", path, (unsigned char*)to_child->string, to_child);
                    to_child = to_child->next;
                }
            }
            return;
        }

        default:
            /* cJSON_False, cJSON_True, cJSON_NULL, cJSON_Raw */
            /* These types are compared by reference or direct value if applicable, handled by type mismatch or no change */
            /* If from->type == to->type (e.g. both cJSON_True), no patch is needed unless values differ. */
            /* For bool/null, if types are same, values are same, so no patch. */
            /* String/Number/Array/Object have specific value comparisons. */
            break;
    }
}
/* Potentially one extra closing brace from original user input was here, removed for correctness */
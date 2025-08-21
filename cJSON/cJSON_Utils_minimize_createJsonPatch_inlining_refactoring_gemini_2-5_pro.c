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

/* JSON Patch operation strings defined as constants for safety and maintainability */
static const unsigned char op_replace[] = "replace";
static const unsigned char op_add[] = "add";
static const unsigned char op_remove[] = "remove";

/* securely comparison of floating-point variables */
static cJSON_bool compare_double(double a, double b)
{
    double maxVal = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    return (fabs(a - b) <= maxVal * DBL_EPSILON);
}

/*
 * HELPER FUNCTION: compare_strings
 * This function centralizes the string comparison logic used in sorting and object key comparison.
 * It handles case sensitivity and preserves the exact behavior of the original implementation.
 */
static int compare_strings(const unsigned char *string1, const unsigned char *string2, const cJSON_bool case_sensitive)
{
    if ((string1 == NULL) || (string2 == NULL))
    {
        return 1; /* Consistent with original behavior: non-NULL is considered greater */
    }
    if (string1 == string2)
    {
        return 0;
    }

    if (case_sensitive)
    {
        return strcmp((const char*)string1, (const char*)string2);
    }

    /* Case-insensitive comparison logic from the original code */
    for (; tolower(*string1) == tolower(*string2); (void)string1++, string2++)
    {
        if (*string1 == '\0')
        {
            break;
        }
    }
    return (*string1) - tolower(*string2);
}

/* sort lists using mergesort */
static cJSON *sort_list(cJSON *list, const cJSON_bool case_sensitive)
{
    cJSON *first = list;
    cJSON *second = list;
    cJSON *current_item = list;
    cJSON *result = list;
    cJSON *result_tail = NULL;

    if ((list == NULL) || (list->next == NULL))
    {
        /* A list with zero or one items is already sorted. */
        return result;
    }

    /* Check if the list is already sorted to avoid unnecessary work. */
    while ((current_item != NULL) && (current_item->next != NULL) &&
           (compare_strings((unsigned char*)current_item->string, (unsigned char*)current_item->next->string, case_sensitive) < 0))
    {
        current_item = current_item->next;
    }
    if ((current_item == NULL) || (current_item->next == NULL))
    {
        /* The list is already sorted. */
        return result;
    }

    /* Split the list into two halves using the fast/slow pointer method. */
    current_item = list;
    while (current_item != NULL)
    {
        second = second->next;
        current_item = current_item->next;
        if (current_item != NULL)
        {
            current_item = current_item->next;
        }
    }
    if ((second != NULL) && (second->prev != NULL))
    {
        second->prev->next = NULL;
        second->prev = NULL;
    }

    /* Recursively sort the sub-lists. */
    first = sort_list(first, case_sensitive);
    second = sort_list(second, case_sensitive);
    result = NULL;

    /* Merge the sorted sub-lists. */
    while ((first != NULL) && (second != NULL))
    {
        cJSON *smaller = NULL;
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
            result = smaller;
            result_tail = smaller;
        }
        else
        {
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

    /* Append the remainder of the lists. */
    if (first != NULL)
    {
        if (result == NULL) return first;
        result_tail->next = first;
        first->prev = result_tail;
    }
    else if (second != NULL)
    {
        if (result == NULL) return second;
        result_tail->next = second;
        second->prev = result_tail;
    }

    return result;
}

/*
 * HELPER FUNCTION: build_json_pointer
 * Builds a new JSON Pointer path by appending an escaped segment to a base path.
 * According to RFC 6902, '~' is escaped to '~0' and '/' is escaped to '~1'.
 * The caller is responsible for freeing the returned string.
 */
static unsigned char* build_json_pointer(const unsigned char *base_path, const unsigned char *segment)
{
    size_t base_path_len = strlen((const char*)base_path);
    size_t segment_escaped_len = 0;
    const unsigned char *p = segment;
    unsigned char *new_path = NULL;
    unsigned char *q = NULL;

    /* Calculate the required length for the escaped segment */
    while (*p != '\0')
    {
        if ((*p == '~') || (*p == '/'))
        {
            segment_escaped_len++; /* Needs an extra character for escaping */
        }
        segment_escaped_len++;
        p++;
    }

    /* Allocate memory for "base_path" + "/" + "escaped_segment" + "\0" */
    new_path = (unsigned char*)malloc(base_path_len + 1 /* for '/' */ + segment_escaped_len + 1 /* for '\0' */);
    if (new_path == NULL)
    {
        return NULL;
    }

    /* Build the new path string */
    memcpy(new_path, base_path, base_path_len);
    q = new_path + base_path_len;
    *q++ = '/';

    p = segment;
    while (*p != '\0')
    {
        if (*p == '/')
        {
            *q++ = '~';
            *q++ = '1';
        }
        else if (*p == '~')
        {
            *q++ = '~';
            *q++ = '0';
        }
        else
        {
            *q++ = *p;
        }
        p++;
    }
    *q = '\0';

    return new_path;
}

/* Creates a JSON Patch object and adds it to the patches array. */
static void compose_patch(cJSON * const patches, const unsigned char * const operation, const unsigned char * const path, const unsigned char *suffix, const cJSON * const value)
{
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
        unsigned char *full_path = build_json_pointer(path, suffix);
        if (full_path)
        {
            cJSON_AddItemToObject(patch, "path", cJSON_CreateString((const char*)full_path));
            free(full_path);
        }
    }

    if (value != NULL)
    {
        cJSON_AddItemToObject(patch, "value", cJSON_Duplicate(value, 1));
    }
    cJSON_AddItemToArray(patches, patch);
}

/* Forward declaration for the main recursive function. */
void create_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive);

/* HELPER FUNCTION: handle_array_diff - Generates patches for differences between two JSON arrays. */
static void handle_array_diff(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive)
{
    size_t index = 0;
    cJSON *from_child = from->child;
    cJSON *to_child = to->child;
    /* Allow space for 64bit int (log10(2^64) < 20) + path + "/" */
    unsigned char *new_path = (unsigned char*)malloc(strlen((const char*)path) + 20 + 2);
    if(new_path == NULL) return;

    /* Generate patches for elements that exist in both arrays */
    for (index = 0; (from_child != NULL) && (to_child != NULL); (void)(from_child = from_child->next), (void)(to_child = to_child->next), index++)
    {
        if (index > ULONG_MAX) { free(new_path); return; } /* Safety check */
        sprintf((char*)new_path, "%s/%lu", path, (unsigned long)index);
        create_patches(patches, new_path, from_child, to_child, case_sensitive);
    }

    /* Remove leftover elements from 'from' that are not in 'to' */
    for (; (from_child != NULL); (void)(from_child = from_child->next), index++)
    {
        if (index > ULONG_MAX) { free(new_path); return; } /* Safety check */
        sprintf((char*)new_path, "%lu", (unsigned long)index);
        compose_patch(patches, op_remove, path, new_path, NULL);
    }

    /* Add new elements in 'to' that were not in 'from' */
    for (; (to_child != NULL); (void)(to_child = to_child->next))
    {
        compose_patch(patches, op_add, path, (const unsigned char*)"-", to_child);
    }

    free(new_path);
}

/* HELPER FUNCTION: handle_object_diff - Generates patches for differences between two JSON objects. */
static void handle_object_diff(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive)
{
    cJSON *from_child = (from != NULL) ? sort_list(from->child, case_sensitive) : NULL;
    cJSON *to_child = (to != NULL) ? sort_list(to->child, case_sensitive) : NULL;
    if (from) from->child = from_child;
    if (to) to->child = to_child;


    while ((from_child != NULL) || (to_child != NULL))
    {
        int diff;
        if (from_child == NULL)
        {
            diff = 1; /* 'to' has an item that 'from' doesn't */
        }
        else if (to_child == NULL)
        {
            diff = -1; /* 'from' has an item that 'to' doesn't */
        }
        else
        {
            diff = compare_strings((unsigned char*)from_child->string, (unsigned char*)to_child->string, case_sensitive);
        }

        if (diff == 0)
        {
            /* Keys match, so compare values recursively. */
            unsigned char *new_path = build_json_pointer(path, (unsigned char*)from_child->string);
            if(new_path)
            {
                create_patches(patches, new_path, from_child, to_child, case_sensitive);
                free(new_path);
            }
            from_child = from_child->next;
            to_child = to_child->next;
        }
        else if (diff < 0)
        {
            /* Item in 'from' is not in 'to' -> remove it. */
            compose_patch(patches, op_remove, path, (unsigned char*)from_child->string, NULL);
            from_child = from_child->next;
        }
        else
        {
            /* Item in 'to' is not in 'from' -> add it. */
            compose_patch(patches, op_add, path, (unsigned char*)to_child->string, to_child);
            to_child = to_child->next;
        }
    }
}

/* Main recursive function to generate JSON patches by comparing 'from' and 'to' JSON objects. */
void create_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive)
{
    if ((from == NULL) || (to == NULL))
    {
        return;
    }

    if ((from->type & 0xFF) != (to->type & 0xFF))
    {
        compose_patch(patches, op_replace, path, NULL, to);
        return;
    }

    switch (from->type & 0xFF)
    {
        case cJSON_Number:
            if ((from->valueint != to->valueint) || !compare_double(from->valuedouble, to->valuedouble))
            {
                compose_patch(patches, op_replace, path, NULL, to);
            }
            return;

        case cJSON_String:
            if (strcmp(from->valuestring, to->valuestring) != 0)
            {
                compose_patch(patches, op_replace, path, NULL, to);
            }
            return;

        case cJSON_Array:
            handle_array_diff(patches, path, from, to, case_sensitive);
            return;

        case cJSON_Object:
            handle_object_diff(patches, path, from, to, case_sensitive);
            return;

        default:
            /* cJSON_False, cJSON_True, cJSON_NULL, cJSON_Raw are compared by type only, which was already checked. */
            break;
    }
}

int main() {
    printf("\n--- Testing create_patches ---\n");
    const char *from_json_string = "{\n"
                                   "  \"name\": \"John Doe\",\n"
                                   "  \"age\": 30,\n"
                                   "  \"city\": \"Anytown\",\n"
                                   "  \"tags\": [\"json\", \"c\"]\n"
                                   "}";

    const char *to_json_string = "{\n"
                                 "  \"name\": \"Jane Doe\",\n"
                                 "  \"age\": 31,\n"
                                 "  \"occupation\": \"Engineer\",\n"
                                 "  \"tags\": [\"json\", \"c\", \"patch\"]\n"
                                 "}";

    cJSON *from_json = cJSON_Parse(from_json_string);
    cJSON *to_json = cJSON_Parse(to_json_string);
    cJSON *patches_array = cJSON_CreateArray();

    create_patches(patches_array, (const unsigned char *)"", from_json, to_json, cJSON_True);

    char *patches_string = cJSON_Print(patches_array);
    printf("Generated Patches:\n%s\n", patches_string);
    free(patches_string);

    cJSON_Delete(from_json);
    cJSON_Delete(to_json);
    cJSON_Delete(patches_array);

    return 0;
}
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

/* Forward declarations for static functions */
static int compare_strings(const unsigned char *string1, const unsigned char *string2, cJSON_bool case_sensitive);
static unsigned char* build_json_pointer(const unsigned char *base_path, const unsigned char *segment);
static void compose_patch(cJSON * const patches, const unsigned char * const operation, const unsigned char * const path, const unsigned char *suffix, const cJSON * const value);
static void create_patches_for_array(cJSON * const patches, const unsigned char * const path, cJSON * const from_array, cJSON * const to_array, const cJSON_bool case_sensitive);
static void create_patches_for_object(cJSON * const patches, const unsigned char * const path, cJSON * const from_object, cJSON * const to_object, const cJSON_bool case_sensitive);
static void create_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive);

/* securely comparison of floating-point variables */
static cJSON_bool compare_double(double a, double b)
{
    double maxVal = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    return (fabs(a - b) <= maxVal * DBL_EPSILON);
}

/* A helper function for case-sensitive and case-insensitive string comparison. */
static int compare_strings(const unsigned char *string1, const unsigned char *string2, const cJSON_bool case_sensitive)
{
    if ((string1 == NULL) || (string2 == NULL))
    {
        /* Treat NULL strings as "greater" than non-NULL strings for a consistent sort order. */
        return (string1 == string2) ? 0 : (string1 == NULL) ? 1 : -1;
    }

    if (string1 == string2)
    {
        return 0;
    }

    if (case_sensitive)
    {
        return strcmp((const char*)string1, (const char*)string2);
    }

    /* Case-insensitive comparison */
    for (; tolower(*string1) == tolower(*string2); (void)string1++, string2++)
    {
        if (*string1 == '\0')
        {
            /* strings are equal */
            return 0;
        }
    }

    return tolower(*string1) - tolower(*string2);
}

/*
 * Builds a new JSON Pointer path by appending a segment.
 * Escapes '~' to '~0' and '/' to '~1' as per RFC 6901.
 * The caller is responsible for freeing the returned memory.
 */
static unsigned char* build_json_pointer(const unsigned char *base_path, const unsigned char *segment)
{
    size_t base_path_length = strlen((const char*)base_path);
    size_t segment_length_escaped = 0;
    const unsigned char *p = segment;
    unsigned char *new_path = NULL;
    unsigned char *destination = NULL;

    /* Calculate the required length for the escaped segment */
    while (*p != '\0')
    {
        if ((*p == '~') || (*p == '/'))
        {
            segment_length_escaped += 2; /* e.g., '/' becomes '~1' (2 chars) */
        }
        else
        {
            segment_length_escaped++;
        }
        p++;
    }

    /* Allocate memory: base_path + '/' + escaped_segment + '\0' */
    new_path = (unsigned char*)malloc(base_path_length + 1 + segment_length_escaped + 1);
    if (new_path == NULL)
    {
        return NULL;
    }

    /* Copy base_path and the separator */
    memcpy(new_path, base_path, base_path_length);
    new_path[base_path_length] = '/';
    destination = new_path + base_path_length + 1;

    /* Copy and escape the segment */
    p = segment;
    while (*p != '\0')
    {
        if (*p == '/')
        {
            *destination++ = '~';
            *destination++ = '1';
        }
        else if (*p == '~')
        {
            *destination++ = '~';
            *destination++ = '0';
        }
        else
        {
            *destination++ = *p;
        }
        p++;
    }
    *destination = '\0';

    return new_path;
}

/* sort lists using mergesort */
static cJSON *sort_list(cJSON *list, const cJSON_bool case_sensitive)
{
    cJSON *first = list;
    cJSON *second = NULL;
    cJSON *result = NULL;
    cJSON *result_tail = NULL;
    cJSON *smaller = NULL;

    if ((list == NULL) || (list->next == NULL))
    {
        /* A list with zero or one elements is already sorted. */
        return list;
    }

    /* Check if the list is already sorted */
    cJSON *iterator = list;
    while ((iterator != NULL) && (iterator->next != NULL))
    {
        if (compare_strings((unsigned char*)iterator->string, (unsigned char*)iterator->next->string, case_sensitive) > 0)
        {
            /* List is not sorted */
            break;
        }
        iterator = iterator->next;
    }
    if ((iterator == NULL) || (iterator->next == NULL))
    {
        /* The list was already sorted. */
        return list;
    }

    /* === Divide phase === */
    /* Find the middle of the list using the fast/slow pointer method */
    cJSON *slow_pointer = list;
    cJSON *fast_pointer = list->next; /* Start fast_pointer one step ahead */
    while ((fast_pointer != NULL) && (fast_pointer->next != NULL))
    {
        slow_pointer = slow_pointer->next;
        fast_pointer = fast_pointer->next->next;
    }

    /* Split the list into two halves */
    second = slow_pointer->next;
    slow_pointer->next = NULL;
    if (second != NULL)
    {
        second->prev = NULL;
    }

    /* Recursively sort the sub-lists. */
    first = sort_list(first, case_sensitive);
    second = sort_list(second, case_sensitive);
    result = NULL;

    /* === Conquer (Merge) phase === */
    while ((first != NULL) || (second != NULL))
    {
        if (first == NULL)
        {
            smaller = second;
            second = (second != NULL) ? second->next : NULL;
        }
        else if (second == NULL)
        {
            smaller = first;
            first = (first != NULL) ? first->next : NULL;
        }
        else
        {
            if (compare_strings((unsigned char*)first->string, (unsigned char*)second->string, case_sensitive) < 0)
            {
                smaller = first;
                first = first->next;
            }
            else
            {
                smaller = second;
                second = second->next;
            }
        }


        if (result == NULL)
        {
            /* start merged list with the smaller element */
            result = smaller;
            result_tail = smaller;
        }
        else
        {
            /* add smaller element to the list */
            result_tail->next = smaller;
            smaller->prev = result_tail;
            result_tail = smaller;
        }
    }

    return result;
}

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
        if (full_path != NULL)
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

static void create_patches_for_array(cJSON * const patches, const unsigned char * const path, cJSON * const from_array, cJSON * const to_array, const cJSON_bool case_sensitive)
{
    size_t index = 0;
    cJSON *from_child = from_array->child;
    cJSON *to_child = to_array->child;
    /* Max digits for 64-bit size_t is 20. Add space for path, '/', and '\0'. */
    static const int MAX_SIZE_T_DECIMAL_LENGTH = 20;
    size_t path_len = strlen((const char*)path);
    unsigned char *new_path = (unsigned char*)malloc(path_len + 1 + MAX_SIZE_T_DECIMAL_LENGTH + 1);

    if (new_path == NULL)
    {
        return;
    }

    /* generate patches for all array elements that exist in both "from" and "to" */
    for (index = 0; (from_child != NULL) && (to_child != NULL); (void)(from_child = from_child->next), (void)(to_child = to_child->next), index++)
    {
        /* check if conversion to unsigned long is valid */
        if (index > ULONG_MAX)
        {
            break; /* Prevent overflow */
        }
        sprintf((char*)new_path, "%s/%lu", path, (unsigned long)index);
        create_patches(patches, new_path, from_child, to_child, case_sensitive);
    }

    /* remove leftover elements from 'from' that are not in 'to' */
    for (; (from_child != NULL); (void)(from_child = from_child->next))
    {
        if (index > ULONG_MAX)
        {
            break; /* Prevent overflow */
        }
        sprintf((char*)new_path, "%lu", (unsigned long)index);
        compose_patch(patches, (const unsigned char*)"remove", path, new_path, NULL);
    }
    /* add new elements in 'to' that were not in 'from' */
    for (; (to_child != NULL); (void)(to_child = to_child->next), index++)
    {
        compose_patch(patches, (const unsigned char*)"add", path, (const unsigned char*)"-", to_child);
    }

    free(new_path);
}

static void create_patches_for_object(cJSON * const patches, const unsigned char * const path, cJSON * const from_object, cJSON * const to_object, const cJSON_bool case_sensitive)
{
    cJSON *from_child = NULL;
    cJSON *to_child = NULL;

    if (from_object != NULL)
    {
        from_object->child = sort_list(from_object->child, case_sensitive);
    }
    if (to_object != NULL)
    {
        to_object->child = sort_list(to_object->child, case_sensitive);
    }

    from_child = from_object->child;
    to_child = to_object->child;

    while ((from_child != NULL) || (to_child != NULL))
    {
        int diff = compare_strings((unsigned char*)(from_child ? from_child->string : NULL),
                                   (unsigned char*)(to_child ? to_child->string : NULL),
                                   case_sensitive);

        if (diff == 0)
        {
            /* both object keys are the same, check for value differences */
            unsigned char* new_path = build_json_pointer(path, (unsigned char*)from_child->string);
            if (new_path != NULL)
            {
                create_patches(patches, new_path, from_child, to_child, case_sensitive);
                free(new_path);
            }
            from_child = from_child->next;
            to_child = to_child->next;
        }
        else if (diff < 0)
        {
            /* object element exists in 'from' but not in 'to' --> remove it */
            compose_patch(patches, (const unsigned char*)"remove", path, (unsigned char*)from_child->string, NULL);
            from_child = from_child->next;
        }
        else /* diff > 0 */
        {
            /* object element exists in 'to' but not in 'from' --> add it */
            compose_patch(patches, (const unsigned char*)"add", path, (unsigned char*)to_child->string, to_child);
            to_child = to_child->next;
        }
    }
}


void create_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive)
{
    if ((from == NULL) || (to == NULL))
    {
        return;
    }

    /* If types are different, 'replace' the entire value */
    if ((from->type & 0xFF) != (to->type & 0xFF))
    {
        compose_patch(patches, (const unsigned char*)"replace", path, NULL, to);
        return;
    }

    /* Dispatch to type-specific handlers */
    switch (from->type & 0xFF)
    {
        case cJSON_Number:
            if ((from->valueint != to->valueint) || !compare_double(from->valuedouble, to->valuedouble))
            {
                compose_patch(patches, (const unsigned char*)"replace", path, NULL, to);
            }
            break;

        case cJSON_String:
            if (strcmp(from->valuestring, to->valuestring) != 0)
            {
                compose_patch(patches, (const unsigned char*)"replace", path, NULL, to);
            }
            break;

        case cJSON_Array:
            create_patches_for_array(patches, path, from, to, case_sensitive);
            break;

        case cJSON_Object:
            create_patches_for_object(patches, path, from, to, case_sensitive);
            break;

        default:
            /* cJSON_False, cJSON_True, cJSON_NULL have no value to compare */
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
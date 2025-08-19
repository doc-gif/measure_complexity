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

/* A number sufficiently large to hold digit representation of a size_t. */
#define MAX_SIZE_T_STRING_LENGTH 20

/* Forward declarations for recursive functions. */
static void create_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive);
static cJSON *sort_list(cJSON *list, const cJSON_bool case_sensitive);


/* securely comparison of floating-point variables */
static cJSON_bool compare_double(double a, double b)
{
    double maxVal = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    return (fabs(a - b) <= maxVal * DBL_EPSILON);
}

/* copy a string while escaping '~' and '/' with ~0 and ~1 JSON pointer escape codes */
static void encode_string_as_pointer(unsigned char *destination, const unsigned char *source)
{
    while (*source)
    {
        if (*source == '/')
        {
            *destination++ = '~';
            *destination++ = '1';
        }
        else if (*source == '~')
        {
            *destination++ = '~';
            *destination++ = '0';
        }
        else
        {
            *destination++ = *source;
        }
        source++;
    }
    *destination = '\0';
}

/*
 * Helper function to compare two strings, with optional case sensitivity.
 * Returns an integer less than, equal to, or greater than zero if string1 is
 * found, respectively, to be less than, to match, or be greater than string2.
 */
static int compare_strings(const unsigned char *string1, const unsigned char *string2, const cJSON_bool case_sensitive)
{
    if ((string1 == NULL) || (string2 == NULL))
    {
        /* Treat NULL pointers as "greater" to ensure non-NULL keys are handled first. */
        if (string1 == string2) return 0;
        return (string1 == NULL) ? 1 : -1;
    }
    if (string1 == string2)
    {
        return 0;
    }

    if (case_sensitive)
    {
        return strcmp((const char *)string1, (const char *)string2);
    }

    /* Case-insensitive comparison */
    for (;;)
    {
        int diff = tolower(*string1) - tolower(*string2);
        if (diff != 0 || *string1 == '\0')
        {
            return diff;
        }
        string1++;
        string2++;
    }
}


/* sort lists using mergesort */
static cJSON *sort_list(cJSON *list, const cJSON_bool case_sensitive)
{
    cJSON *first = list;
    cJSON *second = list;
    cJSON *current_item = list;
    cJSON *result = list;
    cJSON *result_tail = NULL;
    cJSON_bool is_sorted = true;

    if ((list == NULL) || (list->next == NULL))
    {
        /* One entry is sorted already. */
        return result;
    }

    /* Check if the list is already sorted as an optimization */
    while ((current_item != NULL) && (current_item->next != NULL))
    {
        if (compare_strings((unsigned char*)current_item->string, (unsigned char*)current_item->next->string, case_sensitive) > 0)
        {
            is_sorted = false;
            break;
        }
        current_item = current_item->next;
    }
    if (is_sorted)
    {
        return result;
    }

    /* If not sorted, apply mergesort */
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
        cJSON *smaller = NULL;
        int comparison = compare_strings((unsigned char*)first->string, (unsigned char*)second->string, case_sensitive);

        if (comparison < 0)
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

    /* Append the rest of the non-empty list. */
    cJSON *rest = (first != NULL) ? first : second;
    if (rest != NULL) {
        if (result == NULL)
        {
            return rest;
        }
        result_tail->next = rest;
        rest->prev = result_tail;
    }

    return result;
}

static void compose_patch(cJSON * const patches, const unsigned char * const operation, const unsigned char * const path, const unsigned char *suffix, const cJSON * const value)
{
    if ((patches == NULL) || (operation == NULL) || (path == NULL))
    {
        return;
    }

    cJSON *patch = cJSON_CreateObject();
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
        size_t path_length = strlen((const char*)path);
        size_t suffix_length = 0;
        const unsigned char *p = suffix;
        while (*p)
        {
            if (*p == '~' || *p == '/')
            {
                suffix_length++; /* for the escape character '~' */
            }
            suffix_length++;
            p++;
        }

        unsigned char *full_path = (unsigned char*)malloc(path_length + suffix_length + sizeof("/"));
        if (full_path)
        {
            sprintf((char*)full_path, "%s/", (const char*)path);
            encode_string_as_pointer(full_path + path_length + 1, suffix);
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

/* Generate patches for array differences */
static void create_patches_for_array(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive)
{
    size_t index = 0;
    cJSON *from_child = from->child;
    cJSON *to_child = to->child;
    unsigned char *new_path = (unsigned char*)malloc(strlen((const char*)path) + MAX_SIZE_T_STRING_LENGTH + sizeof("/"));
    if (new_path == NULL) return;

    /* Generate patches for all array elements that exist in both "from" and "to" */
    while ((from_child != NULL) && (to_child != NULL))
    {
        if (index > ULONG_MAX)
        {
            free(new_path);
            return;
        }
        sprintf((char*)new_path, "%s/%lu", path, (unsigned long)index);
        create_patches(patches, new_path, from_child, to_child, case_sensitive);

        from_child = from_child->next;
        to_child = to_child->next;
        index++;
    }

    /* Remove leftover elements from 'from' that are not in 'to' */
    while (from_child != NULL)
    {
        if (index > ULONG_MAX)
        {
            free(new_path);
            return;
        }
        sprintf((char*)new_path, "%lu", (unsigned long)index);
        compose_patch(patches, (const unsigned char*)"remove", path, new_path, NULL);
        from_child = from_child->next;
    }

    /* Add new elements in 'to' that were not in 'from' */
    while (to_child != NULL)
    {
        compose_patch(patches, (const unsigned char*)"add", path, (const unsigned char*)"-", to_child);
        to_child = to_child->next;
    }

    free(new_path);
}

/* Generate patches for object differences */
static void create_patches_for_object(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive)
{
    if (from != NULL) from->child = sort_list(from->child, case_sensitive);
    if (to != NULL) to->child = sort_list(to->child, case_sensitive);

    cJSON *from_child = (from != NULL) ? from->child : NULL;
    cJSON *to_child = (to != NULL) ? to->child : NULL;

    while ((from_child != NULL) || (to_child != NULL))
    {
        const unsigned char* from_key = (from_child != NULL) ? (unsigned char*)from_child->string : NULL;
        const unsigned char* to_key = (to_child != NULL) ? (unsigned char*)to_child->string : NULL;
        int diff = compare_strings(from_key, to_key, case_sensitive);

        if (diff == 0)
        {
            /* Keys match, compare values recursively */
            size_t path_length = strlen((const char*)path);
            size_t key_length = 0;
            const unsigned char* p = from_key;
            while (*p) {
                if (*p == '~' || *p == '/') key_length++;
                key_length++;
                p++;
            }

            unsigned char* new_path = (unsigned char*)malloc(path_length + key_length + sizeof("/"));
            if (new_path) {
                sprintf((char*)new_path, "%s/", path);
                encode_string_as_pointer(new_path + path_length + 1, from_key);
                create_patches(patches, new_path, from_child, to_child, case_sensitive);
                free(new_path);
            }

            from_child = from_child->next;
            to_child = to_child->next;
        }
        else if (diff < 0)
        {
            /* Key in 'from' is smaller, meaning it's not in 'to'. Remove it. */
            compose_patch(patches, (const unsigned char*)"remove", path, from_key, NULL);
            from_child = from_child->next;
        }
        else
        {
            /* Key in 'to' is smaller, meaning it's not in 'from'. Add it. */
            compose_patch(patches, (const unsigned char*)"add", path, to_key, to_child);
            to_child = to_child->next;
        }
    }
}


/* Main function to generate patches by comparing 'from' and 'to' JSON objects. */
void create_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive)
{
    if ((patches == NULL) || (from == NULL) || (to == NULL))
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
            /* cJSON_NULL, cJSON_True, cJSON_False: if types are same, they are equal. Do nothing. */
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
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

CJSON_PUBLIC(char *) cJSONUtils_FindPointerFromObjectTo(const cJSON * const object, const cJSON * const target)
{
    unsigned char *copy;
    unsigned char *target_pointer;
    unsigned char *full_pointer;
    const unsigned char *string;
    size_t length;
    size_t pointer_encoded_length;
    size_t child_index = 0;
    cJSON *current_child = 0;

    if ((object == NULL) || (target == NULL))
    {
        return NULL;
    }

    if (object == target)
    {
        /* found */
        string = (const unsigned char*)"";
        length = 0;
        copy = NULL;

        length = strlen((const char*)string) + sizeof("");
        copy = (unsigned char*) cJSON_malloc(length);
        if (copy == NULL)
        {
            return NULL;
        }
        memcpy(copy, string, length);

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
                /* reserve enough memory for a 64 bit integer + '/' and '\0' */
                full_pointer = (unsigned char*)cJSON_malloc(strlen((char*)target_pointer) + 20 + sizeof("/"));
                /* check if conversion to unsigned long is valid
                 * This should be eliminated at compile time by dead code elimination
                 * if size_t is an alias of unsigned long, or if it is bigger */
                if (child_index > ULONG_MAX)
                {
                    cJSON_free(target_pointer);
                    cJSON_free(full_pointer);
                    return NULL;
                }
                sprintf((char*)full_pointer, "/%lu%s", (unsigned long)child_index, target_pointer); /* /<array_index><path> */
                cJSON_free(target_pointer);

                return (char*)full_pointer;
            }

            if (cJSON_IsObject(object))
            {
                string = (unsigned char*)current_child->string;
                for (pointer_encoded_length = 0; *string != '\0'; (void)string++, pointer_encoded_length++)
                {
                    /* character needs to be escaped? */
                    if ((*string == '~') || (*string == '/'))
                    {
                        pointer_encoded_length++;
                    }
                }

                full_pointer = (unsigned char*)cJSON_malloc(strlen((char*)target_pointer) + pointer_encoded_length + 2);
                full_pointer[0] = '/';
                encode_string_as_pointer(full_pointer + 1, (unsigned char*)current_child->string);
                strcat((char*)full_pointer, (char*)target_pointer);
                cJSON_free(target_pointer);

                return (char*)full_pointer;
            }

            /* reached leaf of the tree, found nothing */
            cJSON_free(target_pointer);
            return NULL;
        }
    }

    /* not found */
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

/** リファクタリングにより新たに定義された **/
/* calculate the length of a string after applying JSON pointer escaping */
static size_t get_encoded_string_length(const unsigned char *source)
{
    size_t length = 0;
    const unsigned char *string = source;
    for (length = 0; *string != '\0'; (void)string++, length++)
    {
        /* character needs to be escaped? */
        if ((*string == '~') || (*string == '/'))
        {
            length++;
        }
    }
    return length;
}

/** リファクタリングにより新たに定義された **/
/* creates a full path string by appending a suffix with JSON pointer escaping */
static unsigned char* create_path_with_suffix(const unsigned char * const path, const unsigned char *suffix)
{
    unsigned char *full_path;
    size_t suffix_length;
    size_t path_length;

    suffix_length = get_encoded_string_length(suffix);
    path_length = strlen((const char*)path);
    full_path = (unsigned char*)cJSON_malloc(path_length + suffix_length + sizeof("/"));

    sprintf((char*)full_path, "%s/", (const char*)path);
    encode_string_as_pointer(full_path + path_length + 1, suffix);

    return full_path;
}

static void compose_patch(cJSON * const patches, const unsigned char * const operation, const unsigned char * const path, const unsigned char *suffix, const cJSON * const value)
{
    unsigned char *full_path = NULL;
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
        full_path = create_path_with_suffix(path, suffix);
        cJSON_AddItemToObject(patch, "path", cJSON_CreateString((const char*)full_path));
        cJSON_free(full_path);
    }

    if (value != NULL)
    {
        cJSON_AddItemToObject(patch, "value", cJSON_Duplicate(value, 1));
    }
    cJSON_AddItemToArray(patches, patch);
}

/** リファクタリングにより新たに定義された **/
/* compares two cJSON numbers for equality, considering floating point precision */
static cJSON_bool compare_json_numbers(const cJSON * const from, const cJSON * const to)
{
    double a;
    double b;
    double maxVal;

    a = from->valuedouble;
    b = to->valuedouble;

    maxVal = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    return (cJSON_bool)(fabs(a - b) <= maxVal * DBL_EPSILON);
}

/** リファクタリングにより新たに定義された **/
/* handles patching for cJSON array types */
static void handle_array_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive)
{
    size_t index;
    unsigned char *new_path;
    cJSON *from_child;
    cJSON *to_child;

    new_path = (unsigned char*)cJSON_malloc(strlen((const char*)path) + 20 + sizeof("/")); /* Allow space for 64bit int. log10(2^64) = 20 */
    if (new_path == NULL)
    {
        return;
    }

    index = 0;
    from_child = from->child;
    to_child = to->child;

    /* generate patches for all array elements that exist in both "from" and "to" */
    for (index = 0; (from_child != NULL) && (to_child != NULL); (void)(from_child = from_child->next), (void)(to_child = to_child->next), index++)
    {
        /* check if conversion to unsigned long is valid
         * This should be eliminated at compile time by dead code elimination
         * if size_t is an alias of unsigned long, or if it is bigger */
        if (index > ULONG_MAX)
        {
            cJSON_free(new_path);
            return;
        }
        sprintf((char*)new_path, "%s/%lu", path, (unsigned long)index); /* path of the current array element */
        create_patches(patches, new_path, from_child, to_child, case_sensitive);
    }

    /* remove leftover elements from 'from' that are not in 'to' */
    for (; (from_child != NULL); (void)(from_child = from_child->next))
    {
        /* check if conversion to unsigned long is valid
         * This should be eliminated at compile time by dead code elimination
         * if size_t is an alias of unsigned long, or if it is bigger */
        if (index > ULONG_MAX)
        {
            cJSON_free(new_path);
            return;
        }
        sprintf((char*)new_path, "%lu", (unsigned long)index);
        compose_patch(patches, (const unsigned char*)"remove", path, new_path, NULL);
    }
    /* add new elements in 'to' that were not in 'from' */
    for (; (to_child != NULL); (void)(to_child = to_child->next), index++)
    {
        compose_patch(patches, (const unsigned char*)"add", path, (const unsigned char*)"-", to_child);
    }
    cJSON_free(new_path);
}

/** リファクタリングにより新たに定義された **/
/* handles patching for cJSON object types */
static void handle_object_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive)
{
    int diff;
    unsigned char *new_path = NULL;
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
    /* for all object values in the object with more of them */
    while ((from_child != NULL) || (to_child != NULL))
    {
        if (from_child == NULL)
        {
            diff = 1;
        }
        else if (to_child == NULL)
        {
            diff = -1;
        }
        else
        {
            diff = compare_strings((unsigned char*)from_child->string, (unsigned char*)to_child->string, case_sensitive);
        }

        if (diff == 0)
        {
            /* both object keys are the same */
            new_path = create_path_with_suffix(path, (unsigned char*)from_child->string);

            /* create a patch for the element */
            create_patches(patches, new_path, from_child, to_child, case_sensitive);
            cJSON_free(new_path);

            from_child = from_child->next;
            to_child = to_child->next;
        }
        else if (diff < 0)
        {
            /* object element doesn't exist in 'to' --> remove it */
            compose_patch(patches, (const unsigned char*)"remove", path, (unsigned char*)from_child->string, NULL);

            from_child = from_child->next;
        }
        else
        {
            /* object element doesn't exist in 'from' --> add it */
            compose_patch(patches, (const unsigned char*)"add", path, (unsigned char*)to_child->string, to_child);

            to_child = to_child->next;
        }
    }
}

void create_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive)
{
    cJSON_bool compared_double;

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
            compared_double = compare_json_numbers(from, to);
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
            handle_array_patches(patches, path, from, to, case_sensitive);
            return;

        case cJSON_Object:
            handle_object_patches(patches, path, from, to, case_sensitive);
            return;

        default:
            break;
    }
}

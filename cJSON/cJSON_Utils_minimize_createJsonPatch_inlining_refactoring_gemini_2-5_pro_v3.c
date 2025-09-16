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

#if !defined(_CRT_SECURE_NO_DEPRECATE) && defined(_MSC_VER)
#define _CRT_SECURE_NO_DEPRECATE
#endif

#ifdef __GNUCC__
#pragma GCC visibility push(default)
#endif
#if defined(_MSC_VER)
#pragma warning (push)
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

#ifdef true
#undef true
#endif
#define true ((cJSON_bool)1)

#ifdef false
#undef false
#endif
#define false ((cJSON_bool)0)

static cJSON_bool compare_double(double a, double b)
{
    double max_val = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    return (fabs(a - b) <= max_val * DBL_EPSILON);
}

static int compare_item_strings(const cJSON * const item1, const cJSON * const item2, const cJSON_bool case_sensitive)
{
    const unsigned char *string1 = (const unsigned char *)item1->string;
    const unsigned char *string2 = (const unsigned char *)item2->string;

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
        return strcmp((const char *)string1, (const char *)string2);
    }

    for ( ; tolower(*string1) == tolower(*string2); (void)string1++, string2++)
    {
        if (*string1 == '\0')
        {
            return 0;
        }
    }

    return tolower(*string1) - tolower(*string2);
}

static cJSON * merge_sorted_lists(cJSON *first, cJSON *second, const cJSON_bool case_sensitive)
{
    cJSON *result = NULL;
    cJSON *result_tail = NULL;
    cJSON *smaller = NULL;

    while ((first != NULL) && (second != NULL))
    {
        if (compare_item_strings(first, second, case_sensitive) <= 0)
        {
            smaller = first;
            first = first->next;
        }
        else
        {
            smaller = second;
            second = second->next;
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
    }

    if (first != NULL)
    {
        if (result == NULL)
        {
            return first;
        }
        result_tail->next = first;
        first->prev = result_tail;
    }
    else if (second != NULL)
    {
        if (result == NULL)
        {
            return second;
        }
        result_tail->next = second;
        second->prev = result_tail;
    }

    return result;
}

static cJSON * sort_list(cJSON *list, const cJSON_bool case_sensitive)
{
    cJSON *first = list;
    cJSON *second = NULL;
    cJSON *current = list;
    cJSON *slow = NULL;
    cJSON *fast = NULL;

    if ((list == NULL) || (list->next == NULL))
    {
        return list;
    }

    while ((current != NULL) && (current->next != NULL))
    {
        if (compare_item_strings(current, current->next, case_sensitive) > 0)
        {
            break;
        }
        current = current->next;
    }
    if ((current == NULL) || (current->next == NULL))
    {
        return list;
    }

    slow = list;
    fast = list->next;
    while ((fast != NULL) && (fast->next != NULL))
    {
        slow = slow->next;
        fast = fast->next->next;
    }
    second = slow->next;
    slow->next = NULL;
    if (second != NULL)
    {
        second->prev = NULL;
    }

    first = sort_list(first, case_sensitive);
    second = sort_list(second, case_sensitive);

    return merge_sorted_lists(first, second, case_sensitive);
}

static unsigned char * create_patched_path(const unsigned char * const path, const unsigned char * const suffix)
{
    size_t path_length = strlen((const char *)path);
    size_t suffix_escaped_length = 0;
    const unsigned char *suffix_ptr = suffix;
    unsigned char *full_path = NULL;
    unsigned char *destination = NULL;

    while (*suffix_ptr != '\0')
    {
        suffix_escaped_length++;
        if ((*suffix_ptr == '~') || (*suffix_ptr == '/'))
        {
            suffix_escaped_length++;
        }
        suffix_ptr++;
    }

    full_path = (unsigned char *)malloc(path_length + suffix_escaped_length + 2);
    if (full_path == NULL)
    {
        return NULL;
    }

    memcpy(full_path, path, path_length);
    full_path[path_length] = '/';
    destination = full_path + path_length + 1;

    suffix_ptr = suffix;
    while (*suffix_ptr != '\0')
    {
        if (*suffix_ptr == '/')
        {
            *destination++ = '~';
            *destination++ = '1';
        }
        else if (*suffix_ptr == '~')
        {
            *destination++ = '~';
            *destination++ = '0';
        }
        else
        {
            *destination++ = *suffix_ptr;
        }
        suffix_ptr++;
    }
    *destination = '\0';

    return full_path;
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
        unsigned char *full_path = create_patched_path(path, suffix);
        if (full_path != NULL)
        {
            cJSON_AddItemToObject(patch, "path", cJSON_CreateString((const char *)full_path));
            free(full_path);
        }
        else
        {
            cJSON_Delete(patch);
            return;
        }
    }

    if (value != NULL)
    {
        cJSON_AddItemToObject(patch, "value", cJSON_Duplicate(value, 1));
    }
    cJSON_AddItemToArray(patches, patch);
}

void create_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive);

static void create_patches_for_array(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive)
{
    size_t index = 0;
    cJSON *from_child = from->child;
    cJSON *to_child = to->child;
    char *new_path = (char *)malloc(strlen((const char*)path) + 22);
    if (new_path == NULL)
    {
        return;
    }

    for (index = 0; (from_child != NULL) && (to_child != NULL); index++)
    {
        if (index > ULONG_MAX)
        {
            free(new_path);
            return;
        }
        sprintf(new_path, "%s/%lu", path, (unsigned long)index);
        create_patches(patches, (unsigned char *)new_path, from_child, to_child, case_sensitive);
        from_child = from_child->next;
        to_child = to_child->next;
    }

    for (; from_child != NULL; from_child = from_child->next)
    {
        if (index > ULONG_MAX)
        {
            free(new_path);
            return;
        }
        sprintf(new_path, "%lu", (unsigned long)index);
        compose_patch(patches, (const unsigned char*)"remove", path, (unsigned char *)new_path, NULL);
    }

    for (; to_child != NULL; to_child = to_child->next)
    {
        compose_patch(patches, (const unsigned char*)"add", path, (const unsigned char*)"-", to_child);
    }

    free(new_path);
}

static void create_patches_for_object(cJSON * const patches, const unsigned char * const path, cJSON * from, cJSON * to, const cJSON_bool case_sensitive)
{
    cJSON *from_child = NULL;
    cJSON *to_child = NULL;
    int key_comparison = 0;

    from->child = sort_list(from->child, case_sensitive);
    to->child = sort_list(to->child, case_sensitive);

    from_child = from->child;
    to_child = to->child;

    while ((from_child != NULL) || (to_child != NULL))
    {
        if (from_child == NULL)
        {
            key_comparison = 1;
        }
        else if (to_child == NULL)
        {
            key_comparison = -1;
        }
        else
        {
            key_comparison = compare_item_strings(from_child, to_child, case_sensitive);
        }

        if (key_comparison == 0)
        {
            unsigned char *new_path = create_patched_path(path, (unsigned char *)from_child->string);
            if (new_path != NULL)
            {
                create_patches(patches, new_path, from_child, to_child, case_sensitive);
                free(new_path);
            }
            from_child = from_child->next;
            to_child = to_child->next;
        }
        else if (key_comparison < 0)
        {
            compose_patch(patches, (const unsigned char*)"remove", path, (unsigned char*)from_child->string, NULL);
            from_child = from_child->next;
        }
        else
        {
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
            break;
    }
}

char * read_file_to_buffer(const char *filepath)
{
    FILE *fp = NULL;
    char *buffer = NULL;
    long file_size = 0;
    size_t read_size = 0;

    fp = fopen(filepath, "rb");
    if (fp == NULL)
    {
        return NULL;
    }

    if (fseek(fp, 0L, SEEK_END) != 0)
    {
        fclose(fp);
        return NULL;
    }

    file_size = ftell(fp);
    if (file_size == -1L)
    {
        fclose(fp);
        return NULL;
    }

    rewind(fp);

    buffer = (char *)calloc(file_size + 1, sizeof(char));
    if (buffer == NULL)
    {
        fclose(fp);
        return NULL;
    }

    read_size = fread(buffer, sizeof(char), file_size, fp);
    if (read_size != (size_t)file_size)
    {
        if (ferror(fp))
        {
            fclose(fp);
            free(buffer);
            return NULL;
        }
    }

    buffer[read_size] = '\0';

    fclose(fp);

    return buffer;
}


int main(void)
{
    const char* from_filepath = "from.json";
    const char* to_filepath = "to.json";

    char *from_buffer = read_file_to_buffer(from_filepath);
    char *to_buffer = read_file_to_buffer(to_filepath);

    cJSON *from_json = NULL;
    cJSON *to_json = NULL;
    cJSON *patches_array = NULL;
    char *patches_string = NULL;

    if ((from_buffer == NULL) || (to_buffer == NULL))
    {
        free(from_buffer);
        free(to_buffer);
        return 1;
    }

    from_json = cJSON_Parse(from_buffer);
    to_json = cJSON_Parse(to_buffer);
    patches_array = cJSON_CreateArray();

    if ((from_json != NULL) && (to_json != NULL) && (patches_array != NULL))
    {
        create_patches(patches_array, (const unsigned char *)"", from_json, to_json, cJSON_True);
        patches_string = cJSON_Print(patches_array);
        if (patches_string != NULL)
        {
            printf("Generated Patches:\n%s\n", patches_string);
            free(patches_string);
        }
    }

    cJSON_Delete(from_json);
    cJSON_Delete(to_json);
    cJSON_Delete(patches_array);

    free(from_buffer);
    free(to_buffer);

    return 0;
}
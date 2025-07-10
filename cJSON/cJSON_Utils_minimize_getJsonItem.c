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

static unsigned char* cJSONUtils_strdup(const unsigned char* const string)
{
    size_t length = 0;
    unsigned char *copy = NULL;

    length = strlen((const char*)string) + sizeof("");
    copy = (unsigned char*) malloc(length);
    if (copy == NULL)
    {
        return NULL;
    }
    memcpy(copy, string, length);

    return copy;
}

/* calculate the length of a string if encoded as JSON pointer with ~0 and ~1 escape sequences */
static size_t pointer_encoded_length(const unsigned char *string)
{
    size_t length;
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
    unsigned char *target_pointer, *full_pointer;
    size_t child_index = 0;
    cJSON *current_child = 0;

    if ((object == NULL) || (target == NULL))
    {
        return NULL;
    }

    if (object == target)
    {
        /* found */
        return (char*)cJSONUtils_strdup((const unsigned char*)"");
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
                full_pointer = (unsigned char*)malloc(strlen((char*)target_pointer) + 20 + sizeof("/"));
                /* check if conversion to unsigned long is valid
                 * This should be eliminated at compile time by dead code elimination
                 * if size_t is an alias of unsigned long, or if it is bigger */
                if (child_index > ULONG_MAX)
                {
                    free(target_pointer);
                    free(full_pointer);
                    return NULL;
                }
                sprintf((char*)full_pointer, "/%lu%s", (unsigned long)child_index, target_pointer); /* /<array_index><path> */
                free(target_pointer);

                return (char*)full_pointer;
            }

            if (cJSON_IsObject(object))
            {
                full_pointer = (unsigned char*)malloc(strlen((char*)target_pointer) + pointer_encoded_length((unsigned char*)current_child->string) + 2);
                full_pointer[0] = '/';
                encode_string_as_pointer(full_pointer + 1, (unsigned char*)current_child->string);
                strcat((char*)full_pointer, (char*)target_pointer);
                free(target_pointer);

                return (char*)full_pointer;
            }

            /* reached leaf of the tree, found nothing */
            free(target_pointer);
            return NULL;
        }
    }

    /* not found */
    return NULL;
}

int main() {
    printf("--- Testing cJSONUtils_FindPointerFromObjectTo ---\n");
    const char *json_string = "{\n"
                              "  \"name\": \"John Doe\",\n"
                              "  \"age\": 30,\n"
                              "  \"address\": {\n"
                              "    \"street\": \"123 Main St\",\n"
                              "    \"city\": \"Anytown\"\n"
                              "  },\n"
                              "  \"phones\": [\n"
                              "    { \"type\": \"home\", \"number\": \"555-1234\" },\n"
                              "    { \"type\": \"work\", \"number\": \"555-5678\" }\n"
                              "  ]\n"
                              "}";

    cJSON *root = cJSON_Parse(json_string);

    cJSON *address_object = cJSON_GetObjectItemCaseSensitive(root, "address");
    char *pointer_to_address = cJSONUtils_FindPointerFromObjectTo(root, address_object);
    printf("Pointer to 'address': %s\n", pointer_to_address);
    free(pointer_to_address);

    cJSON *city_item = cJSON_GetObjectItemCaseSensitive(address_object, "city");
    char *pointer_to_city = cJSONUtils_FindPointerFromObjectTo(root, city_item);
    printf("Pointer to 'city' (from root): %s\n", pointer_to_city);
    free(pointer_to_city);

    cJSON *phones_array = cJSON_GetObjectItemCaseSensitive(root, "phones");
    cJSON *second_phone_object = cJSON_GetArrayItem(phones_array, 1);
    cJSON *work_number_item = cJSON_GetObjectItemCaseSensitive(second_phone_object, "number");
    char *pointer_to_work_number = cJSONUtils_FindPointerFromObjectTo(root, work_number_item);
    printf("Pointer to 'work number': %s\n", pointer_to_work_number);
    free(pointer_to_work_number);

    cJSON_Delete(root);

    return 0;
}

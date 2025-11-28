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

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include "cJSON.h"
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

typedef struct {
    const unsigned char *content;
    size_t length;
    size_t offset;
    size_t depth;
} parse_buffer;

static cJSON *create_new_item() {
    cJSON *node;

    node = (cJSON *) malloc(sizeof(cJSON));
    if (node) {
        memset(node, '\0', sizeof(cJSON));
    }

    return node;
}

static cJSON_bool can_read(const parse_buffer *const buffer, size_t size) {
    if (buffer == NULL) {
        return false;
    }

    return buffer->offset + size <= buffer->length;
}

/* check if the buffer can be accessed at the given index (starting with 0) */
static cJSON_bool can_access_at_index(const parse_buffer *const buffer, size_t index) {
    if (buffer == NULL) {
        return false;
    }

    return buffer->offset + index < buffer->length;
}

static const unsigned char *buffer_at_offset(const parse_buffer *const buffer) {
    return buffer->content + buffer->offset;
}

static cJSON_bool parse_number(cJSON *const item, parse_buffer *const input_buffer) {
    double number = 0;
    unsigned char *after_end = NULL;
    unsigned char number_c_string[64];
    unsigned char c;
    size_t i = 0;

    if ((input_buffer == NULL) || (input_buffer->content == NULL)) {
        return false;
    }

    /* copy the number into a temporary buffer and replace '.' with the decimal point
     * of the current locale (for strtod)
     * This also takes care of '\0' not necessarily being available for marking the end of the input */
    for (i = 0; (i < (sizeof(number_c_string) - 1)) && can_access_at_index(input_buffer, i); i++) {
        c = buffer_at_offset(input_buffer)[i];

        if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == 'e' || c == 'E') {
            number_c_string[i] = c;
        } else if (c == '.') {
            number_c_string[i] = '.';
        } else {
            break;
        }
    }
    number_c_string[i] = '\0';

    number = strtod((const char *) number_c_string, (char **) &after_end);
    if (number_c_string == after_end) {
        return false; /* parse_error */
    }

    item->valuedouble = number;

    /* use saturation in case of overflow */
    if (number >= INT_MAX) {
        item->valueint = INT_MAX;
    } else if (number <= (double) INT_MIN) {
        item->valueint = INT_MIN;
    } else {
        item->valueint = (int) number;
    }

    item->type = cJSON_Number;

    input_buffer->offset += (size_t) (after_end - number_c_string);
    return true;
}

/* Parse the input text into an unescaped cinput, and populate item. */
static cJSON_bool parse_string(cJSON *const item, parse_buffer *const input_buffer) {
    const unsigned char *input_pointer = buffer_at_offset(input_buffer) + 1;
    const unsigned char *input_end = buffer_at_offset(input_buffer) + 1;
    unsigned char *output_pointer = NULL;
    unsigned char *output = NULL;
    unsigned char sequence_length;
    cJSON_bool isSuccess = true;
    /* calculate approximate size of the output (overestimate) */
    size_t allocation_length = 0;
    size_t skipped_bytes = 0;
    while (((size_t) (input_end - input_buffer->content) < input_buffer->length) && (*input_end != '\"')) {
        /* is escape sequence */
        if (input_end[0] == '\\') {
            if ((size_t) (input_end + 1 - input_buffer->content) >= input_buffer->length) {
                /* prevent buffer overflow when last input character is a backslash */
                return false;
            }
            skipped_bytes++;
            input_end++;
        }
        input_end++;
    }

    /* This is at most how much we need for the output */
    allocation_length = (size_t) (input_end - buffer_at_offset(input_buffer)) - skipped_bytes;
    output = (unsigned char *) malloc(allocation_length + sizeof(""));
    if (output == NULL) {
        return false; /* allocation failure */
    }

    output_pointer = output;
    /* loop through the string literal */
    while (input_pointer < input_end) {
        if (*input_pointer != '\\') {
            *output_pointer++ = *input_pointer++;
        }
        /* escape sequence */
        else {
            sequence_length = 2;

            switch (input_pointer[1]) {
                case 'b':
                    *output_pointer++ = '\b';
                    break;
                case 'f':
                    *output_pointer++ = '\f';
                    break;
                case 'n':
                    *output_pointer++ = '\n';
                    break;
                case 'r':
                    *output_pointer++ = '\r';
                    break;
                case 't':
                    *output_pointer++ = '\t';
                    break;
                case '\"':
                case '\\':
                case '/':
                    *output_pointer++ = input_pointer[1];
                    break;

                default:
                    isSuccess = false;
                    break;
            }

            if (!isSuccess) {
                break;
            }

            input_pointer += sequence_length;
        }
    }

    if (isSuccess) {
        /* zero terminate the output */
        *output_pointer = '\0';

        item->type = cJSON_String;
        item->valuestring = (char *) output;

        input_buffer->offset = (size_t) (input_end - input_buffer->content);
        input_buffer->offset++;

        return true;
    } else {
        free(output);
        output = NULL;

        if (input_pointer != NULL) {
            input_buffer->offset = (size_t) (input_pointer - input_buffer->content);
        }

        return false;
    }
}

/* Predeclare these prototypes. */
static cJSON_bool parse_value(cJSON *const item, parse_buffer *const input_buffer);

static cJSON_bool parse_array(cJSON *const item, parse_buffer *const input_buffer);

static cJSON_bool parse_object(cJSON *const item, parse_buffer *const input_buffer);

/* skip the UTF-8 BOM (byte order mark) if it is at the beginning of a buffer */
static parse_buffer *skip_utf8_bom(parse_buffer *const buffer) {
    if ((buffer == NULL) || (buffer->content == NULL) || (buffer->offset != 0)) {
        return NULL;
    }

    if (can_access_at_index(buffer, 4) && (strncmp((const char *) buffer_at_offset(buffer), "\xEF\xBB\xBF", 3) == 0)) {
        buffer->offset += 3;
    }

    return buffer;
}

/* Parse an object - create a new root, and populate. */
CJSON_PUBLIC(cJSON *) json_parse(const char *value, size_t buffer_length) {
    parse_buffer buffer = {0, 0, 0, 0};
    parse_buffer *buffer_skip_utf8_bom;
    parse_buffer *buffer_skip_whitespace;
    cJSON *item = NULL;

    if (value == NULL || 0 == buffer_length) {
        return NULL;
    }

    buffer.content = (const unsigned char *) value;
    buffer.length = buffer_length;
    buffer.offset = 0;

    item = create_new_item();
    if (item == NULL) {
        return NULL;
    }

    buffer_skip_utf8_bom = skip_utf8_bom(&buffer);

    if (buffer_skip_utf8_bom == NULL || buffer_skip_utf8_bom->content == NULL) {
        buffer_skip_whitespace = NULL;
    } else {
        if (can_access_at_index(buffer_skip_utf8_bom, 0)) {
            while (can_access_at_index(buffer_skip_utf8_bom, 0) && (buffer_at_offset(buffer_skip_utf8_bom)[0] <= 32)) {
                buffer_skip_utf8_bom->offset++;
            }

            if (buffer_skip_utf8_bom->offset == buffer_skip_utf8_bom->length) {
                buffer_skip_utf8_bom->offset--;
            }
        }

        buffer_skip_whitespace = buffer_skip_utf8_bom;
    }

    if (!parse_value(item, buffer_skip_whitespace)) {
        cJSON_Delete(item);
        return NULL;
    }

    return item;
}

/* Parser core - when encountering text, process appropriately. */
static cJSON_bool parse_value(cJSON *const item, parse_buffer *const input_buffer) {
    /* parse the different types of values */
    /* null */
    if (can_read(input_buffer, 4) && (strncmp((const char *) buffer_at_offset(input_buffer), "null", 4) == 0)) {
        item->type = cJSON_NULL;
        input_buffer->offset += 4;
        return true;
    }
    /* false */
    if (can_read(input_buffer, 5) && (strncmp((const char *) buffer_at_offset(input_buffer), "false", 5) == 0)) {
        item->type = cJSON_False;
        input_buffer->offset += 5;
        return true;
    }
    /* true */
    if (can_read(input_buffer, 4) && (strncmp((const char *) buffer_at_offset(input_buffer), "true", 4) == 0)) {
        item->type = cJSON_True;
        item->valueint = 1;
        input_buffer->offset += 4;
        return true;
    }
    /* string */
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '\"')) {
        return parse_string(item, input_buffer);
    }
    /* number */
    if (can_access_at_index(input_buffer, 0) && ((buffer_at_offset(input_buffer)[0] == '-') || (
                                                     (buffer_at_offset(input_buffer)[0] >= '0') && (buffer_at_offset(
                                                         input_buffer)[0] <= '9')))) {
        return parse_number(item, input_buffer);
    }
    /* array */
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '[')) {
        return parse_array(item, input_buffer);
    }
    /* object */
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '{')) {
        return parse_object(item, input_buffer);
    }

    return false;
}

/* Build an array from input text. */
static cJSON_bool parse_array(cJSON *const item, parse_buffer *const input_buffer) {
    cJSON *head = NULL; /* head of the linked list */
    cJSON *current_item = NULL;
    cJSON *new_item = NULL;
    cJSON_bool isSuccess = true;

    input_buffer->depth++;
    input_buffer->offset++;

    if (can_access_at_index(input_buffer, 0)) {
        while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] <= 32)) {
            input_buffer->offset++;
        }

        if (input_buffer->offset == input_buffer->length) {
            input_buffer->offset--;
        }
    }

    /* step back to character in front of the first element */
    input_buffer->offset--;
    /* loop through the comma separated array elements */
    do {
        /* allocate next item */
        new_item = create_new_item();
        if (new_item == NULL) {
            isSuccess = false;
            break;
        }

        /* attach next item to list */
        if (head == NULL) {
            /* start the linked list */
            current_item = head = new_item;
        } else {
            /* add to the end and advance */
            current_item->next = new_item;
            new_item->prev = current_item;
            current_item = new_item;
        }

        /* parse next value */
        input_buffer->offset++;
        if (can_access_at_index(input_buffer, 0)) {
            while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] <= 32)) {
                input_buffer->offset++;
            }

            if (input_buffer->offset == input_buffer->length) {
                input_buffer->offset--;
            }
        }

        parse_value(current_item, input_buffer);

        if (can_access_at_index(input_buffer, 0)) {
            while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] <= 32)) {
                input_buffer->offset++;
            }

            if (input_buffer->offset == input_buffer->length) {
                input_buffer->offset--;
            }
        }
    } while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

    if (isSuccess) {
        input_buffer->depth--;
        head->prev = current_item;
        item->type = cJSON_Array;
        item->child = head;
        input_buffer->offset++;

        return true;
    } else {
        cJSON_Delete(head);

        return false;
    }
}

/* Build an object from the text. */
static cJSON_bool parse_object(cJSON *const item, parse_buffer *const input_buffer) {
    cJSON *head = NULL; /* linked list head */
    cJSON *current_item = NULL;
    cJSON *new_item = NULL;
    cJSON_bool isSuccess = true;

    input_buffer->depth++;
    input_buffer->offset++;
    if (can_access_at_index(input_buffer, 0)) {
        while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] <= 32)) {
            input_buffer->offset++;
        }

        if (input_buffer->offset == input_buffer->length) {
            input_buffer->offset--;
        }
    }

    /* step back to character in front of the first element */
    input_buffer->offset--;
    /* loop through the comma separated array elements */
    do {
        /* allocate next item */
        new_item = create_new_item();
        if (new_item == NULL) {
            isSuccess = false;
            break;
        }

        /* attach next item to list */
        if (head == NULL) {
            /* start the linked list */
            current_item = head = new_item;
        } else {
            /* add to the end and advance */
            current_item->next = new_item;
            new_item->prev = current_item;
            current_item = new_item;
        }

        if (!can_access_at_index(input_buffer, 1)) {
            isSuccess = false;
            break;
        }

        /* parse the name of the child */
        input_buffer->offset++;

        if (can_access_at_index(input_buffer, 0)) {
            while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] <= 32)) {
                input_buffer->offset++;
            }

            if (input_buffer->offset == input_buffer->length) {
                input_buffer->offset--;
            }
        }

        if (!parse_string(current_item, input_buffer)) {
            isSuccess = false;
            break;
        }

        if (can_access_at_index(input_buffer, 0)) {
            while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] <= 32)) {
                input_buffer->offset++;
            }

            if (input_buffer->offset == input_buffer->length) {
                input_buffer->offset--;
            }
        }

        /* swap valuestring and string, because we parsed the name */
        current_item->string = current_item->valuestring;
        current_item->valuestring = NULL;

        if (!can_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != ':')) {
            isSuccess = false;
            break;
        }

        /* parse the value */
        input_buffer->offset++;

        if (can_access_at_index(input_buffer, 0)) {
            while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] <= 32)) {
                input_buffer->offset++;
            }

            if (input_buffer->offset == input_buffer->length) {
                input_buffer->offset--;
            }
        }

        if (!parse_value(current_item, input_buffer)) {
            isSuccess = false;
            break;
        }

        if (can_access_at_index(input_buffer, 0)) {
            while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] <= 32)) {
                input_buffer->offset++;
            }

            if (input_buffer->offset == input_buffer->length) {
                input_buffer->offset--;
            }
        }
    } while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

    if (isSuccess) {
        input_buffer->depth--;

        if (head != NULL) {
            head->prev = current_item;
        }

        item->type = cJSON_Object;
        item->child = head;

        input_buffer->offset++;
        return true;
    } else {
        cJSON_Delete(head);

        return false;
    }
}

cJSON *load_json_file(const char *filepath) {
    char *buffer;
    cJSON *json;
    FILE *fp;
    long file_size;
    size_t read_size;

    fp = fopen(filepath, "r");
    if (fp == NULL) {
        return NULL;
    }

    if (fseek(fp, 0L, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }

    file_size = ftell(fp);
    if (file_size == -1L) {
        fclose(fp);
        return NULL;
    }

    rewind(fp);

    buffer = (char *) calloc(file_size + 1, sizeof(char));
    if (buffer == NULL) {
        fclose(fp);
        return NULL;
    }

    read_size = fread(buffer, sizeof(char), file_size, fp);
    if (read_size != (size_t) file_size) {
        if (!feof(fp) && ferror(fp)) {
            fclose(fp);
            free(buffer);
            return NULL;
        }
    }

    buffer[read_size] = '\0';

    fclose(fp);

    json = json_parse(buffer, strlen(buffer) + sizeof(""));
    free(buffer);

    return json;
}

/* string comparison which doesn't consider NULL pointers equal */
static int compare_strings(const unsigned char *string1, const unsigned char *string2, const cJSON_bool case_sensitive) {
    if ((string1 == NULL) || (string2 == NULL)) {
        return 1;
    }

    if (string1 == string2) {
        return 0;
    }

    if (case_sensitive) {
        return strcmp((const char *) string1, (const char *) string2);
    }

    for (; tolower(*string1) == tolower(*string2); (void) string1++, string2++) {
        if (*string1 == '\0') {
            return 0;
        }
    }

    return tolower(*string1) - tolower(*string2);
}

/* sort lists using mergesort */
static cJSON *sort_list(cJSON *list, const cJSON_bool case_sensitive) {
    cJSON *smaller;
    cJSON *first = list;
    cJSON *second = list;
    cJSON *current_item = list;
    cJSON *result = list;
    cJSON *result_tail = NULL;
    int compare_strings;
    const unsigned char *string1;
    const unsigned char *string2;

    if ((list == NULL) || (list->next == NULL)) {
        /* One entry is sorted already. */
        return result;
    }

    string1 = (unsigned char *) current_item->string;
    string2 = (unsigned char *) current_item->next->string;

    if ((string1 == NULL) || (string2 == NULL)) {
        compare_strings = 1;
    } else if (string1 == string2) {
        compare_strings = 0;
    } else {
        if (case_sensitive) {
            compare_strings = strcmp((const char *) string1, (const char *) string2);
        } else {
            while (tolower(*string1) == tolower(*string2)) {
                if (*string1 == '\0') {
                    break;
                }
                string1++;
                string2++;
            }
            compare_strings = tolower(*string1) - tolower(*string2);
        }
    }

    while ((current_item != NULL) && (current_item->next != NULL) && (compare_strings < 0)) {
        /* Test for list sorted. */
        current_item = current_item->next;
    }
    if ((current_item == NULL) || (current_item->next == NULL)) {
        /* Leave sorted lists unmodified. */
        return result;
    }

    /* reset pointer to the beginning */
    current_item = list;
    while (current_item != NULL) {
        /* Walk two pointers to find the middle. */
        second = second->next;
        current_item = current_item->next;
        /* advances current_item two steps at a time */
        if (current_item != NULL) {
            current_item = current_item->next;
        }
    }
    if ((second != NULL) && (second->prev != NULL)) {
        /* Split the lists */
        second->prev->next = NULL;
        second->prev = NULL;
    }

    /* Recursively sort the sub-lists. */
    first = sort_list(first, case_sensitive);
    second = sort_list(second, case_sensitive);
    result = NULL;

    /* Merge the sub-lists */
    while ((first != NULL) && (second != NULL)) {
        smaller = NULL;
        string1 = (unsigned char *) first->string;
        string2 = (unsigned char *) second->string;

        if ((string1 == NULL) || (string2 == NULL)) {
            compare_strings = 1;
        } else if (string1 == string2) {
            compare_strings = 0;
        } else {
            if (case_sensitive) {
                compare_strings = strcmp((const char *) string1, (const char *) string2);
            } else {
                while (tolower(*string1) == tolower(*string2)) {
                    if (*string1 == '\0') {
                        break;
                    }
                    string1++;
                    string2++;
                }
                compare_strings = tolower(*string1) - tolower(*string2);
            }
        }

        if (compare_strings < 0) {
            smaller = first;
        } else {
            smaller = second;
        }

        if (result == NULL) {
            /* start merged list with the smaller element */
            result_tail = smaller;
            result = smaller;
        } else {
            /* add smaller element to the list */
            result_tail->next = smaller;
            smaller->prev = result_tail;
            result_tail = smaller;
        }

        if (first == smaller) {
            first = first->next;
        } else {
            second = second->next;
        }
    }

    if (first != NULL) {
        /* Append rest of first list. */
        if (result == NULL) {
            return first;
        }
        result_tail->next = first;
        first->prev = result_tail;
    }
    if (second != NULL) {
        /* Append rest of second list */
        if (result == NULL) {
            return second;
        }
        result_tail->next = second;
        second->prev = result_tail;
    }

    return result;
}

int main() {
    cJSON *example1;

    example1 = load_json_file("example1.json");

    cJSON *key1 = example1->child;
    cJSON *key2 = key1->next;
    cJSON *key3 = key2->next;
    cJSON *key4 = key3->child;
    cJSON *key5 = key3->next;
    cJSON *key5Item1 = key5->child;
    cJSON *key5Item2 = key5Item1->next;

    printf("{\n");
    printf("  \"%s\": %f,\n", key1->string, key1->valuedouble);
    printf("  \"%s\": \"%s\",\n", key2->string, key2->valuestring);
    printf("  \"%s\": {\n", key3->string);
    printf("    \"%s\": %d\n", key4->string, key4->valueint);
    printf("  },\n");
    printf("  \"%s\": [\n", key5->string);
    printf("    %d,\n", key5Item1->valueint);
    printf("    \"%s\",\n", key5Item2->valuestring);
    printf("  ]\n");
    printf("}\n");

    cJSON_Delete(example1);

    return 0;
}

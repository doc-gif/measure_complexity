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

        if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == 'e' || c == 'E' || c == '.') {
            number_c_string[i] = c;
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
    size_t allocation_length = 0;
    size_t skipped_bytes = 0;

    if (!can_access_at_index(input_buffer, 0) || buffer_at_offset(input_buffer)[0] != '\"') {
        return false;
    }

    /* calculate approximate size of the output (overestimate) */
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

/* Utility to jump whitespace and cr/lf */
static parse_buffer *buffer_skip_whitespace(parse_buffer *const buffer) {
    if ((buffer == NULL) || (buffer->content == NULL)) {
        return NULL;
    }

    if (!can_access_at_index(buffer, 0)) {
        return buffer;
    }

    while (can_access_at_index(buffer, 0) && isspace(buffer_at_offset(buffer)[0])) {
        buffer->offset++;
    }

    if (buffer->offset == buffer->length) {
        buffer->offset--;
    }

    return buffer;
}

/* Predeclare these prototypes. */
static cJSON_bool parse_value(cJSON *const item, parse_buffer *const input_buffer);

/* Build an array from input text. */
static cJSON_bool parse_array(cJSON *const item, parse_buffer *const input_buffer) {
    cJSON *head = NULL; /* head of the linked list */
    cJSON *current_item = NULL;
    cJSON *new_item = NULL;
    cJSON_bool isSuccess = true;

    input_buffer->offset++;
    buffer_skip_whitespace(input_buffer);

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
            head = new_item;
            current_item = new_item;
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

        /* parse next value */
        input_buffer->offset++;
        buffer_skip_whitespace(input_buffer);
        if (!parse_value(current_item, input_buffer)) {
            isSuccess = false;
            break;
        }
        buffer_skip_whitespace(input_buffer);
    } while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

    if (isSuccess) {
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

    input_buffer->offset++;
    buffer_skip_whitespace(input_buffer);

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
            head = new_item;
            current_item = new_item;
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
        buffer_skip_whitespace(input_buffer);
        if (!parse_string(current_item, input_buffer)) {
            isSuccess = false;
            break;
        }
        buffer_skip_whitespace(input_buffer);

        /* swap valuestring and string, because we parsed the name */
        current_item->string = current_item->valuestring;
        current_item->valuestring = NULL;

        if (!can_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != ':')) {
            isSuccess = false;
            break;
        }

        /* parse the value */
        input_buffer->offset++;
        buffer_skip_whitespace(input_buffer);
        if (!parse_value(current_item, input_buffer)) {
            isSuccess = false;
            break;
        }
        buffer_skip_whitespace(input_buffer);
    } while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

    if (isSuccess) {
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
    if (can_access_at_index(input_buffer, 0) && ((buffer_at_offset(input_buffer)[0] == '-') || ((buffer_at_offset(input_buffer)[0] >= '0') && (buffer_at_offset(input_buffer)[0] <= '9')))) {
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

/* Parse an object - create a new root, and populate. */
cJSON * json_parse(const char *value, size_t buffer_length) {
    parse_buffer buffer = {0, 0, 0};
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

    if (!parse_value(item, buffer_skip_whitespace(&buffer))) {
        cJSON_Delete(item);
        return NULL;
    }

    return item;
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

    if ((list == NULL) || (list->next == NULL)) {
        /* One entry is sorted already. */
        return result;
    }

    while ((current_item != NULL) && (current_item->next != NULL) && (compare_strings((unsigned char *) current_item->string,(unsigned char *) current_item->next->string,case_sensitive) < 0)) {
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
        if (compare_strings((unsigned char *) first->string, (unsigned char *) second->string, case_sensitive) < 0) {
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
    cJSON *json1, *json2, *json3, *json4, *json5, *json6, *json7, *json8;

    json1 = load_json_file("example1.json");

    json2 = load_json_file("example2.json");

    json3 = load_json_file("example3.json");

    json4 = load_json_file("example4.json");

    json5 = load_json_file("example5.json");

    json6 = load_json_file("example6.json");
    json6->child = sort_list(json6->child, cJSON_False);

    json7 = load_json_file("example7.json");
    printf("%s", json7->valuestring);

    json8 = load_json_file("example8.json");

    cJSON_Delete(json1);
    cJSON_Delete(json2);
    cJSON_Delete(json3);
    cJSON_Delete(json4);
    cJSON_Delete(json5);
    cJSON_Delete(json6);
    cJSON_Delete(json7);
    cJSON_Delete(json8);

    return 0;
}

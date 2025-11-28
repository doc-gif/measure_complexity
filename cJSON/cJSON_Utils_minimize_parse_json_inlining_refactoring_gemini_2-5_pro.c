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
    cJSON *node = (cJSON *) malloc(sizeof(cJSON));
    if (node) {
        memset(node, '\0', sizeof(cJSON));
    }
    return node;
}

static cJSON_bool can_read(const parse_buffer *const buffer, size_t size) {
    return buffer && (buffer->offset + size <= buffer->length);
}

static cJSON_bool can_access_at_index(const parse_buffer *const buffer, size_t index) {
    return buffer && (buffer->offset + index < buffer->length);
}

static const unsigned char *buffer_at_offset(const parse_buffer *const buffer) {
    return buffer->content + buffer->offset;
}

static void skip_whitespace(parse_buffer *const buffer) {
    if (buffer == NULL || !can_access_at_index(buffer, 0)) {
        return;
    }
    while (can_access_at_index(buffer, 0) && (buffer_at_offset(buffer)[0] <= 32)) {
        buffer->offset++;
    }
    if (buffer->offset == buffer->length) {
        buffer->offset--;
    }
}

static void append_item(cJSON **head, cJSON **current, cJSON *new_item) {
    if (*head == NULL) {
        *head = *current = new_item;
    } else {
        (*current)->next = new_item;
        new_item->prev = *current;
        *current = new_item;
    }
}

static cJSON_bool parse_number(cJSON *const item, parse_buffer *const input_buffer) {
    double number = 0;
    unsigned char *after_end = NULL;
    unsigned char number_c_string[64];
    size_t i = 0;

    if (input_buffer == NULL || input_buffer->content == NULL) {
        return false;
    }

    for (i = 0; (i < sizeof(number_c_string) - 1) && can_access_at_index(input_buffer, i); i++) {
        unsigned char c = buffer_at_offset(input_buffer)[i];
        if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == 'e' || c == 'E' || c == '.') {
            number_c_string[i] = c;
        } else {
            break;
        }
    }
    number_c_string[i] = '\0';

    number = strtod((const char *) number_c_string, (char **) &after_end);
    if (number_c_string == after_end) {
        return false;
    }

    item->valuedouble = number;
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

static cJSON_bool parse_string(cJSON *const item, parse_buffer *const input_buffer) {
    const unsigned char *input_pointer = buffer_at_offset(input_buffer) + 1;
    const unsigned char *input_end = buffer_at_offset(input_buffer) + 1;
    unsigned char *output = NULL;
    unsigned char *output_pointer = NULL;
    size_t allocation_length = 0;
    size_t skipped_bytes = 0;

    while (((size_t) (input_end - input_buffer->content) < input_buffer->length) && (*input_end != '\"')) {
        if (input_end[0] == '\\') {
            if ((size_t) (input_end + 1 - input_buffer->content) >= input_buffer->length) {
                return false;
            }
            skipped_bytes++;
            input_end++;
        }
        input_end++;
    }

    allocation_length = (size_t) (input_end - buffer_at_offset(input_buffer)) - skipped_bytes;
    output = (unsigned char *) malloc(allocation_length + sizeof(""));
    if (output == NULL) {
        return false;
    }

    output_pointer = output;
    while (input_pointer < input_end) {
        if (*input_pointer != '\\') {
            *output_pointer++ = *input_pointer++;
        } else {
            input_pointer++;
            switch (*input_pointer) {
                case 'b': *output_pointer++ = '\b';
                    break;
                case 'f': *output_pointer++ = '\f';
                    break;
                case 'n': *output_pointer++ = '\n';
                    break;
                case 'r': *output_pointer++ = '\r';
                    break;
                case 't': *output_pointer++ = '\t';
                    break;
                case '\"':
                case '\\':
                case '/': *output_pointer++ = *input_pointer;
                    break;
                default:
                    free(output);
                    input_buffer->offset = (size_t) (input_pointer - input_buffer->content);
                    return false;
            }
            input_pointer++;
        }
    }

    *output_pointer = '\0';
    item->type = cJSON_String;
    item->valuestring = (char *) output;
    input_buffer->offset = (size_t) (input_end - input_buffer->content) + 1;
    return true;
}

static cJSON_bool parse_value(cJSON *const item, parse_buffer *const input_buffer);

static cJSON_bool parse_array(cJSON *const item, parse_buffer *const input_buffer);

static cJSON_bool parse_object(cJSON *const item, parse_buffer *const input_buffer);

static parse_buffer *skip_utf8_bom(parse_buffer *const buffer) {
    if (buffer && buffer->content && buffer->offset == 0 && can_access_at_index(buffer, 3) && (strncmp((const char *) buffer_at_offset(buffer), "\xEF\xBB\xBF", 3) == 0)) {
        buffer->offset += 3;
    }
    return buffer;
}

CJSON_PUBLIC(cJSON *) json_parse(const char *value, size_t buffer_length) {
    parse_buffer buffer = {0, 0, 0, 0};
    cJSON *item = NULL;

    if (value == NULL || buffer_length == 0) {
        return NULL;
    }

    buffer.content = (const unsigned char *) value;
    buffer.length = buffer_length;

    item = create_new_item();
    if (item == NULL) {
        return NULL;
    }

    parse_buffer *buffer_ptr = skip_utf8_bom(&buffer);
    skip_whitespace(buffer_ptr);

    if (!parse_value(item, buffer_ptr)) {
        cJSON_Delete(item);
        return NULL;
    }

    return item;
}

static cJSON_bool parse_value(cJSON *const item, parse_buffer *const input_buffer) {
    if (can_read(input_buffer, 4) && (strncmp((const char *) buffer_at_offset(input_buffer), "null", 4) == 0)) {
        item->type = cJSON_NULL;
        input_buffer->offset += 4;
        return true;
    }
    if (can_read(input_buffer, 5) && (strncmp((const char *) buffer_at_offset(input_buffer), "false", 5) == 0)) {
        item->type = cJSON_False;
        input_buffer->offset += 5;
        return true;
    }
    if (can_read(input_buffer, 4) && (strncmp((const char *) buffer_at_offset(input_buffer), "true", 4) == 0)) {
        item->type = cJSON_True;
        item->valueint = 1;
        input_buffer->offset += 4;
        return true;
    }
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '\"')) {
        return parse_string(item, input_buffer);
    }
    if (can_access_at_index(input_buffer, 0) && ((buffer_at_offset(input_buffer)[0] == '-') || ((buffer_at_offset(input_buffer)[0] >= '0') && (buffer_at_offset(input_buffer)[0] <= '9')))) {
        return parse_number(item, input_buffer);
    }
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '[')) {
        return parse_array(item, input_buffer);
    }
    if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '{')) {
        return parse_object(item, input_buffer);
    }
    return false;
}

static cJSON_bool parse_array(cJSON *const item, parse_buffer *const input_buffer) {
    cJSON *head = NULL;
    cJSON *current_item = NULL;

    input_buffer->depth++;
    input_buffer->offset++;
    skip_whitespace(input_buffer);

    input_buffer->offset--;
    do {
        cJSON *new_item = create_new_item();
        if (new_item == NULL) {
            cJSON_Delete(head);
            return false;
        }
        append_item(&head, &current_item, new_item);

        input_buffer->offset++;
        skip_whitespace(input_buffer);
        parse_value(current_item, input_buffer);
        skip_whitespace(input_buffer);
    } while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

    input_buffer->depth--;
    if (head) {
        head->prev = current_item;
    }
    item->type = cJSON_Array;
    item->child = head;
    input_buffer->offset++;
    return true;
}

static cJSON_bool parse_object(cJSON *const item, parse_buffer *const input_buffer) {
    cJSON *head = NULL;
    cJSON *current_item = NULL;
    cJSON_bool isSuccess = true;

    input_buffer->depth++;
    input_buffer->offset++;
    skip_whitespace(input_buffer);

    input_buffer->offset--;
    do {
        cJSON *new_item = create_new_item();
        if (new_item == NULL) {
            isSuccess = false;
            break;
        }
        append_item(&head, &current_item, new_item);

        input_buffer->offset++;
        skip_whitespace(input_buffer);
        if (!parse_string(current_item, input_buffer)) {
            isSuccess = false;
            break;
        }
        skip_whitespace(input_buffer);

        current_item->string = current_item->valuestring;
        current_item->valuestring = NULL;

        if (!can_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != ':')) {
            isSuccess = false;
            break;
        }

        input_buffer->offset++;
        skip_whitespace(input_buffer);
        if (!parse_value(current_item, input_buffer)) {
            isSuccess = false;
            break;
        }
        skip_whitespace(input_buffer);
    } while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

    input_buffer->depth--;
    if (isSuccess) {
        if (head != NULL) {
            head->prev = current_item;
        }
        item->type = cJSON_Object;
        item->child = head;
        input_buffer->offset++;
    } else {
        cJSON_Delete(head);
    }
    return isSuccess;
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

static int compare_strings(const unsigned char *string1, const unsigned char *string2, const cJSON_bool case_sensitive) {
    if (string1 == NULL || string2 == NULL) {
        return 1;
    }
    if (string1 == string2) {
        return 0;
    }

    if (case_sensitive) {
        return strcmp((const char *) string1, (const char *) string2);
    }

    while (tolower(*string1) == tolower(*string2)) {
        if (*string1 == '\0') {
            break;
        }
        string1++;
        string2++;
    }

    return tolower(*string1) - tolower(*string2);
}

static cJSON *sort_list(cJSON *list, const cJSON_bool case_sensitive) {
    if (list == NULL || list->next == NULL) {
        return list;
    }

    cJSON *current = list;
    while (current && current->next && (compare_strings((const unsigned char *) current->string, (const unsigned char *) current->next->string, case_sensitive) < 0)) {
        current = current->next;
    }
    if (current == NULL || current->next == NULL) {
        return list;
    }

    cJSON *first = list;
    cJSON *second = list;
    current = list;
    while (current != NULL) {
        second = second->next;
        current = current->next;
        if (current != NULL) {
            current = current->next;
        }
    }
    if (second != NULL && second->prev != NULL) {
        second->prev->next = NULL;
        second->prev = NULL;
    }

    first = sort_list(first, case_sensitive);
    second = sort_list(second, case_sensitive);

    cJSON *result = NULL;
    cJSON *result_tail = NULL;
    while (first != NULL && second != NULL) {
        cJSON *smaller = (compare_strings((const unsigned char *) first->string, (const unsigned char *) second->string, case_sensitive) < 0)
                             ? first
                             : second;
        if (result == NULL) {
            result = result_tail = smaller;
        } else {
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

    cJSON *rest = (first != NULL) ? first : second;
    if (result == NULL) {
        return rest;
    }
    result_tail->next = rest;
    if (rest != NULL) {
        rest->prev = result_tail;
    }
    return result;
}

int main() {
    cJSON *example1 = load_json_file("example1.json");
    if (example1 == NULL) {
        return 1;
    }

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

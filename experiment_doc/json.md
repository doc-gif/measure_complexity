# json

この資料では、外部ファイルで定義されているマクロ、型、関数を説明します。

## マクロ

### cJSON_False

```c
#define cJSON_False  (1 << 0)
```

JSON真偽値の偽を表現する値です。

**宣言・定義**

- 定義場所：`cJSON_Utils.h`

### cJSON_True

```c
#define cJSON_True  (1 << 1)
```

JSON真偽値の真を表現する値です。

**宣言・定義**

- 定義場所：`cJSON_Utils.h`

### cJSON_NULL

```c
#define cJSON_NULL  (1 << 2)
```

NULL値を表現する値です。

**宣言・定義**

- 定義場所：`cJSON_Utils.h`

### cJSON_Number

```c
#define cJSON_Number  (1 << 3)
```

JSON数値を表現する値です。

**宣言・定義**

- 定義場所：`cJSON_Utils.h`

### cJSON_String

```c
#define cJSON_String  (1 << 4)
```

JSON文字列を表現する値です。

**宣言・定義**

- 定義場所：`cJSON_Utils.h`

### cJSON_Array

```c
#define cJSON_Array  (1 << 5)
```

JSON配列を表現する値です。

**宣言・定義**

- 定義場所：`cJSON_Utils.h`

### cJSON_Object

```c
#define cJSON_Object  (1 << 7)
```

JSONオブジェクトを表現する値です。

**宣言・定義**

- 定義場所：`cJSON_Utils.h`

### INT_MAX

int型で表現できる値の最大値です。

**宣言・定義**

- 定義場所：`limits.h`

### INT_MIN

int型で表現できる値の最小値です。

**宣言・定義**

- 定義場所：`limits.h`

## 型

### cJSON

```c
typedef struct cJSON {
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;
    int type;
    char *valuestring;
    int valueint;
    double valuedouble;
    char *string;
} cJSON;
```

JSONデータを表現する型です。

**宣言・定義**

- 定義場所：`cJSON_Utils.h`

### cJSON_bool

```c
typedef int cJSON_bool;
```

真偽値を表現する型です。

**宣言・定義**

- 定義場所：`cJSON_Utils.h`

### FILE

ファイル入出力を制御する型です。ファイルのオープン、読み書き、クローズなどの操作に使用されます。

宣言・定義

- 宣言場所：`stdio.h`

### size_t

オブジェクトのサイズを表現するのに十分な大きさが保証した、符号なし整数型です。

宣言・定義

- 定義場所：`stdlib.h`

## 関数

### cJSON_Delete（cJSONオブジェクトのメモリ解放）

```c
void cJSON_Delete(cJSON *item);
```

引数`item` で指定したcJSONオブジェクトのメモリを解放します。

**宣言・定義**

- 宣言場所：`cJSON_Utils.h`

### calloc（メモリ領域の確保）

```c
void *calloc(size_t num, size_t size);
```

引数`size`で指定したバイト数のメモリ領域を、引数`num`で指定した個数だけ確保し、そのメモリ領域のポインタを返します。

- 引数：
    - `size`：確保するメモリ領域の１つ当たりのバイト数
    - `num`：確保するメモリ領域の個数
- 戻り値：
    - メモリ領域の確保に成功した場合：確保したメモリ領域のポインタ
    - メモリ領域の確保に失敗した場合：`NULL`

**宣言・定義**

- 宣言場所：`stdlib.h`

### fclose（ファイルクローズ）

```c
int fclose(FILE *stream);
```

引数`stream`で指定したファイルをクローズします。

- 引数：
    - `stream`：クローズするファイルのポインタ
- 戻り値：
    - クローズに成功した場合：`0`
    - クローズに失敗した場合：`EOF`

**宣言・定義**

- 宣言場所：`stdio.h`

### feof（ファイルのEOF判定）

```c
int feof(FILE *stream);
```

引数`stream`で指定したファイルが指す位置が、`EOF` であるかを判別し、その判別結果を返します。

- 引数：
    - `stream`：`EOF`判定をするファイルのポインタ
- 戻り値：
    - ファイルが指す位置が`EOF`である場合：`0` 以外の値
    - ファイルが指す位置が`EOF`でない場合：`0`

**宣言・定義**

- 宣言場所：`stdio.h`

### ferror（ファイルのエラー判定）

```c
int ferror(FILE *stream);
```

引数`stream`で指定したファイルで、エラーが発生しているかを判別し、その判別結果を返します。

- 引数：
    - `stream`：エラー判定をするファイルのポインタ
- 戻り値：
    - エラーが発生した場合：`0` 以外の値
    - エラーが発生していない場合：`0`

**宣言・定義**

- 宣言場所：`stdio.h`

### fopen（ファイルオープン）

```c
FILE *fopen(char *path, char *mode);
```

引数`path` で指定したファイルパスを、引数`mode` で指定したモードで、オープンします。

- 引数：
    - `path`：オープンするファイルパスを表す文字列
    - `mode`：ファイルをオープンする際のモード
        - `r`：テキストファイルを読み込み専用でオープンする。
- 戻り値：
    - オープンに成功した場合：オープンしたファイルのポインタ
    - オープンに失敗した場合：`NULL`

**宣言・定義**

- 宣言場所：`stdio.h`

### fread（ファイル読み取り）

```c
size_t fread(void *buffer, size_t size, size_t count, FILE *stream);
```

引数`stream`で指定したファイルから、引数`size`で指定したバイト数ごとに、引数`count` で指定した個数だけデータを読み取り、引数`buffer` で指定したバッファに格納します。

- 引数：
    - `buffer` ：データを格納するバッファのポインタ
    - `size` ：1つ当たりのデータのバイト数
    - `count` ：読み取るデータの個数
    - `stream` ：文字読み込みをするファイルのポインタ
- 戻り値：
    - 読み取ったデータの個数

**宣言・定義**

- 宣言場所：`stdio.h`

### free（メモリ解放）

```c
void free(void *ptr);

```

引数`ptr`で指定したメモリ領域を解放します。

- 引数：
    - `ptr`：解放するメモリ領域
- 戻り値：無し

**宣言・定義**

- 宣言場所：`stdlib.h`

### fseek（ファイルが指す位置の移動）

```c
void fseek(FILE *fp, long offset, int origin);

```

引数`fp` で指定したファイルが指す位置を、引数`origin` で指定した位置から引数`offset` で指定したバイト分移動した位置に設定します。

- 引数：
    - `fp`：ファイルが指す位置を移動させるファイルのポインタ
    - `offset` ：引数`origin` で指定した位置から移動するバイト数
    - `origin` ：移動の起点となる位置
        - `SEEK_END` ：ファイルの終端
- 戻り値：無し

**宣言・定義**

- 宣言場所：`stdio.h`

### ftell（ファイルが指す位置の取得）

```c
long fseek(FILE *fp);

```

引数`fp` で指定したファイルが指す位置を取得して返します。

- 引数：
    - `fp`：ファイルが指す位置を取得するファイルのポインタ
- 戻り値：
    - ファイルが指す位置の取得に成功した場合：ファイルが指す位置
    - ファイルが指す位置の取得に失敗した場合：`-1`

**宣言・定義**

- 宣言場所：`stdio.h`

### isspace（空白文字の判別）

```c
int isspace(int c);
```

引数cで指定した文字コードが、空白文字であるかを判別し、その判別結果を返します。

- 引数：
    - `c`：判別する文字
- 戻り値：
    - 空白文字である場合：`0`以外の値
    - 空白文字でない場合：`0`

**宣言・定義**

- 宣言場所：`ctype.h`

### malloc（メモリ確保）

```c
void *malloc(size_t size);
```

引数`size` で指定したバイト数のメモリ領域を確保し、そのメモリ領域のポインタを返します。

- 引数：
    - `size`：確保するメモリ領域のバイト数
- 戻り値：
    - メモリ領域の確保に成功した場合：確保したメモリ領域のポインタ
    - メモリ領域の確保に失敗した場合：`NULL`

**宣言・定義**

- 宣言場所：`stdlib.h`

### memset（メモリ領域の値の置き換え）

```c
void *memset(void *s, int c, size_t n);
```

引数`s` で指定したメモリ領域のうち、先頭から引数`n` で指定したバイト数分、引数`c` で指定した文字に置き換えます。

- 引数：
    - `s` ：メモリ領域のポインタ
    - `c` ：メモリ領域に置き換える文字
    - `n` ：メモリ領域を置き換えるバイト数
- 戻り値：
    - 引数`s` で指定したメモリ領域のポインタ

**宣言・定義**

- 宣言場所：`string.h`

### printf（標準出力）

```c
int printf(const char* str, ...);
```

引数`src`で指定した文字列を標準出力します。引数`src` で指定した文字列が書式を含む場合、その書式に第二引数以降で指定した引数の値を格納した文字列を標準出力します。

- 引数：
    - `str`：標準出力する文字列
    - `...`：引数`str`が含む書式に格納する引数（可変長引数）
- 戻り値：
    - 標準出力に成功した場合：標準出力した文字数
    - 標準出力に失敗した場合：負の数

**宣言・定義**

- 宣言場所：`stdio.h`

### rewind（ファイルが指す位置を先頭に移動）

```c
void rewind(FILE *fp);
```

引数`fp` で指定したファイルが指す位置を、ファイルの先頭に移動させます。

- 引数：
    - `str`：標準出力する文字列
    - `...`：引数`str`が含む書式に格納する引数（可変長引数）
- 戻り値：
    - 標準出力に成功した場合：標準出力した文字数
    - 標準出力に失敗した場合：負の数

**宣言・定義**

- 宣言場所：`stdio.h`

### strcmp（文字列比較）

```c
int strcmp(const char *s1, const char *s2);
```

引数`s1` で指定した文字列と引数`s2` で指定した文字列を比較し、その比較結果を返します。

- 引数：
    - `s1` ：比較する文字列
    - `s2` ：比較する文字列
- 戻り値：
    - 引数`s1` ,`s2` が等しい場合：`0`
    - 引数`s1` が引数`s2` より辞書順で大きい場合：正の数
    - 引数`s1` が引数`s2` より辞書順で小さい場合：負の数

**宣言・定義**

- 宣言場所：`string.h`

### strlen（文字列長の計算）

```c
size_t strlen(const char *str);
```

引数`str` で指定した文字列の先頭から終端文字`\0`までの長さを計算して返します。文字列長には、終端文字`\0` の長さは含まれません。

- 引数：
    - `str` ：文字列長を計算する文字列
- 戻り値：
    - 引数`str` で指定した文字列の文字列長

**宣言・定義**

- 宣言場所：`string.h`

### strncmp（文字列比較）

```c
int strncmp(const char *s1, const char *s2, size_t num);
```

引数`s1` で指定した文字列と引数`s2` で指定した文字列を、先頭から引数`num` で指定したバイト数分比較し、その比較結果を返します。

- 引数：
    - `s1` ：比較する文字列
    - `s2` ：比較する文字列
    - `n` ：文字列を比較するバイト数
- 戻り値：
    - 引数`s1` ,`s2` が等しい場合：`0`
    - 引数`s1` が引数`s2` より辞書順で大きい場合：正の数
    - 引数`s1` が引数`s2` より辞書順で小さい場合：負の数

**宣言・定義**

- 宣言場所：`string.h`

### strtod（文字列から浮動小数点数への変換）

```c
double strtod(const char *s, char **endp);
```

引数`s` で指定した文字列を、浮動小数点数に変換して返します。引数`endp` には、変換に成功した場合は`NULL` 、変換に失敗した場合は、変換に失敗した文字へのポインタを格納します。

- 引数：
    - `s` ：変換する文字列
    - `endp` ：変換失敗時の文字を格納するポインタ
- 戻り値：
    - 変換に成功した場合：引数`s`を変換した浮動小数点数
    - 変換に失敗した場合：`0`

**宣言・定義**

- 宣言場所：`stdlib.h`

### tolower（大文字を小文字に変換）

```c
int tolower(int c);
```

引数`c` で指定した文字を小文字に変換して返します。

- 引数：
    - `c` ：変換する文字
- 戻り値：
    - 変換に成功した場合：変換した文字
    - 変換に失敗した場合：引数`c` で指定した文字

**宣言・定義**

- 宣言場所：`ctype.h`

```c++
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

```

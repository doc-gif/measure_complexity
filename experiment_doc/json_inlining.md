### ファイル構成とインクルードについて

この資料で説明する構造体、マクロ、関数は、cJSONライブラリによって提供されています。ファイル構成と利用方法は、以下の共通ルールに基づいています。

1.  cJSON.h(ヘッダーファイル)
    -   外部のプログラムから利用するための宣言が記述されています。
    -   これには、cJSON構造体、cJSON_bool型、各種マクロ、そして公開されている関数のプロトタイプ宣言が含まれます。

2.  cJSON.c (ソースファイル)
    -   cJSON.hで宣言された関数の具体的な処理内容（定義）が記述されています。

3.  インクルード経路
    -   この資料で扱うソースコードは cJSON_Utils.h をインクルードしています。
    -   cJSON_Utils.h は内部で cJSON.h をインクルードしているため、ここで説明する全ての機能は cJSON_Utils.h をインクルードするだけで利用可能になります。

このルールに基づき、各項目での冗長な説明は省略します。

### 構造体・マクロ・関数の説明

以下は、ソースコード読解実験に使用するプログラムを補足するものです。プログラム本体には定義されていませんが、外部ファイルで利用されている構造体・マクロ・関数について説明します。

```c++
typedef struct cJSON
{
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
JSON形式データを表現する構造体です。
-   next：同じ階層にあるcJSONノード（配列の要素群、JSONオブジェクト群）のうち1つ次のcJSONノードのポインタが格納されます。
-   prev：同じ階層にあるcJSONノードのうち1つ前のcJSONノードのポインタが格納されます。
-	child：1つ下の階層に子ノードを持つ時に、そのcJSONノードのポインタが格納されます。
-	type：このcJSONノードの種類を示します。
-	valuestring：typeがcJSON_StringまたはcJSON_rawのときに文字列が格納されます。
-	valueint：typeがcJSON_Numberかつその値が整数の時に、数値が格納されます。
-	valuedouble：typeがcJSON_Numberかつその値が小数の時に、数値が格納されます。
-	string：このcJSONノード名の文字列が格納されます。

#### 宣言・定義
-   定義場所：cJSON.h
-   インクルード経路：この定義は cJSON.h にありますが、cJSON_Utils.h が内部で cJSON.h をインクルードしているため、cJSON_Utils.h をインクルードすることでも利用可能になります。

---

```c++
typedef int cJSON_bool;
```

JSON真偽値を扱うために使われる型です。

#### 宣言・定義
-   定義場所：cJSON.h

---

```c++
#define CJSON_PUBLIC(type) type
```

引数typeで指定された関数型を返します。

#### 宣言・定義
-   定義場所：cJSON.h

---

```c++
#define cJSON_Invalid (0) 
#define cJSON_False  (1 << 0) 
#define cJSON_True   (1 << 1) 
#define cJSON_NULL   (1 << 2) 
#define cJSON_Number (1 << 3) 
#define cJSON_String (1 << 4) 
#define cJSON_Array  (1 << 5) 
#define cJSON_Object (1 << 6) 
#define cJSON_Raw    (1 << 7)
```

cJSONノードの種類を識別するマクロです。
-	cJSON_Invalid	：無効な文字列（cJSONノードに変換できない）
-	cJSON_False	    ：JSON真偽値の偽（false）
-	cJSON_True	    ：JSON真偽値の真（true）
-	cJSON_NULL	    ：JSON null値
-	cJSON_Number	：JSON数値（整数や小数）
-	cJSON_String	：JSON文字列
-	cJSON_Array	    ：JSON配列
-	cJSON_Object	：JSONオブジェクト（キーとバリューのペアの集まり）
-	cJSON_Raw	    ：cJSONノードに変換されていない文字列

#### 宣言・定義
-   定義場所：cJSON.h

---

```c++
CJSON_PUBLIC(void) cJSON_Delete(cJSON *item);
```

引数itemで指定されたcJSONノードとそのメンバ、格納されているcJSONノードをメモリ上から再帰的に解放します。
- 入力（引数）：
    -	item：削除したいcJSONノードへのポインタ
- 出力（戻り値）：
    -	無し
 
#### 宣言・定義
-   宣言場所：cJSON.h
-   定義場所：cJSON.c

---

```c++
CJSON_PUBLIC(cJSON *) cJSON_CreateString(const char *string);
```
引数stringで指定された文字列を元に、新たにcJSONノードを作成し、その種類（type）をcJSON_Stringに設定します。
- 入力（引数）：
    -	string：「JSON文字列」として作成したいテキスト
- 出力（戻り値）：
    - cJSONノードの作成に成功した場合：新たに作成したcJSONノードへのポインタ
    - cJSONノードの作成に失敗した場合：NULL
 
#### 宣言・定義
-   宣言場所：cJSON.h
-   定義場所：cJSON.c

---

```c++
CJSON_PUBLIC(cJSON *) cJSON_CreateObject(void);
```

メモリ上に新たに空のcJSONノードを作成し、その種類（type）をcJSON_Objectに設定します。作成した時点では、キーとバリューのペアは含まれていません。
- 入力（引数）：
    -	無し
- 出力（戻り値）：
    - 	cJSONノードの作成に成功した場合：新たに作成したcJSONノードへのポインタ
    - 	cJSONノードの作成に失敗した場合：NULL
 
#### 宣言・定義
-   宣言場所：cJSON.h
-   定義場所：cJSON.c

---

```c++
CJSON_PUBLIC(cJSON *) cJSON_CreateArray(void );
```

新たにメモリ上にcJSONノードを作成し、その種類（type）をcJSON_Arrayに設定します。作成時点では、「JSON配列」にcJSONノードは含まれていません。
- 入力（引数）：
    - 	無し
- 出力（戻り値）：
    - 	cJSONノードの作成に成功した場合：新たに作成された「JSON配列」のcJSONノードへのポインタ
    - 	cJSONノードの作成に失敗した場合：NULL

#### 宣言・定義
-   宣言場所：cJSON.h
-   定義場所：cJSON.c

---

```c++
CJSON_PUBLIC(cJSON *) cJSON_Parse(const char *value);
```

引数valueで指定された文字列を受け取り、その内容がJSON形式であるかを解析します。解析結果に応じて、対応するcJSONノードを構築します。
- 入力（引数）：
    - 	value：解析したい文字列
- 出力（戻り値）：
    - 	文字列の解析に成功した場合：解析結果として構築されたcJSONノードの最上位ノードへのポインタ
    - 	文字列の解析に失敗した場合：NULL

#### 宣言・定義
-   宣言場所：cJSON.h
-   定義場所：cJSON.c

---

```c++
CJSON_PUBLIC(char *) cJSON_Print(const cJSON *item);
```

引数itemで指定されたcJSONノードからJSON形式の文字列を生成して戻します。
- 入力（引数）：
    - 	item：JSON形式の文字列として生成したいcJSONノードへのポインタ
- 出力（戻り値）：
    - 	JSON形式の文字列の生成に成功した場合：生成されたJSON形式の文字列
    - 	JSON形式の文字列の生成に失敗した場合：NULL

#### 宣言・定義
-   宣言場所：cJSON.h
-   定義場所：cJSON.c

---

```c++
CJSON_PUBLIC(cJSON *) cJSON_Duplicate(const cJSON *item, cJSON_bool recurse);
```

引数itemで指定されたcJSONノードを複製します。引数recurseを指定することで、引数itemに格納されているcJSONノード（next, prev, child）も再帰的に複製するかを制御できます。\
・引数recurseがtrueの場合：引数itemに格納されているcJSONノードも複製され、複製されたcJSONノードが置き換わります。\
・引数recurseがfalseの場合：引数itemに格納されているcJSONノードも複製され、元のcJSONノードが参照されます。
- 入力（引数）：
    - 	item：複製したいcJSONノードへのポインタ
    - 	recurse：引数itemで指定されたに格納されているcJSONノードも再帰的に複製するかどうかの真偽値
- 出力（戻り値）：
    - 	cJSONノードの複製に成功した場合：複製されたcJSONノードへのポインタ
    - 	cJSONノードの複製に失敗した場合：NULL

#### 宣言・定義
-   宣言場所：cJSON.h
-   定義場所：cJSON.c

---

```c++
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToArray(cJSON *array, cJSON *item);
```

引数arrayで指定された「JSON配列」に、引数itemで指定されたcJSONノードを「JSON配列」に追加します。
- 入力（引数）：
    - 	array：cJSONノードを追加したい対象の「JSON配列」へのポインタ
    - 	item：「JSON配列」に追加したいcJSONノードへのポインタ
- 出力（戻り値）：
    - 	cJSONノードの「JSON配列」への追加が成功した場合：true
    - 	cJSONノードの「JSON配列」への追加が失敗した場合：false

#### 宣言・定義
-   宣言場所：cJSON.h
-   定義場所：cJSON.c

---

```c++
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);
```

引数objectで指定された「JSONオブジェクト」に、引数stringで指定されたキーと、引数itemで指定されたcJSONノードをペアとして追加します。
- 入力（引数）：
    - object：cJSONノードを追加したい対象の「JSONオブジェクト」へのポインタ
    - string：「JSONオブジェクト」に追加するcJSONノードのキーに使用する文字列
    - item：「JSONオブジェクト」に追加したいcJSONノードへのポインタ
- 出力（戻り値）：
    - cJSONノードの「JSONオブジェクト」への追加が成功した場合：true
    - cJSONノードの「JSONオブジェクト」への追加が失敗した場合：false

#### 宣言・定義
-   宣言場所：cJSON.h
-   定義場所：cJSON.c

---

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

/* securely comparison of floating-point variables */
static cJSON_bool compare_double(double a, double b)
{
    double maxVal = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    return (fabs(a - b) <= maxVal * DBL_EPSILON);
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

/* sort lists using mergesort */
static cJSON *sort_list(cJSON *list, const cJSON_bool case_sensitive)
{
    cJSON *smaller;
    cJSON *first = list;
    cJSON *second = list;
    cJSON *current_item = list;
    cJSON *result = list;
    cJSON *result_tail = NULL;
    int compare_strings;
    const unsigned char *string1;
    const unsigned char *string2;

    if ((list == NULL) || (list->next == NULL))
    {
        /* One entry is sorted already. */
        return result;
    }

    string1 = (unsigned char*)current_item->string;
    string2 = (unsigned char*)current_item->next->string;

    if ((string1 == NULL) || (string2 == NULL))
    {
        compare_strings = 1;
    }
    else if (string1 == string2)
    {
        compare_strings = 0;
    }
    else if (case_sensitive)
    {
        compare_strings = strcmp((const char*)string1, (const char*)string2);
    }
    else
    {
        for(; tolower(*string1) == tolower(*string2); (void)string1++, string2++)
        {
            if (*string1 == '\0')
            {
                break;
            }
        }

        compare_strings = (*string1) - tolower(*string2);
    }

    while ((current_item != NULL) && (current_item->next != NULL) && (compare_strings < 0))
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

        string1 = (unsigned char*)first->string;
        string2 = (unsigned char*)second->string;

        if ((string1 == NULL) || (string2 == NULL))
        {
            compare_strings = 1;
        }
        else if (string1 == string2)
        {
            compare_strings = 0;
        }
        else if (case_sensitive)
        {
            compare_strings = strcmp((const char*)string1, (const char*)string2);
        }
        else
        {
            for(; tolower(*string1) == tolower(*string2); (void)string1++, string2++)
            {
                if (*string1 == '\0')
                {
                    break;
                }
            }

            compare_strings = (*string1) - tolower(*string2);
        }

        if (compare_strings < 0)
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

static void sort_object(cJSON * const object, const cJSON_bool case_sensitive)
{
    if (object == NULL)
    {
        return;
    }
    object->child = sort_list(object->child, case_sensitive);
}

static void compose_patch(cJSON * const patches, const unsigned char * const operation, const unsigned char * const path, const unsigned char *suffix, const cJSON * const value)
{
    unsigned char *full_path;
    const unsigned char *string;
    cJSON *patch = NULL;
    size_t suffix_length, path_length;

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
        string = suffix;
        for (suffix_length = 0; *string != '\0'; (void)string++, suffix_length++)
        {
            /* character needs to be escaped? */
            if ((*string == '~') || (*string == '/'))
            {
                suffix_length++;
            }
        }
        path_length = strlen((const char*)path);
        full_path = (unsigned char*)malloc(path_length + suffix_length + sizeof("/"));

        sprintf((char*)full_path, "%s/", (const char*)path);
        encode_string_as_pointer(full_path + path_length + 1, suffix);

        cJSON_AddItemToObject(patch, "path", cJSON_CreateString((const char*)full_path));
        free(full_path);
    }

    if (value != NULL)
    {
        cJSON_AddItemToObject(patch, "value", cJSON_Duplicate(value, 1));
    }
    cJSON_AddItemToArray(patches, patch);
}

void create_patches(cJSON * const patches, const unsigned char * const path, cJSON * const from, cJSON * const to, const cJSON_bool case_sensitive)
{
    int diff;
    unsigned char *new_path;
    const unsigned char *string;
    const unsigned char *string1;
    const unsigned char *string2;
    size_t index;
    cJSON *from_child, *to_child;
    size_t path_length, from_child_name_length;

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
            return;

        case cJSON_String:
            if (strcmp(from->valuestring, to->valuestring) != 0)
            {
                compose_patch(patches, (const unsigned char*)"replace", path, NULL, to);
            }
            return;

        case cJSON_Array:
        {
            index = 0;
            from_child = from->child;
            to_child = to->child;
            new_path = (unsigned char*)malloc(strlen((const char*)path) + 20 + sizeof("/")); /* Allow space for 64bit int. log10(2^64) = 20 */

            /* generate patches for all array elements that exist in both "from" and "to" */
            for (index = 0; (from_child != NULL) && (to_child != NULL); (void)(from_child = from_child->next), (void)(to_child = to_child->next), index++)
            {
                /* check if conversion to unsigned long is valid
                 * This should be eliminated at compile time by dead code elimination
                 * if size_t is an alias of unsigned long, or if it is bigger */
                if (index > ULONG_MAX)
                {
                    free(new_path);
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
                    free(new_path);
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
            free(new_path);
            return;
        }

        case cJSON_Object:
        {
            from_child = NULL;
            to_child = NULL;

            if (from != NULL)
            {
                from->child = sort_list(from->child, case_sensitive);
            }
            if (to != NULL)
            {
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
                    string1 = (unsigned char*)from_child->string;
                    string2 = (unsigned char*)to_child->string;

                    if ((string1 == NULL) || (string2 == NULL))
                    {
                        diff = 1;
                    }
                    else if (string1 == string2)
                    {
                        diff = 0;
                    }
                    else if (case_sensitive)
                    {
                        diff = strcmp((const char*)string1, (const char*)string2);
                    }
                    else
                    {
                        for(; tolower(*string1) == tolower(*string2); (void)string1++, string2++)
                        {
                            if (*string1 == '\0')
                            {
                                break;
                            }
                        }

                        diff = (*string1) - tolower(*string2);
                    }
                }

                if (diff == 0)
                {
                    /* both object keys are the same */
                    path_length = strlen((const char*)path);
                    string = (unsigned char*)from_child->string;
                    for (from_child_name_length = 0; *string != '\0'; (void)string++, from_child_name_length++)
                    {
                        /* character needs to be escaped? */
                        if ((*string == '~') || (*string == '/'))
                        {
                            from_child_name_length++;
                        }
                    }
                    new_path = (unsigned char*)malloc(path_length + from_child_name_length + sizeof("/"));

                    sprintf((char*)new_path, "%s/", path);
                    encode_string_as_pointer(new_path + path_length + 1, (unsigned char*)from_child->string);

                    /* create a patch for the element */
                    create_patches(patches, new_path, from_child, to_child, case_sensitive);
                    free(new_path);

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
            return;
        }

        default:
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
```

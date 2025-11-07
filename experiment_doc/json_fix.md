### インクルード経路について

この資料では、ヘッダーファイル間の依存関係を表現するために、以下の記法を用います。

A -> B

ヘッダーファイルAが、ヘッダーファイルBをインクルードしていることを表現します。

このルールに基づき、各項目での冗長な説明は省略します。

### 構造体・マクロ・関数の説明

外部ファイルで定義されている構造体・マクロ・関数について説明します。

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
JSONのバリューを表現する構造体です。
-   next：同じ階層のバリュー（JSON配列、JSONオブジェクトの要素群）のうち、1つ次のバリューへのポインタを格納します。
-   prev：同じ階層のバリューのうち、1つ前のバリューへのポインタを格納します。
-	child：1つ下の階層のバリュー（JSON配列、JSONオブジェクトの要素）へのポインタを格納します。
-	type：バリューの種類を表現します。
-	valuestring：バリューの種類がJSON文字列のバリューを格納します。
-	valueint：バリューの種類がJSON数値（整数）のバリューを格納します。
-	valuedouble：バリューの種類がJSON数値（浮動小数点）のバリューを格納します。
-	string：バリューにキーが存在するとき、キーを格納します。

#### 宣言・定義
-   定義場所：cJSON_Utils.h -> cJSON.h

---

```c++
typedef int cJSON_bool;
```

JSON真偽値を扱うための型です。

#### 宣言・定義
-   定義場所：cJSON_Utils.h -> cJSON.h

---

```c++
#define CJSON_PUBLIC(type) type
```

引数typeで指定された型を戻します。

#### 宣言・定義
-   定義場所：cJSON_Utils.h -> cJSON.h

---

```c++
#define cJSON_False  (1 << 0) 
#define cJSON_True   (1 << 1) 
#define cJSON_NULL   (1 << 2) 
#define cJSON_Number (1 << 3) 
#define cJSON_String (1 << 4) 
#define cJSON_Array  (1 << 5) 
#define cJSON_Object (1 << 6) 
```

バリューの種類を識別するマクロです。
-	cJSON_False	    ：JSON真偽値（偽）
-	cJSON_True	    ：JSON真偽値（真）
-	cJSON_NULL	    ：null値
-	cJSON_Number	：JSON数値
-	cJSON_String	：JSON文字列
-	cJSON_Array	    ：JSON配列
-	cJSON_Object	：JSONオブジェクト

#### 宣言・定義
-   定義場所：cJSON_Utils.h -> cJSON.h

---

```c++
CJSON_PUBLIC(void) cJSON_Delete(cJSON *item);
```

引数itemで指定されたcJSONオブジェクトのメモリを解放します。
- 入力（引数）：
    -	item：削除するcJSONオブジェクトへのポインタ
- 出力（戻り値）：
    -	無し
 
#### 宣言・定義
-   宣言場所：cJSON_Utils.h -> cJSON.h

---

```c++
CJSON_PUBLIC(cJSON *) cJSON_CreateString(const char *string);
```
以下のようにメンバを設定したcJSONオブジェクトを作成します。
```c++
prev        = NULL;
next        = NULL;
child       = NULL;
type        = cJSON_String;
valuestring = string; // 引数stringを格納する
valueint    = 0;
valuedouble = 0.0;
string      = NULL;
```

- 入力（引数）：
    -	string：「JSON文字列」として作成する文字列
- 出力（戻り値）：
    - cJSONオブジェクトの作成に成功した場合：作成したcJSONオブジェクトへのポインタ
    - cJSONオブジェクトの作成に失敗した場合：NULL
 
#### 宣言・定義
-   宣言場所：cJSON_Utils.h -> cJSON.h

---

```c++
CJSON_PUBLIC(cJSON *) cJSON_CreateObject(void);
```
以下のようにメンバを設定したcJSONオブジェクトを作成します。
```c++
prev        = NULL;
next        = NULL;
child       = NULL;
type        = cJSON_Object;
valuestring = NULL;
valueint    = 0;
valuedouble = 0.0;
string      = NULL;
```

- 入力（引数）：
    -	無し
- 出力（戻り値）：
    - 	cJSONノードの作成に成功した場合：作成したcJSONオブジェクトへのポインタ
    - 	cJSONノードの作成に失敗した場合：NULL
 
#### 宣言・定義
-   宣言場所：cJSON_Utils.h -> cJSON.h

---

```c++
CJSON_PUBLIC(cJSON *) cJSON_CreateArray(void );
```
以下のようにメンバを設定したcJSONオブジェクトを作成します。
```c++
prev        = NULL;
next        = NULL;
child       = NULL;
type        = cJSON_Array;
valuestring = NULL;
valueint    = 0;
valuedouble = 0.0;
string      = NULL;
```

- 入力（引数）：
    - 	無し
- 出力（戻り値）：
    - 	cJSONノードの作成に成功した場合：作成したcJSONオブジェクトへのポインタ
    - 	cJSONノードの作成に失敗した場合：NULL

#### 宣言・定義
-   宣言場所：cJSON_Utils.h -> cJSON.h

---

```c++
CJSON_PUBLIC(cJSON *) cJSON_Parse(const char *value);
```

引数valueで指定された文字列をcJSONオブジェクト群に変換します。
- 入力（引数）：
    - 	value：変換する文字列
- 出力（戻り値）：
    - 	文字列の変換に成功した場合：変換したcJSONオブジェクト群のうち、0階層のcJSONオブジェクトへのポインタ
    - 	文字列の変換に失敗した場合：NULL
 
文字列をcJSONオブジェクト群に変換する例：\
変換する文字列：
```json
{
    "key1": "value1",
    "key2": 1.0
}
```
変換したcJSONオブジェクト群：
<img width="802" height="738" alt="cJSONオブジェクト群の例 drawio" src="https://github.com/user-attachments/assets/e4ab4d30-7be2-461f-944d-06f007ead0d3" />

#### 宣言・定義
-   宣言場所：cJSON_Utils.h -> cJSON.h

---

```c++
CJSON_PUBLIC(char *) cJSON_Print(const cJSON *item);
```

引数itemで指定されたitemから、JSON形式の文字列を生成します。
- 入力（引数）：
    - 	item：JSON形式の文字列を生成するcJSONオブジェクトへのポインタ
- 出力（戻り値）：
    - 	JSON形式の文字列の生成に成功した場合：生成したJSON形式の文字列
    - 	JSON形式の文字列の生成に失敗した場合：NULL

#### 宣言・定義
-   宣言場所：cJSON_Utils.h -> cJSON.h

---

```c++
CJSON_PUBLIC(cJSON *) cJSON_Duplicate(const cJSON *item, cJSON_bool recurse);
```

引数itemで指定されたcJSONオブジェクトを複製します。引数recurseを指定して、引数itemが格納しているcJSONオブジェクト（next, prev, child）も複製するかを制御します。\
・引数recurseがtrueの場合：引数itemが格納しているcJSONオブジェクトも複製し、複製したcJSONオブジェクトに置き換えます。\
・引数recurseがfalseの場合：引数itemが格納しているcJSONオブジェクトは複製せず、引数itemが格納しているcJSONオブジェクトを参照します。
- 入力（引数）：
    - 	item：複製するcJSONオブジェクトへのポインタ
    - 	recurse：引数itemで指定されたcJSONオブジェクトが格納しているcJSONオブジェクトも再帰的に複製するかの真偽値
- 出力（戻り値）：
    - 	cJSONオブジェクトの複製に成功した場合：複製したcJSONオブジェクトへのポインタ
    - 	cJSONオブジェクトの複製に失敗した場合：NULL

#### 宣言・定義
-   宣言場所：cJSON_Utils.h -> cJSON.h

---

```c++
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToArray(cJSON *array, cJSON *item);
```
引数itemで指定されたcJSONオブジェクトを、引数arrayで指定されたcJSONオブジェクト（JSON配列）に追加します。
- 入力（引数）：
    - 	array：cJSONオブジェクトを追加する対象のcJSONオブジェクト（JSON配列）へのポインタ
    - 	item：「JSON配列」に追加するcJSONオブジェクトへのポインタ
- 出力（戻り値）：
    - 	cJSONオブジェクト（JSON配列）への追加が成功した場合：1
    - 	cJSONオブジェクト（JSON配列）への追加が失敗した場合：0

#### 宣言・定義
-   宣言場所：cJSON_Utils.h -> cJSON.h

---

```c++
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);
```

引数stringで指定されたキーと、引数itemで指定されたcJSONオブジェクトの組を、引数objectで指定されたcJSONオブジェクト（JSONオブジェクト）に追加します。
- 入力（引数）：
    - object：cJSONオブジェクトを追加する対象のcJSONオブジェクト（JSONオブジェクト）へのポインタ
    - string：「JSONオブジェクト」に追加するcJSONオブジェクトのキーに使用する文字列
    - item：「JSONオブジェクト」に追加するcJSONオブジェクトへのポインタ
- 出力（戻り値）：
    - cJSONオブジェクト（JSONオブジェクト）への追加が成功した場合：1
    - cJSONオブジェクト（JSONオブジェクト）への追加が失敗した場合：0

#### 宣言・定義
-   宣言場所：cJSON_Utils.h -> cJSON.h

---

### 標準的な構造体・マクロ関数の説明

```c++
#define ULONG_MAX (__LONG_MAX__ *2UL+1UL)
```
unsigned long型が取りうる最大値を定義したマクロです。

#### 宣言・定義
- 定義場所：limits.h

---

```c++
#define DBL_EPSILON __DBL_EPSILON__
```
「1.0」と「1.0より大きい次の表現可能な浮動点小数との差」であるマシンイプシロンを定義したマクロです。

#### 宣言・定義
- 定義場所：float.h

---

```c++
size_t
```
オブジェクトのサイズ（バイト単位）を表現するのに十分な大きさが保障された、符号なし整数型です。

#### 宣言・定義
- 宣言場所：stdlib.h

---

```c++
int strcmp(const char *s1, const char *s2);
```
引数s1で指定された文字列と引数s2で指定された文字列を先頭から1文字ずつ比較し、どちらが大きいか、小さいか、あるいは等しいかを判定します。

- 入力（引数）：
    - s1：比較する1つ目の文字列
    - s2：比較する2つ目の文字列
- 出力（戻り値）：
    - 引数s1で指定された文字列が引数s2で指定された文字列よりも辞書順で小さい場合：負の値
    - 引数s1で指定された文字列と引数s2で指定された文字列が等しい場合：0
    - 引数s1で指定された文字列が引数s2で指定された文字列よりも辞書順で大きい場合：正の値
 
※ 同じアルファベットの大文字小文字を比較した時には、小文字のほうが辞書順で大きいと判断されます。

#### 宣言・定義
-    宣言場所：string.h

---

```c++
size_t strlen(const char *s);
```
引数sで指定された文字列の長さを計算します。文字列の先頭から、終端文字`\0`の直前までの文字数を返します。

- 入力（引数）：
    - s：長さを計算する文字列
- 出力（戻り値）：
    - 終端文字`\0`の直前までの文字数

#### 宣言・定義
- 宣言場所：string.h

---

```c++
int tolower(int c);
```
引数cで指定された文字コードが大文字のアルファベットであった場合に、それに対応する小文字に変換して返します。

- 入力（引数）：
    - c：変換する文字の文字コードを表す整数
- 出力（戻り値）：
    - 引数cが'A'から'Z'までの大文字アルファベットの場合：対応する小文字の文字コード
    - それ以外の場合：引数cの値

#### 宣言・定義
- 宣言場所：ctype.h

---

```c++
double fabs(double a);
```
引数aで指定された浮動点小数の絶対値を計算し、計算した結果を返します。

- 入力（引数）：
    - a：絶対値を計算する値
- 出力（戻り値）：
    - 引数aの絶対値を計算した結果

#### 宣言・定義
-    宣言場所：math.h

---

```c++
void *malloc(size_t size);
```
引数sizeで指定されたバイト数のメモリ領域をヒープ領域から確保し、その領域へのポインタを返します。

- 入力（引数）：
    - size：確保するメモリ領域のバイト数
- 出力（戻り値）：
    - メモリ領域の確保に成功した場合：確保されたメモリ領域の先頭アドレスを指すポインタ
    - メモリ領域の確保に失敗した場合：NULL

#### 宣言・定義
- 宣言場所：stdlib.h

---

```c++
void free(void *ptr)
```
引数ptrで指定されたメモリ領域を解放します。

- 入力（引数）：
    - ptr：解放するメモリ領域
- 出力（戻り値）：
    - 無し

#### 宣言・定義
- 宣言場所：stdlib.h

---

```c++
int sprintf(char *buffer, const char *format, ...);
```
引数formatで指定された書式に従って、第三引数以降で指定された変数を文字列に変換し、引数bufferが指す文字配列に書き込みます。

- 入力（引数）：
    - buffer：書き込み先の文字配列
    - format：書式を指定する文字列
    - ...：指定した書式に対応する可変長引数
- 出力（戻り値）：
    - 書き込みに成功した場合：書き込まれた文字数
    - 書き込みに失敗した場合：負の値

#### 宣言・定義
-    宣言場所：stdio.h

---

```c++
int printf(const char* str, ...);
```
引数srcで指定された書式が含まれた文字列に従って、第二引数以降で指定された引数を文字列に変換し、標準出力します。

- 入力（引数）：
    - str：標準出力する書式が含まれた文字列
    - ...：引数strに含まれる書式の数だけ対応する変数を格納した可変長引数
- 出力（戻り値）：
    - 文字列の標準出力に成功した場合：標準出力した文字数
    - 文字列の標準出力に失敗した場合：負の数

#### 宣言・定義
- 宣言場所：iniparser.h -> dictionary.h -> stdio.h

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
#pragma warning (disable ：4001)
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
    double maxVal = fabs(a) > fabs(b) ? fabs(a) ：fabs(b);
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
        suffix_length = pointer_encoded_length(suffix);
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
            sort_object(from, case_sensitive);
            sort_object(to, case_sensitive);

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
                    path_length = strlen((const char*)path);
                    from_child_name_length = pointer_encoded_length((unsigned char*)from_child->string);
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
                                   "  \"name\"：\"John Doe\",\n"
                                   "  \"age\"：30,\n"
                                   "  \"city\"：\"Anytown\",\n"
                                   "  \"tags\"：[\"json\", \"c\"]\n"
                                   "}";

    const char *to_json_string = "{\n"
                                 "  \"name\"：\"Jane Doe\",\n"
                                 "  \"age\"：31,\n"
                                 "  \"occupation\"：\"Engineer\",\n"
                                 "  \"tags\"：[\"json\", \"c\", \"patch\"]\n"
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

### インクルード経路について

この資料では、ヘッダーファイル間の依存関係を表現するために、以下のような記法を用います。

A -> B

ヘッダーファイルAが、その内部でヘッダーファイルBをインクルードしていることを示します。
ヘッダーファイルAを読み込むと、Bも一緒に読み込まれるという関係性を表します。

### 構造体・マクロ・関数の説明

以下は、ソースコード読解実験に使用するプログラムを補足するものです。プログラム本体には定義されていませんが、外部ファイルで利用されている構造体、マクロ、関数について説明します。

```c++
typedef struct _dictionary_ {
    unsigned        n ;     
    size_t          size ; 
    char        **  val ;   
    char        **  key ;   
    unsigned     *  hash ;  
} dictionary ;
```

辞書型データ構造を表現した構造体です。
- n	：辞書のキーとバリューのペア数
- size	：辞書が保有できる最大ペア数
- val	：バリューのリスト
- key	：キーのリスト
- hash	：ハッシュ化したキーのリスト

#### 宣言・定義
- 定義場所：iniparser.h -> dictionary.h

---

```c++
dictionary * dictionary_new(size_t size);
```

新たに辞書をメモリ上に用意し、その辞書オブジェクトを戻します。\
・引数sizeを指定することで、辞書が保有できる最大ペア数分のメモリを確保することができます。ただし、最低ペア数が128で定義されており、引数size<128の場合、辞書が保有できる最大ペア数は128となります。\
・作成された辞書には、キーとバリューのペアは入っていません。
- 入力（引数）：
  - 	size：作成する辞書が保有できる最大ペア数
- 出力（戻り値）：
  - 	辞書の作成に成功した場合：新しく作成された辞書へのポインタ
  - 	辞書の作成に失敗した場合：NULL

#### 宣言・定義
- 宣言場所：iniparser.h -> dictionary.h

---

```c++
void dictionary_del(dictionary * d);
```

引数dで指定された辞書とそのメンバをメモリ上から削除します。
- 入力（引数）：
  - 	d：削除したい辞書へのポインタ
- 出力（戻り値）：
  - 	無し

#### 宣言・定義
- 宣言場所：iniparser.h -> dictionary.h

---

```c++
const char * dictionary_get(const dictionary * d, const char * key, const char * def);
```

引数dで指定された辞書の中から、引数keyで指定されたキーに対応するバリューを戻します。
- 入力（引数）：
  - 	d：操作対象となる辞書へのポインタ
  - 	key：検索したい情報のキー
  - 	def：引数keyに対応するバリューが見つからなかった場合に戻される文字列
- 出力（戻り値）：
  - 	引数keyで指定されたキーが辞書に登録されている場合：引数keyで指定されたキーに対応するバリュー
  - 	引数keyで指定されたキーが辞書に登録されていない場合：引数defで指定された文字列

#### 宣言・定義
- 宣言場所：iniparser.h -> dictionary.h

---

```c++
int dictionary_set(dictionary * d, const char * key, const char * val);
```

引数dで指定された辞書の中から、引数keyで指定されたキーに対応するバリューが存在するかを確認し、以下の操作を行います。\
・引数keyで指定されたキーに対応するバリューが辞書内に存在する場合は、引数 key に対応する既存のバリューを、引数 val で指定された文字列に置き換えます。\
・引数keyで指定されたキーに対応するバリューが辞書内に存在しない場合は、新たに引数 key で指定されたキーと、それに対応する引数 val で指定されたバリューのペアを、引数 d で指定された辞書に追加します。
- 入力（引数）：
  - d：操作対象となる辞書へのポインタ
  - key：追加または更新したいペアのキー
  - val：keyに対応するバリュー
- 出力（戻り値）：
  - 追加または更新が成功した場合：0
  - 追加または更新が失敗した場合：-1

#### 宣言・定義
- 宣言場所：iniparser.h -> dictionary.h

---

```c++
void dictionary_unset(dictionary * d, const char * key);
```

引数dで指定された辞書の中から引数keyで指定されたキーに対応するペアを削除します。
・引数keyで指定されたキーが辞書に登録されている場合は、引数keyで指定されたキーに対応するペアを辞書から削除します。
・引数keyで指定されたキーが辞書に登録されていない場合は、何も行いません。
- 入力（引数）：
  - 	d：操作対象となる辞書へのポインタ
  - 	key：辞書から削除したいペアのキー
- 出力（戻り値）：
  - 	無し

#### 宣言・定義
- 宣言場所：iniparser.h -> dictionary.h

### 標準的な構造体・マクロ関数の説明

```c++
size_t
```
オブジェクトのサイズ（バイト単位）を表現するのに十分な大きさが保障された、符号なし整数型です。

#### 宣言・定義
- 宣言場所：stdlib.h

---

```c++
va_list
```
可変長引数リストを表現するための型です。

#### 宣言・定義
- 宣言場所：stdarg.h

---

```c++
intmax_t
```
最も大きい符号付き整数型です。

#### 宣言・定義
- 宣言場所：iniparser.h -> stdint.h

---

```c++
uintmax_t
```
最も大きい符号無し整数型です。

#### 宣言・定義
- 宣言場所：iniparser.h -> stdint.h

---

```c++
long strtol(const char *str, char *end, int base);
```
引数strで指定された文字列を引数baseで指定された基数を元に、long型の整数に変換します。変換処理が終了した文字位置を引数endに格納します。

- 入力（引数）：
    - str：変換したい文字列
    - end：変換処理が終了した時の文字位置
    - base：変換したい文字列の基数
- 出力（戻り値）：
    - 文字列の変換に成功した場合：変換された整数
    - 文字列を変換できる文字がなかった場合：0
    - 文字列を変換した結果がlong型を超えてしまった場合：LONG_MAX または LONG_MIN

#### 宣言・定義
- 宣言場所：stdlib.h

---

```c++
intmax_t strtoimax(const char *str, char *end, int base);
```
引数strで指定された文字列を引数baseで指定された基数を元に、intmax_t型の整数に変換します。変換処理が終了した文字位置を引数endに格納します。

- 入力（引数）：
    - str：変換したい文字列
    - end：変換処理が終了した時の文字位置
    - base：変換したい文字列の基数
- 出力（戻り値）：
    - 文字列の変換に成功した場合：変換された整数
    - 文字列を変換できる文字がなかった場合：0
    - 文字列を変換した結果がlong型を超えてしまった場合：INTMAX_MAX または INTMAX_MIN

#### 宣言・定義
- 宣言場所：inttypes.h

---

```c++
uintmax_t strtoumax(const char *str, char *end, int base);
```
引数strで指定された文字列を引数baseで指定された基数を元に、uintmax_t型の整数に変換します。変換処理が終了した文字位置を引数endに格納します。

- 入力（引数）：
    - str：変換したい文字列
    - end：変換処理が終了した時の文字位置
    - base：変換したい文字列の基数
- 出力（戻り値）：
    - 文字列の変換に成功した場合：変換された整数
    - 文字列を変換できる文字がなかった場合：0
    - 文字列を変換した結果がlong型を超えてしまった場合：UINTMAX_MAX または UINTMAX_MIN

#### 宣言・定義
- 宣言場所：inttypes.h

---

```c++
double atof(const char *str);
```
引数strで指定された文字列をdouble型の浮動点小数に変換します。

- 入力（引数）：
    - str：変換したい文字列
- 出力（戻り値）：
    - 文字列の変換に成功した場合：変換された浮動点小数
    - 文字列の変換が途中まで成功した場合：途中まで変換された浮動点小数
    - 文字列の先頭が数値として解釈できない、変換した結果がdouble型を超えた場合：未定義

#### 宣言・定義
- 宣言場所：stdlib.h

---

```c++
int tolower(int c);
```
引数cで指定された文字コードが大文字のアルファベットであった場合に、それに対応する小文字に変換して返します。

- 入力（引数）：
    - c：変換したい文字の文字コードを表す整数
- 出力（戻り値）：
    - 引数cが'A'から'Z'までの大文字アルファベットの場合：対応する小文字の文字コード
    - それ以外の場合：引数cの値

#### 宣言・定義
- 宣言場所：ctype.h

---

```c++
int isspace(int c);
```
引数cで指定された文字コードが空白文字であるかをチェックし、結果を返します。

- 入力（引数）：
    - c：判定したい文字の文字コードを表す整数
- 出力（戻り値）：
    - 文字コードが空白文字の場合：0以外の整数
    - 文字コードが空白文字でない場合：0

#### 宣言・定義
- 定義場所：ctype.h

---

```c++
size_t strlen(const char *s);
```
引数sで指定された文字列の長さを計算します。文字列の先頭から、終端文字`\0`の直前までの文字数を返します。

- 入力（引数）：
    - s：長さを計算したい文字列
- 出力（戻り値）：
    - 終端文字`\0`の直前までの文字数

#### 宣言・定義
- 宣言場所：string.h

---

```c++
void *memcpy(void *dst, const void *src, size_t n);
```
引数srcが指すメモリ領域から引数nで指定されたバイト数分のメモリ領域を、引数dstが指すメモリ領域にコピーします。\
引数srcと引数dstのメモリ領域が重複する場合の動作は、未定義です。

- 入力（引数）：
    - dst：コピー先のメモリ領域を指すポインタ
    - src：コピー元のメモリ領域を指すポインタ
    - n：コピーするバイト数
- 出力（戻り値）：
    - 引数dstで指定されたコピー先のポインタ

#### 宣言・定義
- 宣言場所：string.h

---

```c++
void *memmove(void *dst, const void *src, size_t n);
```
引数srcが指すメモリ領域から引数nで指定されたバイト数分のメモリ領域を、引数dstが指すメモリ領域にコピーします。\
引数srcと引数dstのメモリ領域が重複する場合でも、安全に動作します。

- 入力（引数）：
    - dst：コピー先のメモリ領域を指すポインタ
    - src：コピー元のメモリ領域を指すポインタ
    - n：コピーするバイト数
- 出力（戻り値）：
    - 引数dstで指定されたコピー先のポインタ

#### 宣言・定義
- 宣言場所：string.h

---

```c++
void *memset(void *src, int c, size_t n);
```
引数srcが指すメモリ領域の先頭から引数nで指定されたバイト数分、引数cで指定された値で埋めます。

- 入力（引数）：
    - src：値を埋めたいメモリ領域
    - c：メモリ領域を埋めるための値
    - n：メモリ領域を埋めるバイト数
- 出力（戻り値）：
    - 引数srcで指定されたメモリ領域

#### 宣言・定義
- 宣言場所：string.h

---

```c++
void va_start(va_list argptr, param);
```
引数argptrで指定された可変長引数リストへのアクセスを初期化します。

- 入力（引数）：
    - argptr：初期化対象の可変長引数リスト
    - param：可変長引数リストの直前にある名前付き引数
- 出力（戻り値）：
    - 無し

#### 宣言・定義
- 宣言場所：stdarg.h

---

```c++
void va_end(va_list argptr);
```
引数argptrで指定された可変長引数リストが使用しているリソースを解放し、無効な状態にします。

- 入力（引数）：
    - argptr：利用が終わった可変長引数リスト
- 出力（戻り値）：
    - 無し

#### 宣言・定義
- 宣言場所：stdarg.h

---

```c++
int vfprintf(FILE *stream, const char *format, va_list argptr);
```
引数argptrで指定された可変長引数リストを引数formatで指定された書式で引数streamで指定されたファイルストリームに出力します。
- 入力（引数）：
    - stream：出力先のファイルストリームへのポインタ
    - format：書式を指定する文字列
    - argptr：初期化済みの可変長引数リスト
- 出力（戻り値）：
    - ファイルストリームへの書き込みが成功した場合：書き込まれた文字数
    - ファイルストリームへの書き込みが失敗した場合：-1

#### 宣言・定義
- 宣言場所：iniparser.h -> dictionary.h -> stdio.h

---

```c++
int sscanf(const char *buffer, const char *format, ...);
```
引数bufferで指定された文字列から引数formatで指定された書式に従って読み取り、第三引数以降の変数に格納します。

- 入力（引数）：
    - buffer：読み取り対象の文字列
    - format：書式を指定する文字列
    - ...：読み取ったデータを格納する可変長引数
- 出力（戻り値）：
    - 読み取りに成功した場合：書式通りに読み取りに成功した数
    - 最初の書式の読み取りに失敗した場合：EOF

#### 宣言・定義
- 宣言場所：iniparser.h -> dictionary.h -> stdio.h

---

```c++
int printf(const char* str, ...);
```
引数srcで指定された書式が含まれた文字列に従って、第二引数以降で指定された引数を文字列に変換し、標準出力します。

- 入力（引数）：
    - str：標準出力したい書式が含まれた文字列
    - ...：引数strに含まれる書式の数だけ対応する変数を格納した可変長引数
- 出力（戻り値）：
    - 文字列の標準出力に成功した場合：標準出力した文字数
    - 文字列の標準出力に失敗した場合：負の数

#### 宣言・定義
- 宣言場所：iniparser.h -> dictionary.h -> stdio.h

---

```c++
void *malloc(size_t size);
```
引数sizeで指定されたバイト数のメモリ領域をヒープ領域から確保し、その領域へのポインタを返します。

- 入力（引数）：
    - size：確保したいメモリ領域のバイト数
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
    - ptr：解放したいメモリ領域
- 出力（戻り値）：
    - 無し

#### 宣言・定義
- 宣言場所：stdlib.h

---

```c++
int feof(FILE *fp);
```
引数fpで指定されたファイルストリームがファイルの終端に達しているかを判定します。終端に達していれば0以外の値を返します。

- 入力（引数）：
    - fp：ファイルの終端に達しているか判定したいファイルストリーム
- 出力（戻り値）：
    - ファイルストリームがファイルの終端に達している場合：0以外の値
    - ファイルストリームがファイルの終端に達していない場合：0

#### 宣言・定義
- 宣言場所：iniparser.h -> dictionary.h -> stdio.h

---

```c++
FILE *fopen(const char *filename, const char *mode);
```
引数filenameで指定されたファイルを引数modeで指定されたモードで開き、そのファイルへのポインタを返します。

- 入力（引数）：
    - filename：開きたいファイルの名前
    - mode：ファイルをどのように開くかを指定する文字列
- 出力（戻り値）：
    - ファイルを開くことに成功した場合：そのファイルを指すポインタ
    - ファイルを開くことに失敗した場合：NULL
 
|モード|説明|
|:-:|:-:|
|"r"|読み込み|

#### 宣言・定義
- 宣言場所：iniparser.h -> dictionary.h -> stdio.h

---

```c++
int fclose(FILE *fp);
```
引数fpで指定されたファイルストリームを閉じ、使用していたリソースを解放します。

- 入力（引数）：
    - fp：閉じたいファイルのポインタ
- 出力（戻り値）：
    - ファイルを閉じることに成功した場合：0
    - ファイルを閉じることに失敗した場合：EOF

#### 宣言・定義
- 宣言場所：iniparser.h -> dictionary.h -> stdio.h

---

```c++
/*-------------------------------------------------------------------------*/
/**
   @file    iniparser.c
   @author  N. Devillard
   @brief   Parser for ini files.
*/
/*--------------------------------------------------------------------------*/
/*---------------------------- Includes ------------------------------------*/
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "iniparser.h"

/*---------------------------- Defines -------------------------------------*/
#define ASCIILINESZ         (1024)
#define INI_INVALID_KEY     ((char*)-1)

/*---------------------------------------------------------------------------
                        Private to this module
 ---------------------------------------------------------------------------*/
/**
 * This enum stores the status for each parsed line (internal use only).
 */
typedef enum _line_status_ {
    LINE_UNPROCESSED,
    LINE_ERROR,
    LINE_EMPTY,
    LINE_COMMENT,
    LINE_SECTION,
    LINE_VALUE
} line_status;

/*-------------------------------------------------------------------------*/
/**
  @brief    Default error callback for iniparser: wraps `fprintf(stderr, ...)`.
 */
/*--------------------------------------------------------------------------*/
static int default_error_callback(const char *format, ...) {
    int ret;
    va_list argptr;
    va_start(argptr, format);
    ret = vfprintf(stderr, format, argptr);
    va_end(argptr);
    return ret;
}

static int (*iniparser_error_callback)(const char *, ...) = default_error_callback;

/*-------------------------------------------------------------------------*/
/**
  @brief    Configure a function to receive the error messages.
  @param    errback  Function to call.

  By default, the error will be printed on stderr. If a null pointer is passed
  as errback the error callback will be switched back to default.
 */
/*--------------------------------------------------------------------------*/
void iniparser_set_error_callback(int (*errback)(const char *, ...)) {
    if (errback) {
        iniparser_error_callback = errback;
    } else {
        iniparser_error_callback = default_error_callback;
    }
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Get the string associated to a key
  @param    d       Dictionary to search
  @param    key     Key string to look for
  @param    def     Default value to return if key not found.
  @return   pointer to statically allocated character string

  This function queries a dictionary for a key. A key as read from an
  ini file is given as "section:key". If the key cannot be found,
  the pointer passed as 'def' is returned.
  The returned char pointer is pointing to a string allocated in
  the dictionary, do not free or modify it.
 */
/*--------------------------------------------------------------------------*/
const char *iniparser_getstring(const dictionary *d, const char *key, const char *def) {
    const char *lc_key;
    const char *sval;
    char tmp_str[ASCIILINESZ + 1];
    unsigned i;

    if (d == NULL || key == NULL)
        return def;

    if (key != NULL && tmp_str != NULL && sizeof(tmp_str) != 0) {
        i = 0;
        while (key[i] != '\0' && i < sizeof(tmp_str) - 1) {
            tmp_str[i] = (char) tolower((int) key[i]);
            i++;
        }
        tmp_str[i] = '\0';
        lc_key = tmp_str;
    }

    sval = dictionary_get(d, lc_key, def);
    return sval;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Get the string associated to a key, convert to an long int
  @param    d Dictionary to search
  @param    key Key string to look for
  @param    notfound Value to return in case of error
  @return   long integer

  This function queries a dictionary for a key. A key as read from an
  ini file is given as "section:key". If the key cannot be found,
  the notfound value is returned.

  Supported values for integers include the usual C notation
  so decimal, octal (starting with 0) and hexadecimal (starting with 0x)
  are supported. Examples:

  "42"      ->  42
  "042"     ->  34 (octal -> decimal)
  "0x42"    ->  66 (hexa  -> decimal)

  Warning: the conversion may overflow in various ways. Conversion is
  totally outsourced to strtol(), see the associated man page for overflow
  handling.

  Credits: Thanks to A. Becker for suggesting strtol()
 */
/*--------------------------------------------------------------------------*/
long int iniparser_getlongint(const dictionary *d, const char *key, long int notfound) {
    const char *str;
    const char *lc_key;
    const char *sval;
    char tmp_str[ASCIILINESZ + 1];
    unsigned i;

    if (d == NULL || key == NULL) {
        str = INI_INVALID_KEY;
    } else {
        if (key != NULL && tmp_str != NULL && sizeof(tmp_str) != 0) {
            i = 0;
            while (key[i] != '\0' && i < sizeof(tmp_str) - 1) {
                tmp_str[i] = (char) tolower((int) key[i]);
                i++;
            }
            tmp_str[i] = '\0';
            lc_key = tmp_str;
        }

        sval = dictionary_get(d, lc_key, INI_INVALID_KEY);
        str = sval;
    }

    if (str == NULL || str == INI_INVALID_KEY) return notfound;
    return strtol(str, NULL, 0);
}

int64_t iniparser_getint64(const dictionary *d, const char *key, int64_t notfound) {
    const char *str;
    const char *lc_key;
    const char *sval;
    char tmp_str[ASCIILINESZ + 1];
    unsigned i;

    if (d == NULL || key == NULL) {
        str = INI_INVALID_KEY;
    } else {
        if (key != NULL && tmp_str != NULL && sizeof(tmp_str) != 0) {
            i = 0;
            while (key[i] != '\0' && i < sizeof(tmp_str) - 1) {
                tmp_str[i] = (char) tolower((int) key[i]);
                i++;
            }
            tmp_str[i] = '\0';
            lc_key = tmp_str;
        }

        sval = dictionary_get(d, lc_key, INI_INVALID_KEY);
        str = sval;
    }
    if (str == NULL || str == INI_INVALID_KEY) return notfound;
    return strtoimax(str, NULL, 0);
}

uint64_t iniparser_getuint64(const dictionary *d, const char *key, uint64_t notfound) {
    const char *str;
    const char *lc_key;
    const char *sval;
    char tmp_str[ASCIILINESZ + 1];
    unsigned i;

    if (d == NULL || key == NULL) {
        str = INI_INVALID_KEY;
    } else {
        if (key != NULL && tmp_str != NULL && sizeof(tmp_str) != 0) {
            i = 0;
            while (key[i] != '\0' && i < sizeof(tmp_str) - 1) {
                tmp_str[i] = (char) tolower((int) key[i]);
                i++;
            }
            tmp_str[i] = '\0';
            lc_key = tmp_str;
        }

        sval = dictionary_get(d, lc_key, INI_INVALID_KEY);
        str = sval;
    }
    if (str == NULL || str == INI_INVALID_KEY) return notfound;
    return strtoumax(str, NULL, 0);
}


/*-------------------------------------------------------------------------*/
/**
  @brief    Get the string associated to a key, convert to an int
  @param    d Dictionary to search
  @param    key Key string to look for
  @param    notfound Value to return in case of error
  @return   integer

  This function queries a dictionary for a key. A key as read from an
  ini file is given as "section:key". If the key cannot be found,
  the notfound value is returned.

  Supported values for integers include the usual C notation
  so decimal, octal (starting with 0) and hexadecimal (starting with 0x)
  are supported. Examples:

  "42"      ->  42
  "042"     ->  34 (octal -> decimal)
  "0x42"    ->  66 (hexa  -> decimal)

  Warning: the conversion may overflow in various ways. Conversion is
  totally outsourced to strtol(), see the associated man page for overflow
  handling.

  Credits: Thanks to A. Becker for suggesting strtol()
 */
/*--------------------------------------------------------------------------*/
int iniparser_getint(const dictionary *d, const char *key, int notfound) {
    return (int) iniparser_getlongint(d, key, notfound);
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Get the string associated to a key, convert to a double
  @param    d Dictionary to search
  @param    key Key string to look for
  @param    notfound Value to return in case of error
  @return   double

  This function queries a dictionary for a key. A key as read from an
  ini file is given as "section:key". If the key cannot be found,
  the notfound value is returned.
 */
/*--------------------------------------------------------------------------*/
double iniparser_getdouble(const dictionary *d, const char *key, double notfound) {
    const char *str;
    const char *lc_key;
    const char *sval;
    char tmp_str[ASCIILINESZ + 1];
    unsigned i;

    if (d == NULL || key == NULL) {
        str = INI_INVALID_KEY;
    } else {
        if (key != NULL && tmp_str != NULL && sizeof(tmp_str) != 0) {
            i = 0;
            while (key[i] != '\0' && i < sizeof(tmp_str) - 1) {
                tmp_str[i] = (char) tolower((int) key[i]);
                i++;
            }
            tmp_str[i] = '\0';
            lc_key = tmp_str;
        }

        sval = dictionary_get(d, lc_key, INI_INVALID_KEY);
        str = sval;
    }
    if (str == NULL || str == INI_INVALID_KEY) return notfound;
    return atof(str);
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Get the string associated to a key, convert to a boolean
  @param    d Dictionary to search
  @param    key Key string to look for
  @param    notfound Value to return in case of error
  @return   integer

  This function queries a dictionary for a key. A key as read from an
  ini file is given as "section:key". If the key cannot be found,
  the notfound value is returned.

  A true boolean is found if one of the following is matched:

  - A string starting with 'y'
  - A string starting with 'Y'
  - A string starting with 't'
  - A string starting with 'T'
  - A string starting with '1'

  A false boolean is found if one of the following is matched:

  - A string starting with 'n'
  - A string starting with 'N'
  - A string starting with 'f'
  - A string starting with 'F'
  - A string starting with '0'

  The notfound value returned if no boolean is identified, does not
  necessarily have to be 0 or 1.
 */
/*--------------------------------------------------------------------------*/
int iniparser_getboolean(const dictionary *d, const char *key, int notfound) {
    int ret;
    const char *c;
    const char *lc_key;
    const char *sval;
    char tmp_str[ASCIILINESZ + 1];
    unsigned i;

    if (d == NULL || key == NULL) {
        c = INI_INVALID_KEY;
    } else {
        if (key != NULL && tmp_str != NULL && sizeof(tmp_str) != 0) {
            i = 0;
            while (key[i] != '\0' && i < sizeof(tmp_str) - 1) {
                tmp_str[i] = (char) tolower((int) key[i]);
                i++;
            }
            tmp_str[i] = '\0';
            lc_key = tmp_str;
        }

        sval = dictionary_get(d, lc_key, INI_INVALID_KEY);
        c = sval;
    }
    if (c == NULL || c == INI_INVALID_KEY) return notfound;
    if (c[0] == 'y' || c[0] == 'Y' || c[0] == '1' || c[0] == 't' || c[0] == 'T') {
        ret = 1;
    } else if (c[0] == 'n' || c[0] == 'N' || c[0] == '0' || c[0] == 'f' || c[0] == 'F') {
        ret = 0;
    } else {
        ret = notfound;
    }
    return ret;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Finds out if a given entry exists in a dictionary
  @param    ini     Dictionary to search
  @param    entry   Name of the entry to look for
  @return   integer 1 if entry exists, 0 otherwise

  Finds out if a given entry exists in the dictionary. Since sections
  are stored as keys with NULL associated values, this is the only way
  of querying for the presence of sections in a dictionary.
 */
/*--------------------------------------------------------------------------*/
int iniparser_find_entry(const dictionary *ini, const char *entry) {
    const char *getstring;
    int found = 0;
    const char *lc_key;
    const char *sval;
    char tmp_str[ASCIILINESZ + 1];
    unsigned i;

    if (ini == NULL || entry == NULL) {
        getstring = INI_INVALID_KEY;
    } else {
        if (entry != NULL && tmp_str != NULL && sizeof(tmp_str) != 0) {
            i = 0;
            while (entry[i] != '\0' && i < sizeof(tmp_str) - 1) {
                tmp_str[i] = (char) tolower((int) entry[i]);
                i++;
            }
            tmp_str[i] = '\0';
            lc_key = tmp_str;
        }

        sval = dictionary_get(ini, lc_key, INI_INVALID_KEY);
        getstring = sval;
    }
    if (getstring != INI_INVALID_KEY) {
        found = 1;
    }
    return found;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Set an entry in a dictionary.
  @param    ini     Dictionary to modify.
  @param    entry   Entry to modify (entry name)
  @param    val     New value to associate to the entry.
  @return   int 0 if Ok, -1 otherwise.

  If the given entry can be found in the dictionary, it is modified to
  contain the provided value. If it cannot be found, the entry is created.
  It is Ok to set val to NULL.
 */
/*--------------------------------------------------------------------------*/
int iniparser_set(dictionary *ini, const char *entry, const char *val) {
    char tmp_key[ASCIILINESZ + 1] = {0};
    char tmp_val[ASCIILINESZ + 1] = {0};
    size_t len;
    const char *strlwc;
    unsigned i;

    if (val) {
        len = strlen(val);
        len = len > ASCIILINESZ ? ASCIILINESZ : len;
        memcpy(tmp_val, val, len);
        val = tmp_val;
    }

    if (entry != NULL && tmp_key != NULL && sizeof(tmp_key) != 0) {
        i = 0;
        while (entry[i] != '\0' && i < sizeof(tmp_key) - 1) {
            tmp_key[i] = (char) tolower((int) entry[i]);
            i++;
        }
        tmp_key[i] = '\0';
        strlwc = tmp_key;
    }

    return dictionary_set(ini, strlwc, val);
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Delete an entry in a dictionary
  @param    ini     Dictionary to modify
  @param    entry   Entry to delete (entry name)
  @return   void

  If the given entry can be found, it is deleted from the dictionary.
 */
/*--------------------------------------------------------------------------*/
void iniparser_unset(dictionary *ini, const char *entry) {
    char tmp_str[ASCIILINESZ + 1];
    unsigned i;
    const char *strlwc;

    if (entry == NULL || tmp_str == NULL || sizeof(tmp_str) == 0) {
        i = 0;
        while (entry[i] != '\0' && i < sizeof(tmp_str) - 1) {
            tmp_str[i] = (char) tolower((int) entry[i]);
            i++;
        }
        tmp_str[i] = '\0';
        strlwc = tmp_str;
    }
    dictionary_unset(ini, strlwc);
}

static void parse_quoted_value(char *value, char quote) {
    char c;
    char *quoted, *t;
    int q = 0, v = 0;
    int esc = 0;
    size_t len;

    if (!value)
        return;

    len = strlen(value) + 1;
    t = (char *) malloc(len);
    if (t) {
        memcpy(t, value, len);
    }
    quoted = t;

    if (!quoted) {
        iniparser_error_callback("iniparser: memory allocation failure\n");
        goto end_of_value;
    }

    while ((c = quoted[q]) != '\0') {
        if (!esc) {
            if (c == '\\') {
                esc = 1;
                q++;
                continue;
            }

            if (c == quote) {
                goto end_of_value;
            }
        }
        esc = 0;
        value[v] = c;
        v++;
        q++;
    }
end_of_value:
    value[v] = '\0';
    free(quoted);
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Load a single line from an INI file
  @param    input_line  Input line, may be concatenated multi-line input
  @param    section     Output space to store section
  @param    key         Output space to store key
  @param    value       Output space to store value
  @return   line_status value
 */
/*--------------------------------------------------------------------------*/
static line_status iniparser_line(
    const char *input_line,
    char *section,
    char *key,
    char *value) {
    line_status sta;
    char *line = NULL, *t, *last, *dest;
    size_t len, str_len;
    int d_quote;
    unsigned i;

    if (input_line) {
        str_len = strlen(input_line) + 1;
        t = (char *) malloc(str_len);
        if (t) {
            memcpy(t, input_line, str_len);
        }
        line = t;
    }

    last = NULL;
    dest = line;

    if (line == NULL) {
        line = 0;
    } else {
        last = line + strlen(line);
        while (isspace((unsigned char) *line) && *line) line++;
        while (last > line) {
            if (!isspace((unsigned char) *(last - 1)))
                break ;
            last--;
        }
        *last = (char) 0;

        memmove(dest, line, last - line + 1);

        len = last - line;
    }

    sta = LINE_UNPROCESSED;
    if (len < 1) {
        /* Empty line */
        sta = LINE_EMPTY;
    } else if (line[0] == '#' || line[0] == ';') {
        /* Comment line */
        sta = LINE_COMMENT;
    } else if (line[0] == '[' && line[len - 1] == ']') {
        /* Section name without opening square bracket */
        sscanf(line, "[%[^\n]", section);
        len = strlen(section);
        /* Section name without closing square bracket */
        if (section[len - 1] == ']') {
            section[len - 1] = '\0';
        }

        last = NULL;
        dest = section;

        if (section == NULL) {
            section = 0;
        } else {
            last = section + strlen(section);
            while (isspace((unsigned char) *section) && *section) section++;
            while (last > section) {
                if (!isspace((unsigned char) *(last - 1)))
                    break ;
                last--;
            }
            *last = (char) 0;

            memmove(dest, section, last - section + 1);
        }

        if (section != NULL && len != 0) {
            i = 0;
            while (section[i] != '\0' && i < len - 1) {
                section[i] = (char) tolower((int) section[i]);
                i++;
            }
            section[i] = '\0';
        }
        sta = LINE_SECTION;
    } else if ((d_quote = sscanf(line, "%[^=] = \"%[^\n]\"", key, value)) == 2
               || sscanf(line, "%[^=] = '%[^\n]'", key, value) == 2) {
        /* Usual key=value with quotes, with or without comments */
        last = NULL;
        dest = key;

        if (key != NULL) {
            last = key + strlen(key);
            while (isspace((unsigned char) *key) && *key) key++;
            while (last > key) {
                if (!isspace((unsigned char) *(last - 1)))
                    break ;
                last--;
            }
            *last = (char) 0;

            memmove(dest, key, last - key + 1);
        }

        if (key != NULL && len != 0) {
            i = 0;
            while (key[i] != '\0' && i < len - 1) {
                key[i] = (char) tolower((int) key[i]);
                i++;
            }
            key[i] = '\0';
        }
        if (d_quote == 2)
            parse_quoted_value(value, '"');
        else
            parse_quoted_value(value, '\'');
        /* Don't strip spaces from values surrounded with quotes */
        sta = LINE_VALUE;
    } else if (sscanf(line, "%[^=] = %[^;#]", key, value) == 2) {
        /* Usual key=value without quotes, with or without comments */
        last = NULL;
        dest = key;

        if (key != NULL) {
            last = key + strlen(key);
            while (isspace((unsigned char) *key) && *key) key++;
            while (last > key) {
                if (!isspace((unsigned char) *(last - 1)))
                    break ;
                last--;
            }
            *last = (char) 0;

            memmove(dest, key, last - key + 1);
        }

        if (key != NULL && len != 0) {
            i = 0;
            while (key[i] != '\0' && i < len - 1) {
                key[i] = (char) tolower((int) key[i]);
                i++;
            }
            key[i] = '\0';
        }
        last = NULL;
        dest = value;

        if (value != NULL) {
            last = value + strlen(key);
            while (isspace((unsigned char) *value) && *value) value++;
            while (last > value) {
                if (!isspace((unsigned char) *(last - 1)))
                    break ;
                last--;
            }
            *last = (char) 0;

            memmove(dest, value, last - value + 1);
        }
        /*
         * sscanf cannot handle '' or "" as empty values
         * this is done here
         */
        if (!strcmp(value, "\"\"") || (!strcmp(value, "''"))) {
            value[0] = 0;
        }
        sta = LINE_VALUE;
    } else if (sscanf(line, "%[^=] = %[;#]", key, value) == 2
               || sscanf(line, "%[^=] %[=]", key, value) == 2) {
        /*
         * Special cases:
         * key=
         * key=;
         * key=#
         */
        last = NULL;
        dest = key;

        if (key != NULL) {
            last = key + strlen(key);
            while (isspace((unsigned char) *key) && *key) key++;
            while (last > key) {
                if (!isspace((unsigned char) *(last - 1)))
                    break ;
                last--;
            }
            *last = (char) 0;

            memmove(dest, key, last - key + 1);
        }

        if (key != NULL && len != 0) {
            i = 0;
            while (key[i] != '\0' && i < len - 1) {
                key[i] = (char) tolower((int) key[i]);
                i++;
            }
            key[i] = '\0';
        }
        value[0] = 0;
        sta = LINE_VALUE;
    } else {
        /* Generate syntax error */
        sta = LINE_ERROR;
    }

    free(line);
    return sta;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Parse an ini file and return an allocated dictionary object
  @param    in File to read.
  @param    ininame Name of the ini file to read (only used for nicer error messages)
  @return   Pointer to newly allocated dictionary

  This is the parser for ini files. This function is called, providing
  the file to be read. It returns a dictionary object that should not
  be accessed directly, but through accessor functions instead.

  The returned dictionary must be freed using iniparser_freedict().
 */
/*--------------------------------------------------------------------------*/
dictionary *iniparser_load_file(FILE *in, const char *ininame) {
    char line[ASCIILINESZ + 1];
    char section[ASCIILINESZ + 1];
    char key[ASCIILINESZ + 1];
    char tmp[(ASCIILINESZ * 2) + 2];
    char val[ASCIILINESZ + 1];

    int last = 0;
    int len;
    int lineno = 0;
    int errs = 0;
    int mem_err = 0;

    dictionary *dict;

    dict = dictionary_new(0);
    if (!dict) {
        return NULL;
    }

    memset(line, 0, ASCIILINESZ);
    memset(section, 0, ASCIILINESZ);
    memset(key, 0, ASCIILINESZ);
    memset(val, 0, ASCIILINESZ);
    last = 0;

    while (fgets(line + last, ASCIILINESZ - last, in) != NULL) {
        lineno++;
        len = (int) strlen(line) - 1;
        if (len <= 0)
            continue;
        /* Safety check against buffer overflows */
        if (line[len] != '\n' && !feof(in)) {
            iniparser_error_callback(
                "iniparser: input line too long in %s (%d)\n",
                ininame,
                lineno);
            dictionary_del(dict);
            return NULL;
        }
        /* Get rid of \n and spaces at end of line */
        while ((len >= 0) &&
               ((line[len] == '\n') || (isspace((unsigned char) line[len])))) {
            line[len] = 0;
            len--;
        }
        if (len < 0) {
            /* Line was entirely \n and/or spaces */
            len = 0;
        }
        /* Detect multi-line */
        if (line[len] == '\\') {
            /* Multi-line value */
            last = len;
            continue ;
        } else {
            last = 0;
        }
        switch (iniparser_line(line, section, key, val)) {
            case LINE_EMPTY:
            case LINE_COMMENT:
                break ;

            case LINE_SECTION:
                mem_err = dictionary_set(dict, section, NULL);
                break ;

            case LINE_VALUE:
                sprintf(tmp, "%s:%s", section, key);
                mem_err = dictionary_set(dict, tmp, val);
                break ;

            case LINE_ERROR:
                iniparser_error_callback(
                    "iniparser: syntax error in %s (%d):\n-> %s\n",
                    ininame,
                    lineno,
                    line);
                errs++;
                break;

            default:
                break ;
        }
        memset(line, 0, ASCIILINESZ);
        last = 0;
        if (mem_err < 0) {
            iniparser_error_callback("iniparser: memory allocation failure\n");
            break ;
        }
    }
    if (errs) {
        dictionary_del(dict);
        dict = NULL;
    }
    return dict;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Parse an ini file and return an allocated dictionary object
  @param    ininame Name of the ini file to read.
  @return   Pointer to newly allocated dictionary

  This is the parser for ini files. This function is called, providing
  the name of the file to be read. It returns a dictionary object that
  should not be accessed directly, but through accessor functions
  instead.

  The returned dictionary must be freed using iniparser_freedict().
 */
/*--------------------------------------------------------------------------*/
dictionary *iniparser_load(const char *ininame) {
    FILE *in;
    dictionary *dict;

    if ((in = fopen(ininame, "r")) == NULL) {
        iniparser_error_callback("iniparser: cannot open %s\n", ininame);
        return NULL;
    }

    dict = iniparser_load_file(in, ininame);
    fclose(in);

    return dict;
}


/*-------------------------------------------------------------------------*/
/**
  @brief    Free all memory associated to an ini dictionary
  @param    d Dictionary to free
  @return   void

  Free all memory associated to an ini dictionary.
  It is mandatory to call this function before the dictionary object
  gets out of the current context.
 */
/*--------------------------------------------------------------------------*/
void iniparser_freedict(dictionary *d) {
    dictionary_del(d);
}

int main() {
    dictionary *ini = NULL;
    FILE *fp = NULL;
    const char *filename = "example.ini";
    const char *dump_filename = "dumped_example.ini";

    iniparser_set_error_callback(NULL);
    printf("Error callback set to default.\n\n");

    printf("--- Testing iniparser_load ---\n");
    ini = iniparser_load(filename);
    if (ini == NULL) {
        fprintf(stderr, "Failed to load %s\n", filename);
        return 1;
    }
    printf("Successfully loaded %s\n\n", filename);

    printf("--- Testing iniparser_getstring ---\n");
    const char *stringValue = iniparser_getstring(ini, "section1:stringvalue", "NOT_FOUND");
    printf("Section1:StringValue = %s \n\n", stringValue);

    printf("--- Testing iniparser_getint ---\n");
    int nonExistentInt = iniparser_getint(ini, "section1:nonexistent_int", 999);
    printf("Section1:NonExistentInt = %d \n\n", nonExistentInt);

    printf("--- Testing iniparser_getlongint ---\n");
    long int longIntValue = iniparser_getlongint(ini, "section1:longintvalue", -1L);
    printf("Section1:LongIntValue = %ld \n\n", longIntValue);

    printf("--- Testing iniparser_getint64 ---\n");
    // Note: strtoimax handles signed values
    int64_t int64Value = iniparser_getint64(ini, "section1:intvalue", -1LL);
    printf("Section1:IntValue (as int64) = %lld \n\n", int64Value);

    printf("--- Testing iniparser_getuint64 ---\n");
    uint64_t uint64Value = iniparser_getuint64(ini, "section1:uint64value", 0ULL);
    printf("Section1:UInt64Value = %llu \n\n", uint64Value);

    printf("--- Testing iniparser_getdouble ---\n");
    double nonExistentDouble = iniparser_getdouble(ini, "section1:nonexistent_double", -1.0);
    printf("Section1:NonExistentDouble = %f \n\n", nonExistentDouble);

    printf("--- Testing iniparser_getboolean ---\n");
    int booleanTrue = iniparser_getboolean(ini, "section1:booleantrue", -1);
    printf("Section1:BooleanTrue = %d \n", booleanTrue);
    const char *escapedString = iniparser_getstring(ini, "section1:escapedstring", "Error");
    printf("Section1:EscapedString = %s \n", escapedString);
    const char *quotedEmpty = iniparser_getstring(ini, "section1:quotedempty", "Error");
    printf("Section1:QuotedEmpty = \"%s\" \n\n", quotedEmpty);

    printf("--- Simulating iniparser_line (Internal) ---\n");
    char test_section[ASCIILINESZ + 1];
    char test_key[ASCIILINESZ + 1];
    char test_value[ASCIILINESZ + 1];
    line_status status;

    printf("Parsing line: \"another_key = value_without_quotes ; comment\"\n");
    memset(test_section, 0, sizeof(test_section));
    memset(test_key, 0, sizeof(test_key));
    memset(test_value, 0, sizeof(test_value));
    status = iniparser_line("another_key = value_without_quotes ; comment", test_section, test_key, test_value);
    printf("  Status: %d (LINE_VALUE=%d), Section: '%s', Key: '%s', Value: '%s'\n",
           status, LINE_VALUE, test_section, test_key, test_value);
    printf("\n");


    // 16. iniparser_freedict
    printf("--- Testing iniparser_freedict ---\n");
    iniparser_freedict(ini);
    ini = NULL; // Good practice to set to NULL after freeing
    printf("Dictionary freed.\n\n");

    printf("All tests completed. Check '%s' for dumped content.\n", dump_filename);

    return 0;
}
```

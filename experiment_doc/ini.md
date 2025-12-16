# ini

この資料では、外部ファイルで定義されている型、関数を説明します。

## 型

### dictionary

ディクショナリを表現する型です。

**宣言・定義**

- 定義場所：`dictionary_del_func.h`

### FILE

ファイル入出力を制御する型です。ファイルのオープン、読み書き、クローズなどの操作に使用されます。

宣言・定義

- 宣言場所：`stdio.h`

### size_t

オブジェクトのサイズを表現するのに十分な大きさが保証した、符号なし整数型です。

宣言・定義

- 定義場所：`stdlib.h`

### va_list

可変個引数リストを表現する型です。

宣言・定義

- 定義場所：`stdarg.h`

## 関数

### dictionary_del（ディクショナリのメモリ解放）

```c
void dictionary_del(dictionary * vd);
```

引数`vd`に指定したディクショナリのメモリを解放します。

**宣言・定義**

- 宣言場所：`dictionary_del_func.h`

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

### fgets（ファイルの文字読み取り）

```c
char *fgets(char *s, int n, FILE *stream);
```

引数`stream`で指定したファイルから文字列を読み取り、引数`s` で指定したバッファに格納します。読み取る文字列は、引数`n`で指定した文字数 または 改行文字、`EOF` まで読み取ります。

- 引数：
    - `s` ：読み取った文字列を格納するバッファ
    - `n` ：読み取る文字の最大文字数
    - `stream`：文字を読み取るファイルのポインタ
- 戻り値：
    - 読み取りに成功した場合：引数`s` で指定したバッファのポインタ
    - 読み取りに失敗した場合：`NULL`

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

### memcpy（メモリ領域のコピー）

```c
void *memcpy(void *dest, const void *src, size_t n);
```

引数`src` で指定したメモリ領域の、先頭から引数`n`で指定したバイト数だけ、引数`dest` で指定したメモリ領域にコピーします。コピー先、コピー元のメモリ領域が重なる場合は、コピーが正しくされない可能性があります。

- 引数：
    - `dest` ：コピー先のメモリ領域のポインタ
    - `src` ：コピー元のメモリ領域のポインタ
    - `n` ：コピーするバイト数
- 戻り値：
    - 引数`dest` で指定したメモリ領域のポインタ

**宣言・定義**

- 宣言場所：`string.h`

### memmove（メモリ領域のコピー）

```c
void *memmove(void *dest, const void *src, size_t n);
```

引数`src` で指定したメモリ領域の、先頭から引数`n`で指定したバイト数だけ、引数`dest` で指定したメモリ領域にコピーします。コピー先、コピー元のメモリ領域が重なる場合も正しくコピーされます。

- 引数：
    - `dest` ：コピー先のメモリ領域のポインタ
    - `src` ：コピー元のメモリ領域のポインタ
    - `n` ：コピーするバイト数
- 戻り値：
    - 引数`dest` で指定したメモリ領域のポインタ

**宣言・定義**

- 宣言場所：`string.h`

### sprintf（書式付き文字列の書き込み）

```c
int sprintf(char *str, const char *fmt, ... );
```

引数`fmt`で指定した文字列を、引数`str`で指定したバッファに格納します。引数`fmt`で指定した文字列が書式を含む場合、その書式に第三引数以降で指定した引数の値を格納した文字列を、引数`str` に格納します。

- 引数：
    - `str`：書式付き文字列を格納するバッファ
    - `fmt` ：書式付き文字列
    - `...`：引数`fmt`が含む書式に格納する引数（可変長引数）
- 戻り値：
    - 書き込みに成功した場合：引数`str` に格納した文字数（終端文字`\0` は含まない）
    - 書き込みに失敗した場合：`EOF`

**宣言・定義**

- 宣言場所：`stdio.h`

### sscanf（文字列読み取り）

```c
int sscanf(const char *str, const char *fmt, ... );
```

引数`str`で指定した文字列から、引数`fmt`で指定した書式に一致するデータを読み取り、第三引数以降で指定した引数に格納します。

- 引数：
    - `str`：読み取り対象の文字列
    - `fmt` ：書式付き文字列
    - `...`：抽出したデータを格納する変数（可変長引数）
- 戻り値：
    - 読み取りに成功した場合：読み取りしたデータの個数
    - 読み取りに失敗した場合：`EOF`

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

### va_end（可変長引数リストの初期化）

```c
void va_start(va_list argptr, param);
```

引数`argptr` で指定した可変長引数リストへのアクセスを初期化します。

- 引数：
    - `argptr` ：初期化する可変長引数リスト
    - `param` ：可変長引数リストの直前の引数
- 戻り値：無し

**宣言・定義**

- 宣言場所：`stdarg.h`

### va_start（可変長引数リストのメモリ解放）

```c
void va_end(va_list argptr);
```

引数`argptr` で指定した可変長引数リストのメモリを解放します。

- 引数：
    - `argptr` ：メモリ解放する可変長引数リスト
- 戻り値：無し

**宣言・定義**

- 宣言場所：`stdarg.h`

### vfprintf（可変長引数のファイル出力）

```c
int vfprintf(FILE *stream, const char *format, va_list argptr);
```

引数`argptr` で指定した可変長引数リストを、引数`format` で指定した書式付き文字列に従い、引数`stream` で指定したファイルに出力します。

- 引数：
    - `stream` ：出力するファイルのポインタ
        - `stderr` ：標準エラー出力
    - `format` ：書式付き文字列
    - `argptr` ：可変長引数リスト
- 戻り値：
    - ファイルへの出力が成功した場合：出力した文字数
    - ファイルへの出力が失敗した場合：`-1`

**宣言・定義**

- 宣言場所：`stdio.h`

```c++
/*---------------------------- Includes ------------------------------------*/
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "dictionary_del_func.h"

/*---------------------------- Defines -------------------------------------*/
#define ASCIILINESZ         (32)
#define INI_INVALID_KEY     ((char*)-1)
#define DICTMINSZ   3

/*---------------------------------------------------------------------------
                        Private to this module
 ---------------------------------------------------------------------------*/
typedef enum _line_status_ {
    LINE_UNPROCESSED,
    LINE_ERROR,
    LINE_EMPTY,
    LINE_COMMENT,
    LINE_SECTION,
    LINE_VALUE
} line_status;

static const char *strlwc(const char *in, char *out, unsigned len) {
    unsigned i;

    if (in == NULL || out == NULL || len == 0) return NULL;
    i = 0;
    while (in[i] != '\0' && i < len - 1) {
        out[i] = (char) tolower((int) in[i]);
        i++;
    }
    out[i] = '\0';
    return out;
}

static char *xstrdup(const char *s) {
    char *t;
    size_t len;
    if (!s)
        return NULL;

    len = strlen(s) + 1;
    t = (char *) malloc(len);
    if (t) {
        memcpy(t, s, len);
    }
    return t;
}

static unsigned strstrip(char *s) {
    char *last = NULL;
    char *dest = s;

    if (s == NULL) return 0;

    last = s + strlen(s);
    while (isspace((unsigned char) *s) && *s) s++;
    while (last > s) {
        if (!isspace((unsigned char) *(last - 1)))
            break ;
        last--;
    }
    *last = (char) 0;

    memmove(dest, s, last - s + 1);
    return last - s;
}

dictionary *dictionary_new(size_t size) {
    dictionary *d;

    /* If no size was specified, allocate space for DICTMINSZ */
    if (size < DICTMINSZ) size = DICTMINSZ;

    d = (dictionary *) calloc(1, sizeof *d);

    if (d) {
        d->size = size;
        d->val = (char **) calloc(size, sizeof *d->val);
        d->key = (char **) calloc(size, sizeof *d->key);
        if (!d->val || !d->key) {
            free((void *) d->val);
            free((void *) d->key);
            free(d);
            d = NULL;
        }
    }
    return d;
}

const char *dictionary_get(const dictionary *d, const char *key, const char *def) {
    size_t i;

    if (d == NULL || key == NULL)
        return def;

    for (i = 0; i < d->size; i++) {
        if (d->key[i] == NULL)
            continue ;
        if (!strcmp(key, d->key[i])) {
            return d->val[i];
        }
    }
    return def;
}

int dictionary_set(dictionary *d, const char *key, const char *val) {
    size_t i;

    if (d == NULL || key == NULL) return -1;

    if (d->n > 0) {
        for (i = 0; i < d->size; i++) {
            if (d->key[i] == NULL)
                continue ;
            if (!strcmp(key, d->key[i])) {
                if (d->val[i] != NULL)
                    free(d->val[i]);

                d->val[i] = xstrdup(val);
                return 0;
            }
        }
    }
    if (d->n == d->size) {
        return -1;
    }

    for (i = d->n; d->key[i];) {
        if (++i == d->size) i = 0;
    }
    d->key[i] = xstrdup(key);
    d->val[i] = xstrdup(val);

    d->n++;
    return 0;
}

static int default_error_callback(const char *format, ...) {
    int ret;
    va_list argptr;
    va_start(argptr, format);
    ret = vfprintf(stderr, format, argptr);
    va_end(argptr);
    return ret;
}

static int (*iniparser_error_callback)(const char *, ...) = default_error_callback;

void iniparser_set_error_callback(int (*errback)(const char *, ...)) {
    if (errback) {
        iniparser_error_callback = errback;
    } else {
        iniparser_error_callback = default_error_callback;
    }
}

const char *iniparser_getstring(const dictionary *d, const char *key, const char *def) {
    const char *lc_key;
    const char *sval;
    char tmp_str[ASCIILINESZ + 1];

    if (d == NULL || key == NULL)
        return def;

    lc_key = strlwc(key, tmp_str, sizeof(tmp_str));
    sval = dictionary_get(d, lc_key, def);
    return sval;
}

void iniparser_freedict(dictionary *d) {
    dictionary_del(d);
}


static void parse_quoted_value(char *value, char quote) {
    char c;
    char *quoted;
    int q = 0, v = 0;
    int esc = 0;

    if (!value)
        return;

    quoted = xstrdup(value);

    if (!quoted) {
        iniparser_error_callback("iniparser: memory allocation failure\n");
        value[v] = '\0';
        return;
    }

    while ((c = quoted[q]) != '\0') {
        if (!esc) {
            if (c == '\\') {
                esc = 1;
                q++;
                continue;
            }

            if (c == quote) {
                break;
            }
        }
        esc = 0;
        value[v] = c;
        v++;
        q++;
    }
    value[v] = '\0';
    free(quoted);
}

static line_status iniparser_line(
    const char *input_line,
    char *section,
    char *key,
    char *value) {
    line_status sta;
    char *line = NULL;
    size_t len;
    int d_quote;

    line = xstrdup(input_line);
    len = strstrip(line);

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
        strstrip(section);
        strlwc(section, section, len);
        sta = LINE_SECTION;
    } else if ((d_quote = sscanf(line, "%[^=] = \"%[^\n]\"", key, value)) == 2
               || sscanf(line, "%[^=] = '%[^\n]'", key, value) == 2) {
        /* Usual key=value with quotes, with or without comments */
        strstrip(key);
        strlwc(key, key, len);
        if (d_quote == 2)
            parse_quoted_value(value, '"');
        else
            parse_quoted_value(value, '\'');
        /* Don't strip spaces from values surrounded with quotes */
        sta = LINE_VALUE;
    } else if (sscanf(line, "%[^=] = %[^;#]", key, value) == 2) {
        /* Usual key=value without quotes, with or without comments */
        strstrip(key);
        strlwc(key, key, len);
        strstrip(value);
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
        strstrip(key);
        strlwc(key, key, len);
        value[0] = 0;
        sta = LINE_VALUE;
    } else {
        /* Generate syntax error */
        sta = LINE_ERROR;
    }

    free(line);
    return sta;
}

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

    last = 0;

    while (fgets(line + last, ASCIILINESZ - last, in) != NULL) {
        lineno++;
        len = (int) strlen(line) - 1;
        if (len <= 0)
            continue;
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
        last = 0;
        if (mem_err < 0) {
            iniparser_error_callback("iniparser: memory allocation failure\n");
            break ;
        }
    }
    if (errs) {
        iniparser_freedict(dict);
        dict = NULL;
    }
    return dict;
}

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

int main() {
    dictionary *ini1, *ini2, *ini3, *ini4, *ini5;
    char *value1, *value2;

    iniparser_set_error_callback(NULL);

    iniparser_load("example1.ini");

    iniparser_load("example2.ini");

    ini1 = iniparser_load("example3.ini");
    value1 = iniparser_getstring(ini1, "section1:key3", "NOT_FOUND");

    ini2 = iniparser_load("example4.ini");
    value2 = iniparser_getstring(ini2, "section1:key1", "NOT_FOUND");

    ini3 = iniparser_load("example5.ini");

    ini4 = iniparser_load("example6.ini");

    ini5 = iniparser_load("example7.ini");

    iniparser_freedict(ini1);
    ini1 = NULL;
    iniparser_freedict(ini2);
    ini2 = NULL;
    iniparser_freedict(ini3);
    ini3 = NULL;
    iniparser_freedict(ini4);
    ini4 = NULL;
    iniparser_freedict(ini5);
    ini5 = NULL;

    return 0;
}

```

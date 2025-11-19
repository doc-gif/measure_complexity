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
#define DICTMINSZ   128

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

static char *duplicate_string(const char *s) {
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

static void trim_string(char *str) {
    char *start, *end;

    if (str == NULL) return;

    start = str;
    while (isspace((unsigned char)*start)) {
        start++;
    }

    end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }
    *end = '\0';

    memmove(str, start, end - start + 1);
}

static void string_to_lower(char *str) {
    if (str == NULL) return;

    while (*str) {
        *str = (char)tolower((unsigned char)*str);
        str++;
    }
}

dictionary *dictionary_new(size_t size) {
    dictionary *d;

    if (size < DICTMINSZ) size = DICTMINSZ;

    d = (dictionary *) calloc(1, sizeof *d);

    if (d) {
        d->size = size;
        d->val = (char **) calloc(size, sizeof *d->val);
        d->key = (char **) calloc(size, sizeof *d->key);
        if (!d->val || !d->key) {
            free(d->val);
            free(d->key);
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
            if (d->key[i] != NULL && strcmp(key, d->key[i]) == 0) {
                free(d->val[i]);
                d->val[i] = duplicate_string(val);
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

    d->key[i] = duplicate_string(key);
    d->val[i] = duplicate_string(val);

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
    unsigned i;

    if (d == NULL || key == NULL)
        return def;

    i = 0;
    while (key[i] != '\0' && i < sizeof(tmp_str) - 1) {
        tmp_str[i] = (char) tolower((int) key[i]);
        i++;
    }
    tmp_str[i] = '\0';
    lc_key = tmp_str;

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

    quoted = duplicate_string(value);
    if (!quoted) {
        iniparser_error_callback("iniparser: memory allocation failure\n");
        value[0] = '\0';
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
    char *line;
    size_t len;
    int d_quote;

    if (input_line == NULL) {
        return LINE_EMPTY;
    }

    line = duplicate_string(input_line);
    trim_string(line);
    len = strlen(line);

    if (len < 1) {
        sta = LINE_EMPTY;
    } else if (line[0] == '#' || line[0] == ';') {
        sta = LINE_COMMENT;
    } else if (line[0] == '[' && line[len - 1] == ']') {
        sscanf(line, "[%[^\n]", section);
        len = strlen(section);
        if (len > 0 && section[len - 1] == ']') {
            section[len - 1] = '\0';
        }
        trim_string(section);
        string_to_lower(section);
        sta = LINE_SECTION;
    } else if ((d_quote = sscanf(line, "%[^=] = \"%[^\n]\"", key, value)) == 2
               || sscanf(line, "%[^=] = '%[^\n]'", key, value) == 2) {
        trim_string(key);
        string_to_lower(key);
        if (d_quote == 2)
            parse_quoted_value(value, '"');
        else
            parse_quoted_value(value, '\'');
        sta = LINE_VALUE;
    } else if (sscanf(line, "%[^=] = %[^;#]", key, value) == 2) {
        trim_string(key);
        string_to_lower(key);
        trim_string(value);
        if (!strcmp(value, "\"\"") || (!strcmp(value, "''"))) {
            value[0] = 0;
        }
        sta = LINE_VALUE;
    } else if (sscanf(line, "%[^=] = %[;#]", key, value) == 2
               || sscanf(line, "%[^=] %[=]", key, value) == 2) {
        trim_string(key);
        string_to_lower(key);
        value[0] = 0;
        sta = LINE_VALUE;
    } else {
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
        while ((len >= 0) &&
               ((line[len] == '\n') || (isspace((unsigned char) line[len])))) {
            line[len] = 0;
            len--;
        }
        if (len < 0) {
            len = 0;
        }
        if (line[len] == '\\') {
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
    dictionary *ini1, *ini2, *ini3, *ini4, *ini5, *ini6, *ini7, *ini8;
    char *value1, *value2, *value3, *value4, *value5, *value6;
    int64_t value7;

    iniparser_set_error_callback(NULL);

    ini1 = iniparser_load("example1.ini");

    ini2 = iniparser_load("example2.ini");
    value1 = iniparser_getstring(ini2, ":key1", "NOT_FOUND");

    ini3 = iniparser_load("example3.ini");
    value2 = iniparser_getstring(ini3, ":key1", "NOT_FOUND");
    value3 = iniparser_getstring(ini3, ":key2", "NOT_FOUND");

    ini4 = iniparser_load("example4.ini");
    value4 = iniparser_getstring(ini4, "section1:key1", "NOT_FOUND");

    ini5 = iniparser_load("example5.ini");
    value5 = iniparser_getstring(ini5, "section1:key2", "NOT_FOUND");

    ini6 = iniparser_load("example6.ini");
    value6 = iniparser_getstring(ini6, ":key1", "NOT_FOUND");

    ini7 = iniparser_load("example7.ini");

    ini8 = iniparser_load("example8.ini");

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
    iniparser_freedict(ini6);
    ini6 = NULL;
    iniparser_freedict(ini7);
    ini7 = NULL;

    return 0;
}
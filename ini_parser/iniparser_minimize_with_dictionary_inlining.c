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
    size_t i, str_len;
    char *t;

    if (d == NULL || key == NULL) return -1;

    if (d->n > 0) {
        for (i = 0; i < d->size; i++) {
            if (d->key[i] == NULL)
                continue ;
            if (!strcmp(key, d->key[i])) {
                if (d->val[i] != NULL)
                    free(d->val[i]);

                if (val) {
                    str_len = strlen(val) + 1;
                    t = (char *) malloc(str_len);
                    if (t) {
                        memcpy(t, val, str_len);
                    }
                    d->val[i] = t;
                } else {
                    d->val[i] = NULL;
                }
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

    str_len = strlen(key) + 1;
    t = (char *) malloc(str_len);
    if (t) {
        memcpy(t, key, str_len);
    }
    d->key[i] = t;

    if (val) {
        str_len = strlen(val) + 1;
        t = (char *) malloc(str_len);
        if (t) {
            memcpy(t, val, str_len);
        }
        d->val[i] = t;
    } else {
        d->val[i] = NULL;
    }

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
    } else {
        line = NULL;
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

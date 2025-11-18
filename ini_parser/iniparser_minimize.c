/*---------------------------- Includes ------------------------------------*/
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "iniparser.h"

/*---------------------------- Defines -------------------------------------*/
#define ASCIILINESZ         (32)
#define INI_INVALID_KEY     ((char*)-1)

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

long int iniparser_getlongint(const dictionary *d, const char *key, long int notfound) {
    const char *str;

    str = iniparser_getstring(d, key, INI_INVALID_KEY);
    if (str == NULL || str == INI_INVALID_KEY) return notfound;
    return strtol(str, NULL, 0);
}

int64_t iniparser_getint64(const dictionary *d, const char *key, int64_t notfound) {
    const char *str;

    str = iniparser_getstring(d, key, INI_INVALID_KEY);
    if (str == NULL || str == INI_INVALID_KEY) return notfound;
    return strtoimax(str, NULL, 0);
}

uint64_t iniparser_getuint64(const dictionary *d, const char *key, uint64_t notfound) {
    const char *str;

    str = iniparser_getstring(d, key, INI_INVALID_KEY);
    if (str == NULL || str == INI_INVALID_KEY) return notfound;
    return strtoumax(str, NULL, 0);
}

int iniparser_getint(const dictionary *d, const char *key, int notfound) {
    return (int) iniparser_getlongint(d, key, notfound);
}

double iniparser_getdouble(const dictionary *d, const char *key, double notfound) {
    const char *str;

    str = iniparser_getstring(d, key, INI_INVALID_KEY);
    if (str == NULL || str == INI_INVALID_KEY) return notfound;
    return atof(str);
}

int iniparser_getboolean(const dictionary *d, const char *key, int notfound) {
    int ret;
    const char *c;

    c = iniparser_getstring(d, key, INI_INVALID_KEY);
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
        dictionary_del(dict);
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


void iniparser_freedict(dictionary *d) {
    dictionary_del(d);
}

int main() {
    dictionary *ini1, *ini2, *ini3, *ini4, *ini5, *ini6, *ini7, *ini8;
    char *value1, *value2, *value3, *value4;
    double value5;
    int value6;
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
    value5 = iniparser_getdouble(ini5, "section1:key2", -1.0);

    ini6 = iniparser_load("example6.ini");
    value6 = iniparser_getboolean(ini6, ":key1", -1);
    
    ini7 = iniparser_load("example7.ini");
    value7 = iniparser_getint64(ini7, ":key1", -1);

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

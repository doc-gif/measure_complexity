/*---------------------------- Includes ------------------------------------*/
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "iniparser.h"

/*---------------------------- Defines -------------------------------------*/
#define ASCIILINESZ         (1024)
#define INI_KEY_NOT_FOUND   ((char*)-1)

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

/* Forward declarations for static functions */
static void str_trim(char *s);
static void str_tolower(char *s);
static void string_to_lowercase_copy(char *dest, const char *src, size_t size);
static void parse_quoted_value(char *value, char quote);
static line_status iniparser_line(const char *input_line, char *section, char *key, char *value);

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

/*---------------------------------------------------------------------------
                        Static Helper Functions
 ---------------------------------------------------------------------------*/

static void str_trim(char *s) {
    if (s == NULL) return;
    char *start = s;
    while (isspace((unsigned char)*start)) {
        start++;
    }

    char *end = s + strlen(s);
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }
    *end = '\0';

    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
}

static void str_tolower(char *s) {
    if (s == NULL) return;
    for ( ; *s; ++s) {
        *s = (char)tolower((unsigned char)*s);
    }
}

static void string_to_lowercase_copy(char *dest, const char *src, size_t size) {
    size_t i = 0;
    if (dest == NULL || src == NULL || size == 0) return;
    while (src[i] != '\0' && i < size - 1) {
        dest[i] = (char)tolower((unsigned char)src[i]);
        i++;
    }
    dest[i] = '\0';
}


/*---------------------------------------------------------------------------
                      Public API Getters
 ---------------------------------------------------------------------------*/

const char *iniparser_getstring(const dictionary *d, const char *key, const char *def) {
    char lc_key[ASCIILINESZ + 1];

    if (d == NULL || key == NULL)
        return def;

    string_to_lowercase_copy(lc_key, key, sizeof(lc_key));
    return dictionary_get(d, lc_key, def);
}

long int iniparser_getlongint(const dictionary *d, const char *key, long int notfound) {
    const char *sval;
    char lc_key[ASCIILINESZ + 1];

    if (d == NULL || key == NULL) return notfound;

    string_to_lowercase_copy(lc_key, key, sizeof(lc_key));
    sval = dictionary_get(d, lc_key, INI_KEY_NOT_FOUND);

    if (sval == INI_KEY_NOT_FOUND) return notfound;
    return strtol(sval, NULL, 0);
}

int64_t iniparser_getint64(const dictionary *d, const char *key, int64_t notfound) {
    const char *sval;
    char lc_key[ASCIILINESZ + 1];

    if (d == NULL || key == NULL) return notfound;

    string_to_lowercase_copy(lc_key, key, sizeof(lc_key));
    sval = dictionary_get(d, lc_key, INI_KEY_NOT_FOUND);

    if (sval == INI_KEY_NOT_FOUND) return notfound;
    return strtoimax(sval, NULL, 0);
}

uint64_t iniparser_getuint64(const dictionary *d, const char *key, uint64_t notfound) {
    const char *sval;
    char lc_key[ASCIILINESZ + 1];

    if (d == NULL || key == NULL) return notfound;

    string_to_lowercase_copy(lc_key, key, sizeof(lc_key));
    sval = dictionary_get(d, lc_key, INI_KEY_NOT_FOUND);

    if (sval == INI_KEY_NOT_FOUND) return notfound;
    return strtoumax(sval, NULL, 0);
}

int iniparser_getint(const dictionary *d, const char *key, int notfound) {
    return (int)iniparser_getlongint(d, key, notfound);
}

double iniparser_getdouble(const dictionary *d, const char *key, double notfound) {
    const char *sval;
    char lc_key[ASCIILINESZ + 1];

    if (d == NULL || key == NULL) return notfound;

    string_to_lowercase_copy(lc_key, key, sizeof(lc_key));
    sval = dictionary_get(d, lc_key, INI_KEY_NOT_FOUND);

    if (sval == INI_KEY_NOT_FOUND) return notfound;
    return atof(sval);
}

int iniparser_getboolean(const dictionary *d, const char *key, int notfound) {
    const char *sval;
    char lc_key[ASCIILINESZ + 1];

    if (d == NULL || key == NULL) return notfound;

    string_to_lowercase_copy(lc_key, key, sizeof(lc_key));
    sval = dictionary_get(d, lc_key, INI_KEY_NOT_FOUND);

    if (sval == INI_KEY_NOT_FOUND) return notfound;

    if (sval[0] == 'y' || sval[0] == 'Y' || sval[0] == '1' || sval[0] == 't' || sval[0] == 'T') {
        return 1;
    } else if (sval[0] == 'n' || sval[0] == 'N' || sval[0] == '0' || sval[0] == 'f' || sval[0] == 'F') {
        return 0;
    }
    return notfound;
}

/*---------------------------------------------------------------------------
                      Internal Parsing Functions
 ---------------------------------------------------------------------------*/

static void parse_quoted_value(char *value, char quote) {
    char c;
    char *value_copy, *t;
    int read_idx = 0, write_idx = 0;
    int esc = 0;
    size_t len;

    if (!value) return;

    len = strlen(value) + 1;
    t = (char *) malloc(len);
    if (!t) {
        iniparser_error_callback("iniparser: memory allocation failure\n");
        return;
    }
    memcpy(t, value, len);
    value_copy = t;

    while ((c = value_copy[read_idx]) != '\0') {
        if (!esc) {
            if (c == '\\') {
                esc = 1;
                read_idx++;
                continue;
            }
            if (c == quote) {
                break; /* End of quoted value */
            }
        }
        esc = 0;
        value[write_idx] = c;
        write_idx++;
        read_idx++;
    }
    value[write_idx] = '\0';
    free(value_copy);
}

static line_status iniparser_line(
    const char *input_line,
    char *section,
    char *key,
    char *value) {
    line_status sta;
    char *line_copy = NULL;
    size_t len;
    int d_quote;

    if (!input_line) return LINE_ERROR;

    len = strlen(input_line) + 1;
    line_copy = (char *) malloc(len);
    if (!line_copy) {
        iniparser_error_callback("iniparser: memory allocation failure\n");
        return LINE_ERROR;
    }
    memcpy(line_copy, input_line, len);

    str_trim(line_copy);
    len = strlen(line_copy);

    if (len < 1) {
        sta = LINE_EMPTY;
    } else if (line_copy[0] == '#' || line_copy[0] == ';') {
        sta = LINE_COMMENT;
    } else if (line_copy[0] == '[' && line_copy[len - 1] == ']') {
        sscanf(line_copy, "[%[^]]", section);
        str_trim(section);
        str_tolower(section);
        sta = LINE_SECTION;
    } else if ((d_quote = sscanf(line_copy, "%[^=] = \"%[^\"]\"", key, value)) == 2
               || (sscanf(line_copy, "%[^=] = '%[^\']'", key, value) == 2)) {
        str_trim(key);
        str_tolower(key);
        if (d_quote == 2)
            parse_quoted_value(value, '"');
        else
            parse_quoted_value(value, '\'');
        /* Do not strip spaces from values surrounded by quotes */
        sta = LINE_VALUE;
    } else if (sscanf(line_copy, "%[^=] = %[^;#]", key, value) == 2) {
        str_trim(key);
        str_tolower(key);
        str_trim(value);
        if (!strcmp(value, "\"\"") || (!strcmp(value, "''"))) {
            value[0] = 0;
        }
        sta = LINE_VALUE;
    } else if (sscanf(line_copy, "%[^=] = %[;#]", key, value) == 2
               || sscanf(line_copy, "%[^=] %[=]", key, value) == 2) {
        str_trim(key);
        str_tolower(key);
        value[0] = 0;
        sta = LINE_VALUE;
    } else {
        sta = LINE_ERROR;
    }

    free(line_copy);
    return sta;
}

/*---------------------------------------------------------------------------
                      Public API Loading Functions
 ---------------------------------------------------------------------------*/

dictionary *iniparser_load_file(FILE *in, const char *ininame) {
    char line[ASCIILINESZ + 1];
    char section[ASCIILINESZ + 1] = "";
    char key[ASCIILINESZ + 1];
    char tmp_key[(ASCIILINESZ * 2) + 2];
    char val[ASCIILINESZ + 1];

    int last = 0;
    int len;
    int lineno = 0;
    int errs = 0;
    dictionary *dict;

    dict = dictionary_new(0);
    if (!dict) {
        return NULL;
    }

    while (fgets(line + last, ASCIILINESZ - last, in) != NULL) {
        lineno++;
        len = (int) strlen(line) - 1;
        if (len <= 0) continue;

        if (line[len] != '\n' && !feof(in)) {
            iniparser_error_callback("iniparser: input line too long in %s (%d)\n", ininame, lineno);
            dictionary_del(dict);
            return NULL;
        }

        while ((len >= 0) && ((line[len] == '\n') || (isspace((unsigned char) line[len])))) {
            line[len] = 0;
            len--;
        }
        if (len < 0) {
            len = 0;
        }

        if (line[len] == '\\') {
            last = len;
            continue;
        } else {
            last = 0;
        }

        switch (iniparser_line(line, section, key, val)) {
            case LINE_EMPTY:
            case LINE_COMMENT:
                break;

            case LINE_SECTION:
                if (dictionary_set(dict, section, NULL) < 0) {
                    iniparser_error_callback("iniparser: memory allocation failure\n");
                    errs++;
                }
                break;

            case LINE_VALUE:
                sprintf(tmp_key, "%s:%s", section, key);
                if (dictionary_set(dict, tmp_key, val) < 0) {
                    iniparser_error_callback("iniparser: memory allocation failure\n");
                    errs++;
                }
                break;

            case LINE_ERROR:
                iniparser_error_callback("iniparser: syntax error in %s (%d):\n-> %s\n", ininame, lineno, line);
                errs++;
                break;

            default:
                break;
        }

        if (errs) {
            dictionary_del(dict);
            return NULL;
        }
        memset(line, 0, ASCIILINESZ + 1);
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

/*---------------------------------------------------------------------------
                           Test Main
 ---------------------------------------------------------------------------*/

int main() {
    dictionary *ini = NULL;
    const char *filename = "example.ini";

    iniparser_set_error_callback(NULL);

    ini = iniparser_load(filename);
    if (ini == NULL) {
        return 1;
    }

    const char *stringValue = iniparser_getstring(ini, "section1:stringvalue", "NOT_FOUND");
    printf("section1:stringvalue = %s \n", stringValue);

    int intValue = iniparser_getint(ini, "section1:intvalue", 999);
    printf("section1:intvalue = %d \n", intValue);

    long int longIntValue = iniparser_getlongint(ini, "section1:longintvalue", -1L);
    printf("section1:longintvalue = %ld \n", longIntValue);

    int64_t int64Value = iniparser_getint64(ini, "section1:intvalue", -1LL);
    printf("section1:intvalue = %lld \n", int64Value);

    uint64_t uint64Value = iniparser_getuint64(ini, "section1:uint64value", 0ULL);
    printf("section1:uint64value = %llu \n", uint64Value);

    double doubleValue = iniparser_getdouble(ini, "section1:doublevalue", -1.0);
    printf("section1:doublevalue = %f \n", doubleValue);

    int booleanTrue = iniparser_getboolean(ini, "section1:booleanValue", -1);
    printf("section1:booleanValue = %d \n", booleanTrue);

    const char *longStringValue = iniparser_getstring(ini, "section1:longstringvalue", "Error");
    printf("section1:longstringvalue = %s \n", longStringValue);

    iniparser_freedict(ini);
    ini = NULL;

    return 0;
}

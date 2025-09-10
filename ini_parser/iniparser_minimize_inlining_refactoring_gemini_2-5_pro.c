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
typedef enum _line_status_ {
    LINE_UNPROCESSED,
    LINE_ERROR,
    LINE_EMPTY,
    LINE_COMMENT,
    LINE_SECTION,
    LINE_VALUE
} line_status;

/*---------------------------- Static Helpers ------------------------------*/

static int default_error_callback(const char *format, ...);
static int (*iniparser_error_callback)(const char *, ...) = default_error_callback;

static void strlwc(const char *s, char *d, size_t len) {
    size_t i = 0;
    if (s == NULL || d == NULL || len == 0) return;
    while (s[i] != '\0' && i < len - 1) {
        d[i] = (char)tolower((int)s[i]);
        i++;
    }
    d[i] = '\0';
}

static void strstrip(char * s) {
    if (s == NULL) return;
    char *last = s + strlen(s);
    char *dest = s;
    // Trim leading space
    while (isspace((unsigned char)*s)) s++;
    // Trim trailing space
    while (last > s && isspace((unsigned char)*(last - 1))) last--;
    *last = '\0';
    // Move trimmed string to the beginning if necessary
    if (s != dest) {
        memmove(dest, s, last - s + 1);
    }
}


static const char * iniparser_get_value_string(const dictionary * d, const char * key) {
    char lower_key[ASCIILINESZ + 1];

    if (d == NULL || key == NULL) {
        return INI_INVALID_KEY;
    }

    strlwc(key, lower_key, sizeof(lower_key));
    return dictionary_get(d, lower_key, INI_INVALID_KEY);
}

static int default_error_callback(const char *format, ...) {
    int ret;
    va_list argptr;
    va_start(argptr, format);
    ret = vfprintf(stderr, format, argptr);
    va_end(argptr);
    return ret;
}

/*---------------------------------------------------------------------------
                            Public API
 ---------------------------------------------------------------------------*/

void iniparser_set_error_callback(int (*errback)(const char *, ...)) {
    iniparser_error_callback = errback ? errback : default_error_callback;
}

const char *iniparser_getstring(const dictionary *d, const char *key, const char *def) {
    const char *sval = iniparser_get_value_string(d, key);
    if (sval == INI_INVALID_KEY) {
        return def;
    }
    return sval;
}

long int iniparser_getlongint(const dictionary *d, const char *key, long int notfound) {
    const char *str = iniparser_get_value_string(d, key);
    if (str == INI_INVALID_KEY) {
        return notfound;
    }
    return strtol(str, NULL, 0);
}

int64_t iniparser_getint64(const dictionary *d, const char *key, int64_t notfound) {
    const char *str = iniparser_get_value_string(d, key);
    if (str == INI_INVALID_KEY) {
        return notfound;
    }
    return strtoimax(str, NULL, 0);
}

uint64_t iniparser_getuint64(const dictionary *d, const char *key, uint64_t notfound) {
    const char *str = iniparser_get_value_string(d, key);
    if (str == INI_INVALID_KEY) {
        return notfound;
    }
    return strtoumax(str, NULL, 0);
}

int iniparser_getint(const dictionary *d, const char *key, int notfound) {
    return (int)iniparser_getlongint(d, key, notfound);
}

double iniparser_getdouble(const dictionary *d, const char *key, double notfound) {
    const char *str = iniparser_get_value_string(d, key);
    if (str == INI_INVALID_KEY) {
        return notfound;
    }
    return atof(str);
}

int iniparser_getboolean(const dictionary *d, const char *key, int notfound) {
    const char *c = iniparser_get_value_string(d, key);
    if (c == INI_INVALID_KEY) {
        return notfound;
    }
    if (c[0] == 'y' || c[0] == 'Y' || c[0] == 't' || c[0] == 'T' || c[0] == '1') {
        return 1;
    }
    if (c[0] == 'n' || c[0] == 'N' || c[0] == 'f' || c[0] == 'F' || c[0] == '0') {
        return 0;
    }
    return notfound;
}

static void parse_quoted_value(char *value) {
    char *p = value;
    char *d = value;
    int esc = 0;

    // Remove starting quote
    if (*p == '"' || *p == '\'') {
        char quote = *p++;
        // Copy content, handling escapes, until closing quote
        while (*p) {
            if (esc) {
                *d++ = *p++;
                esc = 0;
            } else if (*p == '\\') {
                esc = 1;
                p++;
            } else if (*p == quote) {
                break; // End of quoted string
            } else {
                *d++ = *p++;
            }
        }
    }
    *d = '\0';
}


static line_status iniparser_line(const char *input_line, char *section, char *key, char *value) {
    char line[ASCIILINESZ + 1];
    char *p;
    size_t len;

    strncpy(line, input_line, ASCIILINESZ);
    line[ASCIILINESZ] = '\0';
    strstrip(line);

    len = strlen(line);
    if (len < 1) {
        return LINE_EMPTY;
    }
    if (line[0] == '#' || line[0] == ';') {
        return LINE_COMMENT;
    }

    if (line[0] == '[' && line[len - 1] == ']') {
        sscanf(line, "[%[^]]", section);
        strstrip(section);
        strlwc(section, section, ASCIILINESZ + 1);
        return LINE_SECTION;
    }

    p = strchr(line, '=');
    if (p == NULL) {
        return LINE_ERROR;
    }

    // Extract key
    *p = '\0';
    strncpy(key, line, ASCIILINESZ);
    key[ASCIILINESZ] = '\0';
    strstrip(key);
    strlwc(key, key, ASCIILINESZ + 1);

    // Extract value
    strncpy(value, p + 1, ASCIILINESZ);
    value[ASCIILINESZ] = '\0';
    strstrip(value);

    // Handle quoted values
    len = strlen(value);
    if (len > 1 && ((value[0] == '"' && value[len-1] == '"') || (value[0] == '\'' && value[len-1] == '\''))) {
        parse_quoted_value(value);
    } else {
        // Remove comments from unquoted values
        p = strpbrk(value, ";#");
        if (p) {
            *p = '\0';
            strstrip(value);
        }
    }

    return LINE_VALUE;
}

dictionary *iniparser_load_file(FILE *in, const char *ininame) {
    char line[ASCIILINESZ + 1];
    char section[ASCIILINESZ + 1] = "";
    char key[ASCIILINESZ + 1], val[ASCIILINESZ + 1];
    char tmp_key[(ASCIILINESZ * 2) + 2];
    int lineno = 0, errs = 0, last = 0, len;
    dictionary *dict;

    dict = dictionary_new(0);
    if (!dict) {
        return NULL;
    }

    while (fgets(line + last, ASCIILINESZ - last, in) != NULL) {
        lineno++;
        len = (int)strlen(line) - 1;
        if (len < 0) continue;

        if (line[len] != '\n' && !feof(in)) {
            iniparser_error_callback("iniparser: line too long in %s (%d)\n", ininame, lineno);
            dictionary_del(dict);
            return NULL;
        }

        // Handle multi-line values (line ending with '\')
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
                if (dictionary_set(dict, section, NULL) != 0) {
                    iniparser_error_callback("iniparser: memory allocation failure\n");
                    goto error;
                }
                break;
            case LINE_VALUE:
                sprintf(tmp_key, "%s:%s", section, key);
                if (dictionary_set(dict, tmp_key, val) != 0) {
                    iniparser_error_callback("iniparser: memory allocation failure\n");
                    goto error;
                }
                break;
            case LINE_ERROR:
                iniparser_error_callback("iniparser: syntax error in %s (%d):\n-> %s\n", ininame, lineno, line);
                errs++;
                break;
            default:
                break;
        }
        memset(line, 0, ASCIILINESZ);
    }

    if (errs > 0) {
        error:
        dictionary_del(dict);
        return NULL;
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
    dictionary *ini = NULL;
    const char *filename = "example.ini";

    iniparser_set_error_callback(NULL);

    ini = iniparser_load(filename);
    if (ini == NULL) {
        return 1;
    }

    const char *stringValue = iniparser_getstring(ini, "Section1:StringValue1", "NOT_FOUND");
    printf("Section1:StringValue1 = %s \n", stringValue);

    int intValue = iniparser_getint(ini, "Section2:IntValue1", 999);
    printf("Section2:IntValue1 = %d \n", intValue);

    long int longIntValue = iniparser_getlongint(ini, "Section1:LongIntValue2", -1L);
    printf("Section1:LongIntValue2 = %ld \n", longIntValue);

    int64_t int64Value = iniparser_getint64(ini, "Section1:IntValue1", -1LL);
    printf("Section1:IntValue1 = %lld \n", int64Value);

    uint64_t uint64Value = iniparser_getuint64(ini, "section1:uint64value1", 0ULL);
    printf("Section1:UInt64Value = %llu \n", uint64Value);

    double doubleValue = iniparser_getdouble(ini, "section1:doublevalue2", -1.0);
    printf("section1:doublevalue2 = %f \n", doubleValue);

    int booleanTrue = iniparser_getboolean(ini, "section1:boolEANtrue1", -1);
    printf("section1:boolEANtrue1 = %d \n", booleanTrue);

    const char *escapedString = iniparser_getstring(ini, "section1:escapedstring", "Error");
    printf("Section1:EscapedString = %s \n", escapedString);

    const char *quotedEmpty = iniparser_getstring(ini, "section1:quotedempty", "Error");
    printf("Section1:QuotedEmpty = \"%s\" \n", quotedEmpty);

    const char *separatedQuotedString = iniparser_getstring(ini, "section2:stringvalue1", "Error");
    printf("Section2:StringValue1 = %s \n", separatedQuotedString);

    iniparser_freedict(ini);
    ini = NULL;

    return 0;
}

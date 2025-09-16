#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include "iniparser.h"

#define ASCIILINESZ         (1024)
#define INI_INVALID_KEY     ((char*)-1)

typedef enum _line_status_ {
    LINE_UNPROCESSED,
    LINE_ERROR,
    LINE_EMPTY,
    LINE_COMMENT,
    LINE_SECTION,
    LINE_VALUE
} line_status;

static int default_error_callback(const char *format, ...);
static line_status iniparser_line(const char *input_line, char *section, char *key, char *value);

static int (*iniparser_error_callback)(const char *, ...) = default_error_callback;

static void str_to_lower(char *s)
{
    if (s == NULL) {
        return;
    }

    for ( ; *s; ++s) {
        *s = (char)tolower((unsigned char)*s);
    }
}

static void trim_string_in_place(char *s)
{
    if (s == NULL) {
        return;
    }

    char *start = s;
    while (isspace((unsigned char)*start)) {
        start++;
    }

    char *end = s + strlen(s);
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }
    *end = '\0';

    if (start > s) {
        memmove(s, start, (size_t)(end - start) + 1);
    }
}

static const char *get_string_from_dictionary(const dictionary *d, const char *key, const char *def)
{
    if (d == NULL || key == NULL) {
        return def;
    }

    char lower_key[ASCIILINESZ + 1];
    size_t i = 0;
    while (key[i] != '\0' && i < (sizeof(lower_key) - 1)) {
        lower_key[i] = (char)tolower((unsigned char)key[i]);
        i++;
    }
    lower_key[i] = '\0';

    return dictionary_get(d, lower_key, def);
}

static const char *get_value_string(const dictionary *d, const char *key)
{
    return get_string_from_dictionary(d, key, INI_INVALID_KEY);
}

static int default_error_callback(const char *format, ...)
{
    int ret;
    va_list argptr;
    va_start(argptr, format);
    ret = vfprintf(stderr, format, argptr);
    va_end(argptr);
    return ret;
}

void iniparser_set_error_callback(int (*errback)(const char *, ...))
{
    if (errback != NULL) {
        iniparser_error_callback = errback;
    } else {
        iniparser_error_callback = default_error_callback;
    }
}

const char *iniparser_getstring(const dictionary *d, const char *key, const char *def)
{
    return get_string_from_dictionary(d, key, def);
}

long int iniparser_getlongint(const dictionary *d, const char *key, long int notfound)
{
    const char *str = get_value_string(d, key);
    if (str == INI_INVALID_KEY) {
        return notfound;
    }
    return strtol(str, NULL, 0);
}

int64_t iniparser_getint64(const dictionary *d, const char *key, int64_t notfound)
{
    const char *str = get_value_string(d, key);
    if (str == INI_INVALID_KEY) {
        return notfound;
    }
    return strtoimax(str, NULL, 0);
}

uint64_t iniparser_getuint64(const dictionary *d, const char *key, uint64_t notfound)
{
    const char *str = get_value_string(d, key);
    if (str == INI_INVALID_KEY) {
        return notfound;
    }
    return strtoumax(str, NULL, 0);
}

int iniparser_getint(const dictionary *d, const char *key, int notfound)
{
    return (int)iniparser_getlongint(d, key, notfound);
}

double iniparser_getdouble(const dictionary *d, const char *key, double notfound)
{
    const char *str = get_value_string(d, key);
    if (str == INI_INVALID_KEY) {
        return notfound;
    }
    return atof(str);
}

int iniparser_getboolean(const dictionary *d, const char *key, int notfound)
{
    const char *c = get_value_string(d, key);

    if (c == INI_INVALID_KEY) {
        return notfound;
    }

    if (c[0] == 'y' || c[0] == 'Y' || c[0] == '1' || c[0] == 't' || c[0] == 'T') {
        return 1;
    } else if (c[0] == 'n' || c[0] == 'N' || c[0] == '0' || c[0] == 'f' || c[0] == 'F') {
        return 0;
    }

    return notfound;
}

static void parse_quoted_value(char *value, char quote)
{
    if (value == NULL) {
        return;
    }

    size_t len = strlen(value) + 1;
    char *temp_val = (char *)malloc(len);
    if (temp_val == NULL) {
        iniparser_error_callback("iniparser: memory allocation failure\n");
        return;
    }
    memcpy(temp_val, value, len);

    char *read_ptr = temp_val;
    char *write_ptr = value;
    int is_escaped = 0;

    while (*read_ptr != '\0') {
        if (!is_escaped) {
            if (*read_ptr == '\\') {
                is_escaped = 1;
                read_ptr++;
                continue;
            }
            if (*read_ptr == quote) {
                break;
            }
        }
        is_escaped = 0;
        *write_ptr = *read_ptr;
        write_ptr++;
        read_ptr++;
    }
    *write_ptr = '\0';
    free(temp_val);
}

static line_status iniparser_line(
    const char *input_line,
    char *section,
    char *key,
    char *value)
{
    line_status status = LINE_UNPROCESSED;
    char *line_buf = NULL;
    size_t len;

    if (input_line == NULL) {
        return LINE_ERROR;
    }

    len = strlen(input_line) + 1;
    line_buf = (char *)malloc(len);
    if (line_buf == NULL) {
        iniparser_error_callback("iniparser: memory allocation failure\n");
        return LINE_ERROR;
    }
    memcpy(line_buf, input_line, len);

    trim_string_in_place(line_buf);
    len = strlen(line_buf);

    if (len < 1) {
        status = LINE_EMPTY;
    } else if (line_buf[0] == '#' || line_buf[0] == ';') {
        status = LINE_COMMENT;
    } else if (line_buf[0] == '[' && line_buf[len - 1] == ']') {
        sscanf(line_buf, "[%[^]]]", section);
        trim_string_in_place(section);
        str_to_lower(section);
        status = LINE_SECTION;
    } else if (sscanf(line_buf, "%[^=] = \"%[^\"]\"", key, value) == 2) {
        trim_string_in_place(key);
        str_to_lower(key);
        parse_quoted_value(value, '"');
        status = LINE_VALUE;
    } else if (sscanf(line_buf, "%[^=] = '%[^\']'", key, value) == 2) {
        trim_string_in_place(key);
        str_to_lower(key);
        parse_quoted_value(value, '\'');
        status = LINE_VALUE;
    } else if (sscanf(line_buf, "%[^=] = %[^;#]", key, value) == 2) {
        trim_string_in_place(key);
        str_to_lower(key);
        trim_string_in_place(value);
        if (strcmp(value, "\"\"") == 0 || strcmp(value, "''") == 0) {
            value[0] = '\0';
        }
        status = LINE_VALUE;
    } else if (sscanf(line_buf, "%[^=] = %[;#]", key, value) == 2 || sscanf(line_buf, "%[^=] %[=]", key, value) == 2) {
        trim_string_in_place(key);
        str_to_lower(key);
        value[0] = '\0';
        status = LINE_VALUE;
    } else {
        status = LINE_ERROR;
    }

    free(line_buf);
    return status;
}

dictionary *iniparser_load_file(FILE *in, const char *ininame)
{
    char line[ASCIILINESZ + 1];
    char section[ASCIILINESZ + 1];
    char key[ASCIILINESZ + 1];
    char tmp[(ASCIILINESZ * 2) + 2];
    char val[ASCIILINESZ + 1];

    int write_offset = 0;
    int len;
    int lineno = 0;
    int errs = 0;
    int mem_err = 0;

    dictionary *dict;

    dict = dictionary_new(0);
    if (dict == NULL) {
        return NULL;
    }

    memset(section, 0, sizeof(section));

    while (fgets(line + write_offset, ASCIILINESZ - write_offset, in) != NULL) {
        lineno++;
        len = (int)strlen(line) - 1;
        if (len <= 0) {
            continue;
        }

        if (line[len] != '\n' && !feof(in)) {
            iniparser_error_callback(
                "iniparser: input line too long in %s (%d)\n",
                ininame,
                lineno);
            dictionary_del(dict);
            return NULL;
        }

        while ((len >= 0) &&
               ((line[len] == '\n') || (isspace((unsigned char)line[len])))) {
            line[len] = '\0';
            len--;
        }

        if (len < 0) {
            len = 0;
        }

        if (line[len] == '\\') {
            write_offset = len;
            continue;
        } else {
            write_offset = 0;
        }

        switch (iniparser_line(line, section, key, val)) {
            case LINE_EMPTY:
            case LINE_COMMENT:
                break;

            case LINE_SECTION:
                mem_err = dictionary_set(dict, section, NULL);
                break;

            case LINE_VALUE:
                sprintf(tmp, "%s:%s", section, key);
                mem_err = dictionary_set(dict, tmp, val);
                break;

            case LINE_ERROR:
                iniparser_error_callback(
                    "iniparser: syntax error in %s (%d):\n-> %s\n",
                    ininame,
                    lineno,
                    line);
                errs++;
                break;

            default:
                break;
        }
        memset(line, 0, sizeof(line));
        if (mem_err < 0) {
            iniparser_error_callback("iniparser: memory allocation failure\n");
            break;
        }
    }

    if (errs > 0) {
        dictionary_del(dict);
        dict = NULL;
    }
    return dict;
}

dictionary *iniparser_load(const char *ininame)
{
    FILE *in;
    dictionary *dict;

    in = fopen(ininame, "r");
    if (in == NULL) {
        iniparser_error_callback("iniparser: cannot open %s\n", ininame);
        return NULL;
    }

    dict = iniparser_load_file(in, ininame);
    fclose(in);

    return dict;
}

void iniparser_freedict(dictionary *d)
{
    dictionary_del(d);
}

int main(void)
{
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
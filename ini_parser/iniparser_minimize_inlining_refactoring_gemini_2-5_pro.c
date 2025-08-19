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

/*---------------------------- Forward declarations --------------------------*/
static void strstrip(char *s);
static void strlwc(const char *s, char *d, size_t len);
static const char *get_value_from_dictionary(const dictionary *d, const char *key);

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
 */
/*--------------------------------------------------------------------------*/
void iniparser_set_error_callback(int (*errback)(const char *, ...)) {
    iniparser_error_callback = errback ? errback : default_error_callback;
}

/*-------------------------------------------------------------------------*/
/**
  @brief	Strips blanks from the beginning and the end of a string.
  @param	s	String to parse.
  @return   void
  s is modified in place.
 */
/*--------------------------------------------------------------------------*/
static void strstrip(char *s) {
    if (s == NULL) return;

    char *last = s + strlen(s) - 1;
    char *dest = s;

    /* Trim leading space */
    while (isspace((unsigned char)*s)) {
        s++;
    }

    /* Trim trailing space */
    while (last > s && isspace((unsigned char)*last)) {
        *last-- = '\0';
    }

    /* Move trimmed string to the beginning if needed */
    if (s != dest) {
        memmove(dest, s, strlen(s) + 1);
    }
}

/*-------------------------------------------------------------------------*/
/**
  @brief	Convert a string to lowercase.
  @param	s	String to convert.
  @param  d   Destination buffer.
  @param  len Size of destination buffer.
  @return   void
 */
/*--------------------------------------------------------------------------*/
static void strlwc(const char *s, char *d, size_t len) {
    if (!s || !d || len == 0) return;
    size_t i = 0;
    while (s[i] != '\0' && i < len - 1) {
        d[i] = (char)tolower((int)s[i]);
        i++;
    }
    d[i] = '\0';
}

/*-------------------------------------------------------------------------*/
/**
  @brief	Helper to get a value from the dictionary with a lowercase key.
  @param	d	Dictionary to search.
  @param	key	Key string to look for.
  @return   Pointer to the value string, or INI_INVALID_KEY if not found.
 */
/*--------------------------------------------------------------------------*/
static const char *get_value_from_dictionary(const dictionary *d, const char *key) {
    if (d == NULL || key == NULL) {
        return INI_INVALID_KEY;
    }
    char lower_key[ASCIILINESZ + 1];
    strlwc(key, lower_key, sizeof(lower_key));
    return dictionary_get(d, lower_key, INI_INVALID_KEY);
}


/*-------------------------------------------------------------------------*/
/**
  @brief    Get the string associated to a key.
 */
/*--------------------------------------------------------------------------*/
const char *iniparser_getstring(const dictionary *d, const char *key, const char *def) {
    const char *sval = get_value_from_dictionary(d, key);
    if (sval == INI_INVALID_KEY) {
        return def;
    }
    return sval;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Get the string associated to a key, convert to a long int.
 */
/*--------------------------------------------------------------------------*/
long int iniparser_getlongint(const dictionary *d, const char *key, long int notfound) {
    const char *str = get_value_from_dictionary(d, key);
    if (str == INI_INVALID_KEY) {
        return notfound;
    }
    return strtol(str, NULL, 0);
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Get the string associated to a key, convert to an int64.
 */
/*--------------------------------------------------------------------------*/
int64_t iniparser_getint64(const dictionary *d, const char *key, int64_t notfound) {
    const char *str = get_value_from_dictionary(d, key);
    if (str == INI_INVALID_KEY) {
        return notfound;
    }
    return strtoimax(str, NULL, 0);
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Get the string associated to a key, convert to a uint64.
 */
/*--------------------------------------------------------------------------*/
uint64_t iniparser_getuint64(const dictionary *d, const char *key, uint64_t notfound) {
    const char *str = get_value_from_dictionary(d, key);
    if (str == INI_INVALID_KEY) {
        return notfound;
    }
    return strtoumax(str, NULL, 0);
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Get the string associated to a key, convert to an int.
 */
/*--------------------------------------------------------------------------*/
int iniparser_getint(const dictionary *d, const char *key, int notfound) {
    return (int)iniparser_getlongint(d, key, notfound);
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Get the string associated to a key, convert to a double.
 */
/*--------------------------------------------------------------------------*/
double iniparser_getdouble(const dictionary *d, const char *key, double notfound) {
    const char *str = get_value_from_dictionary(d, key);
    if (str == INI_INVALID_KEY) {
        return notfound;
    }
    return atof(str);
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Get the string associated to a key, convert to a boolean.
 */
/*--------------------------------------------------------------------------*/
int iniparser_getboolean(const dictionary *d, const char *key, int notfound) {
    const char *c = get_value_from_dictionary(d, key);
    if (c == INI_INVALID_KEY) {
        return notfound;
    }
    if (c[0] == 'y' || c[0] == 'Y' || c[0] == '1' || c[0] == 't' || c[0] == 'T') {
        return 1;
    }
    if (c[0] == 'n' || c[0] == 'N' || c[0] == '0' || c[0] == 'f' || c[0] == 'F') {
        return 0;
    }
    return notfound;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Finds out if a given entry exists in a dictionary.
 */
/*--------------------------------------------------------------------------*/
int iniparser_find_entry(const dictionary *ini, const char *entry) {
    return (get_value_from_dictionary(ini, entry) != INI_INVALID_KEY);
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Set an entry in a dictionary.
 */
/*--------------------------------------------------------------------------*/
int iniparser_set(dictionary *ini, const char *entry, const char *val) {
    char lower_entry[ASCIILINESZ + 1];
    strlwc(entry, lower_entry, sizeof(lower_entry));
    return dictionary_set(ini, lower_entry, val);
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Delete an entry in a dictionary.
 */
/*--------------------------------------------------------------------------*/
void iniparser_unset(dictionary *ini, const char *entry) {
    char lower_entry[ASCIILINESZ + 1];
    strlwc(entry, lower_entry, sizeof(lower_entry));
    dictionary_unset(ini, lower_entry);
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Parse a value surrounded by quotes.
 */
/*--------------------------------------------------------------------------*/
static void parse_quoted_value(char *value, char quote) {
    if (!value) return;

    size_t len = strlen(value);
    char *temp = (char *)malloc(len + 1);
    if (!temp) {
        iniparser_error_callback("iniparser: memory allocation failure\n");
        return;
    }
    memcpy(temp, value, len + 1);

    char *read_ptr = temp;
    char *write_ptr = value;
    int is_escaped = 0;

    while (*read_ptr) {
        if (!is_escaped && *read_ptr == '\\') {
            is_escaped = 1;
        } else if (!is_escaped && *read_ptr == quote) {
            break; /* End of quoted value */
        } else {
            *write_ptr++ = *read_ptr;
            is_escaped = 0;
        }
        read_ptr++;
    }
    *write_ptr = '\0';
    free(temp);
}


/*-------------------------------------------------------------------------*/
/**
  @brief    Load a single line from an INI file.
 */
/*--------------------------------------------------------------------------*/
static line_status iniparser_line(
    const char *input_line,
    char *section,
    char *key,
    char *value)
{
    char line_buf[ASCIILINESZ + 1];
    strncpy(line_buf, input_line, sizeof(line_buf));
    line_buf[ASCIILINESZ] = '\0';
    strstrip(line_buf);

    size_t len = strlen(line_buf);
    if (len < 1) {
        return LINE_EMPTY;
    }
    if (line_buf[0] == '#' || line_buf[0] == ';') {
        return LINE_COMMENT;
    }

    if (line_buf[0] == '[' && line_buf[len - 1] == ']') {
        strncpy(section, line_buf + 1, len - 2);
        section[len - 2] = '\0';
        strstrip(section);
        strlwc(section, section, ASCIILINESZ + 1);
        return LINE_SECTION;
    }

    char *eq_ptr = strchr(line_buf, '=');
    if (!eq_ptr) {
        return LINE_ERROR;
    }

    *eq_ptr = '\0';
    strncpy(key, line_buf, ASCIILINESZ + 1);
    strncpy(value, eq_ptr + 1, ASCIILINESZ + 1);

    strstrip(key);
    strlwc(key, key, ASCIILINESZ + 1);

    strstrip(value);
    len = strlen(value);
    if (len > 1 && ((value[0] == '"' && value[len - 1] == '"') || (value[0] == '\'' && value[len - 1] == '\''))) {
        char quote = value[0];
        value[len - 1] = '\0'; /* Remove trailing quote */
        memmove(value, value + 1, len); /* Remove leading quote */
        parse_quoted_value(value, quote);
    } else {
        /* Stop at comment characters if not quoted */
        char *comment_ptr = strpbrk(value, ";#");
        if (comment_ptr) {
            *comment_ptr = '\0';
            strstrip(value);
        }
    }

    return LINE_VALUE;
}


/*-------------------------------------------------------------------------*/
/**
  @brief    Parse an ini file and return an allocated dictionary object.
 */
/*--------------------------------------------------------------------------*/
dictionary *iniparser_load_file(FILE *in, const char *ininame) {
    char line[ASCIILINESZ + 1];
    char section[ASCIILINESZ + 1] = {0};
    char key[ASCIILINESZ + 1];
    char val[ASCIILINESZ + 1];
    char tmp_key[ASCIILINESZ * 2 + 2];
    char multiline[ASCIILINESZ + 1] = {0};

    int lineno = 0;
    int errs = 0;
    dictionary *dict = dictionary_new(0);
    if (!dict) return NULL;

    while (fgets(line, sizeof(line), in) != NULL) {
        lineno++;
        strstrip(line);
        int len = (int)strlen(line);
        if (len == 0) continue;

        /* Check for buffer overflow */
        if (line[len - 1] != '\n' && !feof(in)) {
             iniparser_error_callback("iniparser: input line too long in %s (%d)\n", ininame, lineno);
             dictionary_del(dict);
             return NULL;
        }

        /* Detect and handle multi-line values */
        if (line[len - 1] == '\\') {
            line[len - 1] = '\0';
            strncat(multiline, line, sizeof(multiline) - strlen(multiline) - 1);
            continue;
        }
        strncat(multiline, line, sizeof(multiline) - strlen(multiline) - 1);

        switch (iniparser_line(multiline, section, key, val)) {
            case LINE_EMPTY:
            case LINE_COMMENT:
                break;

            case LINE_SECTION:
                if (dictionary_set(dict, section, NULL) != 0) {
                    errs++;
                }
                break;

            case LINE_VALUE:
                snprintf(tmp_key, sizeof(tmp_key), "%s:%s", section, key);
                if (dictionary_set(dict, tmp_key, val) != 0) {
                    errs++;
                }
                break;

            case LINE_ERROR:
                iniparser_error_callback("iniparser: syntax error in %s (%d):\n-> %s\n", ininame, lineno, multiline);
                errs++;
                break;

            default:
                break;
        }

        multiline[0] = '\0'; /* Reset multiline buffer */
        if (errs) {
            iniparser_error_callback("iniparser: memory allocation failure\n");
            break;
        }
    }

    if (errs) {
        dictionary_del(dict);
        return NULL;
    }
    return dict;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Parse an ini file and return an allocated dictionary object.
 */
/*--------------------------------------------------------------------------*/
dictionary *iniparser_load(const char *ininame) {
    FILE *in = fopen(ininame, "r");
    if (in == NULL) {
        iniparser_error_callback("iniparser: cannot open %s\n", ininame);
        return NULL;
    }
    dictionary *dict = iniparser_load_file(in, ininame);
    fclose(in);
    return dict;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Free all memory associated to an ini dictionary.
 */
/*--------------------------------------------------------------------------*/
void iniparser_freedict(dictionary *d) {
    dictionary_del(d);
}


/* Main function for testing */
int main() {
    /* To test, you need an "example.ini" file. Example content:
    ;
    ; this is an example ini file
    ;
    [section1]
    stringvalue = "hello world with spaces"
    intvalue = 42
    longintvalue = 1234567890
    uint64value = 18446744073709551615
    booleantrue = y
    escapedstring = "hello \"world\""
    quotedempty = ""
    */
    dictionary *ini = NULL;
    const char *filename = "example.ini";

    /* Create a dummy example.ini for testing if it doesn't exist */
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fp = fopen(filename, "w");
        if (fp) {
            fprintf(fp, "; Test INI file\n");
            fprintf(fp, "[section1]\n");
            fprintf(fp, "stringValue = \"hello world with spaces\"\n");
            fprintf(fp, "intValue = 42\n");
            fprintf(fp, "longIntValue = 1234567890\n");
            fprintf(fp, "uint64value = 18446744073709551615\n");
            fprintf(fp, "booleanTrue = y\n");
            fprintf(fp, "escapedString = \"hello \\\"world\\\"\"\n");
            fprintf(fp, "quotedEmpty = \"\"\n");
            fprintf(fp, "another_key = value_without_quotes ; comment\n");
            fclose(fp);
        }
    } else {
        fclose(fp);
    }

    printf("--- Loading '%s' ---\n", filename);
    ini = iniparser_load(filename);
    if (ini == NULL) {
        fprintf(stderr, "Failed to load %s\n", filename);
        return 1;
    }
    printf("Successfully loaded.\n\n");

    printf("--- Testing Getters ---\n");
    printf("Section1:StringValue = %s\n", iniparser_getstring(ini, "section1:stringvalue", "NOT_FOUND"));
    printf("Section1:IntValue = %d\n", iniparser_getint(ini, "section1:intvalue", -1));
    printf("Section1:LongIntValue = %ld\n", iniparser_getlongint(ini, "section1:longintvalue", -1L));
    printf("Section1:UInt64Value = %llu\n", iniparser_getuint64(ini, "section1:uint64value", 0ULL));
    printf("Section1:BooleanTrue = %d\n", iniparser_getboolean(ini, "section1:booleantrue", -1));
    printf("Section1:EscapedString = %s\n", iniparser_getstring(ini, "section1:escapedstring", "Error"));
    printf("Section1:QuotedEmpty = \"%s\"\n", iniparser_getstring(ini, "section1:quotedempty", "Error"));
    printf("Section1:Another_Key = %s\n", iniparser_getstring(ini, "section1:another_key", "NOT_FOUND"));
    printf("Non-existent key = %d\n", iniparser_getint(ini, "section1:nonexistent", 999));
    printf("\n");

    printf("--- Testing iniparser_find_entry ---\n");
    printf("Entry 'section1' exists? %s\n", iniparser_find_entry(ini, "section1") ? "Yes" : "No");
    printf("Entry 'section1:intvalue' exists? %s\n", iniparser_find_entry(ini, "section1:intvalue") ? "Yes" : "No");
    printf("Entry 'section2' exists? %s\n", iniparser_find_entry(ini, "section2") ? "Yes" : "No");
    printf("\n");

    printf("--- Testing Set/Unset ---\n");
    printf("Setting new key 'section1:new_key' to 'new_value'\n");
    iniparser_set(ini, "section1:new_key", "new_value");
    printf("New Key = %s\n", iniparser_getstring(ini, "section1:new_key", "NOT_FOUND"));
    printf("Unsetting key 'section1:intvalue'\n");
    iniparser_unset(ini, "section1:intvalue");
    printf("IntValue after unset = %d (default)\n", iniparser_getint(ini, "section1:intvalue", -1));
    printf("\n");

    printf("--- Freeing dictionary ---\n");
    iniparser_freedict(ini);
    printf("Dictionary freed.\n\n");

    printf("All tests completed.\n");

    return 0;
}
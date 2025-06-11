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
  @brief    Get number of sections in a dictionary
  @param    d   Dictionary to examine
  @return   int Number of sections found in dictionary

  This function returns the number of sections found in a dictionary.
  The test to recognize sections is done on the string stored in the
  dictionary: a section name is given as "section" whereas a key is
  stored as "section:key", thus the test looks for entries that do not
  contain a colon.

  This clearly fails in the case a section name contains a colon, but
  this should simply be avoided.

  This function returns -1 in case of error.
 */
/*--------------------------------------------------------------------------*/
int iniparser_getnsec(const dictionary *d) {
    size_t i;
    int nsec;

    if (d == NULL) return -1;
    nsec = 0;
    for (i = 0; i < d->size; i++) {
        if (d->key[i] == NULL)
            continue ;
        if (strchr(d->key[i], ':') == NULL) {
            nsec++;
        }
    }
    return nsec;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Get name for section n in a dictionary.
  @param    d   Dictionary to examine
  @param    n   Section number (from 0 to nsec-1).
  @return   Pointer to char string

  This function locates the n-th section in a dictionary and returns
  its name as a pointer to a string statically allocated inside the
  dictionary. Do not free or modify the returned string!

  This function returns NULL in case of error.
 */
/*--------------------------------------------------------------------------*/
const char *iniparser_getsecname(const dictionary *d, int n) {
    size_t i;
    int foundsec;

    if (d == NULL || n < 0) return NULL;
    foundsec = 0;
    for (i = 0; i < d->size; i++) {
        if (d->key[i] == NULL)
            continue ;
        if (strchr(d->key[i], ':') == NULL) {
            foundsec++;
            if (foundsec > n)
                break ;
        }
    }
    if (foundsec <= n) {
        return NULL;
    }
    return d->key[i];
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Dump a dictionary to an opened file pointer.
  @param    d   Dictionary to dump.
  @param    f   Opened file pointer to dump to.
  @return   void

  This function prints out the contents of a dictionary, one element by
  line, onto the provided file pointer. It is OK to specify @c stderr
  or @c stdout as output files. This function is meant for debugging
  purposes mostly.
 */
/*--------------------------------------------------------------------------*/
void iniparser_dump(const dictionary *d, FILE *f) {
    size_t i;

    if (d == NULL || f == NULL) return;
    for (i = 0; i < d->size; i++) {
        if (d->key[i] == NULL)
            continue ;
        if (d->val[i] != NULL) {
            fprintf(f, "[%s]=[%s]\n", d->key[i], d->val[i]);
        } else {
            fprintf(f, "[%s]=UNDEF\n", d->key[i]);
        }
    }
    return;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Save a dictionary to a loadable ini file
  @param    d   Dictionary to dump
  @param    f   Opened file pointer to dump to
  @return   void

  This function dumps a given dictionary into a loadable ini file.
  It is Ok to specify @c stderr or @c stdout as output files.
 */
/*--------------------------------------------------------------------------*/
void iniparser_dump_ini(const dictionary *d, FILE *f) {
    size_t i;
    size_t nsec;
    const char *secname;
    char escaped[(ASCIILINESZ * 2) + 2] = "";
    char c;
    int v, e;

    if (d == NULL || f == NULL) return;

    nsec = iniparser_getnsec(d);
    if (nsec < 1) {
        /* No section in file: dump all keys as they are */
        for (i = 0; i < d->size; i++) {
            if (d->key[i] == NULL)
                continue ;
            v = 0;
            e = 0;

            if (!escaped || !d->val[i])
                return;

            while ((c = d->val[i][v]) != '\0') {
                if (c == '\\' || c == '"') {
                    escaped[e] = '\\';
                    e++;
                }
                escaped[e] = c;
                v++;
                e++;
            }
            escaped[e] = '\0';
            fprintf(f, "%s = \"%s\"\n", d->key[i], escaped);
        }
        return;
    }
    for (i = 0; i < nsec; i++) {
        secname = iniparser_getsecname(d, i);
        iniparser_dumpsection_ini(d, secname, f);
    }
    fprintf(f, "\n");
    return;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Save a dictionary section to a loadable ini file
  @param    d   Dictionary to dump
  @param    s   Section name of dictionary to dump
  @param    f   Opened file pointer to dump to
  @return   void

  This function dumps a given section of a given dictionary into a loadable ini
  file.  It is Ok to specify @c stderr or @c stdout as output files.
 */
/*--------------------------------------------------------------------------*/
void iniparser_dumpsection_ini(const dictionary *d, const char *s, FILE *f) {
    size_t j;
    char keym[ASCIILINESZ + 1];
    int seclen;
    char escaped[(ASCIILINESZ * 2) + 2] = "";
    char c;
    int v, e;

    if (d == NULL || f == NULL) return;
    if (!iniparser_find_entry(d, s)) return;
    if (strlen(s) > sizeof(keym)) return;

    seclen = (int) strlen(s);
    fprintf(f, "\n[%s]\n", s);
    sprintf(keym, "%s:", s);
    for (j = 0; j < d->size; j++) {
        if (d->key[j] == NULL)
            continue ;
        if (!strncmp(d->key[j], keym, seclen + 1)) {
            v = 0;
            e = 0;

            if (!escaped || !d->val[j])
                return;

            while ((c = d->val[j][v]) != '\0') {
                if (c == '\\' || c == '"') {
                    escaped[e] = '\\';
                    e++;
                }
                escaped[e] = c;
                v++;
                e++;
            }
            escaped[e] = '\0';
            fprintf(f, "%-30s = \"%s\"\n", d->key[j] + seclen + 1, escaped);
        }
    }
    fprintf(f, "\n");
    return;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Get the number of keys in a section of a dictionary.
  @param    d   Dictionary to examine
  @param    s   Section name of dictionary to examine
  @return   Number of keys in section
 */
/*--------------------------------------------------------------------------*/
int iniparser_getsecnkeys(const dictionary *d, const char *s) {
    int seclen, nkeys;
    char keym[ASCIILINESZ + 1];
    size_t j;
    unsigned i;

    nkeys = 0;

    if (d == NULL) return nkeys;
    if (!iniparser_find_entry(d, s)) return nkeys;

    seclen = (int) strlen(s);

    if (s != NULL && keym != NULL || sizeof(keym) != 0) {
        i = 0;
        while (s[i] != '\0' && i < sizeof(keym) - 1) {
            keym[i] = (char) tolower((int) s[i]);
            i++;
        }
        keym[i] = '\0';
    }

    keym[seclen] = ':';

    for (j = 0; j < d->size; j++) {
        if (d->key[j] == NULL)
            continue ;
        if (!strncmp(d->key[j], keym, seclen + 1))
            nkeys++;
    }

    return nkeys;
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Get the number of keys in a section of a dictionary.
  @param    d    Dictionary to examine
  @param    s    Section name of dictionary to examine
  @param    keys Already allocated array to store the keys in
  @return   The pointer passed as `keys` argument or NULL in case of error

  This function queries a dictionary and finds all keys in a given section.
  The keys argument should be an array of pointers which size has been
  determined by calling `iniparser_getsecnkeys` function prior to this one.

  Each pointer in the returned char pointer-to-pointer is pointing to
  a string allocated in the dictionary; do not free or modify them.
 */
/*--------------------------------------------------------------------------*/
const char **iniparser_getseckeys(const dictionary *d, const char *s, const char **keys) {
    size_t i, j, seclen;
    char keym[ASCIILINESZ + 1];
    unsigned k;

    if (d == NULL || keys == NULL) return NULL;
    if (!iniparser_find_entry(d, s)) return NULL;

    seclen = strlen(s);

    if (s != NULL && keym != NULL && sizeof(keym) != 0) {
        k = 0;
        while (s[k] != '\0' && k < sizeof(keym) - 1) {
            keym[k] = (char) tolower((int) s[k]);
            k++;
        }
        keym[k] = '\0';
    }

    keym[seclen] = ':';

    i = 0;

    for (j = 0; j < d->size; j++) {
        if (d->key[j] == NULL)
            continue ;
        if (!strncmp(d->key[j], keym, seclen + 1)) {
            keys[i] = d->key[j];
            i++;
        }
    }

    return keys;
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

    if (ini == NULL || key == NULL) {
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

        line = last - line;
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

            section = last - section;
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

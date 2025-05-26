/* (c) 2019 Jan Doczy
 * This code is licensed under MIT license (see LICENSE.txt for details) */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "csv.h"

#include <sys/types.h>
typedef off_t file_off_t;

/* max allowed buffer */
#define BUFFER_WIDTH_APROX (40 * 1024 * 1024)

/* private csv handle:
 * @mem: pointer to memory
 * @pos: position in buffer
 * @size: size of memory chunk
 * @context: context used when processing cols
 * @blockSize: size of mapped block
 * @fileSize: size of opened file
 * @mapSize: ...
 * @auxbuf: auxiliary buffer
 * @auxbufSize: size of aux buffer
 * @auxbufPos: position of aux buffer reader
 * @quotes: number of pending quotes parsed
 * @fh: file handle - descriptor
 * @delim: delimeter - ','
 * @quote: quote '"'
 * @escape: escape char
 */
struct CsvHandle_
{
    void* mem;
    size_t pos;
    size_t size;
    char* context;
    size_t blockSize;
    file_off_t fileSize;
    file_off_t mapSize;
    size_t auxbufSize;
    size_t auxbufPos;
    size_t quotes;
    void* auxbuf;
    int fh;
    char delim;
    char quote;
    char escape;
};

CsvHandle CsvOpen(const char* filename)
{
    /* defaults */
    return CsvOpen2(filename, ',', '"', '\\');
}

/* trivial macro used to get page-aligned buffer size */
#define GET_PAGE_ALIGNED( orig, page ) \
    (((orig) + ((page) - 1)) & ~((page) - 1))


#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

CsvHandle CsvOpen2(const char* filename,
                   char delim,
                   char quote,
                   char escape)
{
    long pageSize;
    struct stat fs;
    CsvHandle handle = NULL;

    /* alloc zero-initialized mem */
    handle = calloc(1, sizeof(struct CsvHandle_));
    if (!handle)
        goto fail;

    /* set chars */
    handle->delim = delim;
    handle->quote = quote;
    handle->escape = escape;

    /* page size */
    pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize < 0)
        goto fail;

    /* align to system page size */
    handle->blockSize = GET_PAGE_ALIGNED(BUFFER_WIDTH_APROX, pageSize);

    /* open new fd */
    handle->fh = open(filename, O_RDONLY);
    if (handle->fh < 0)
        goto fail;

    /* get real file size */
    if (fstat(handle->fh, &fs))
    {
       close(handle->fh);
       goto fail;
    }

    handle->fileSize = fs.st_size;
    return handle;

  fail:
    free(handle);
    return NULL;
}

static void* MapMem(CsvHandle handle)
{
    handle->mem = mmap(0, handle->blockSize,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE,
                       handle->fh, handle->mapSize);
    return handle->mem;
}

static void UnmapMem(CsvHandle handle)
{
    if (handle->mem)
        munmap(handle->mem, handle->blockSize);
}

void CsvClose(CsvHandle handle)
{
    if (!handle)
        return;

    UnmapMem(handle);

    close(handle->fh);
    free(handle->auxbuf);
    free(handle);
}

static int CsvEnsureMapped(CsvHandle handle)
{
    file_off_t newSize;

    /* do not need to map */
    if (handle->pos < handle->size)
        return 0;

    UnmapMem(handle);

    handle->mem = NULL;
    if (handle->mapSize >= handle->fileSize)
        return -EINVAL;

    newSize = handle->mapSize + handle->blockSize;
    if (MapMem(handle))
    {
        handle->pos = 0;
        handle->mapSize = newSize;

        /* read only up to filesize:
         * 1. mapped block size is < then filesize: (use blocksize)
         * 2. mapped block size is > then filesize: (use remaining filesize) */
        handle->size = handle->blockSize;
        if (handle->mapSize > handle->fileSize)
            handle->size = (size_t)(handle->fileSize % handle->blockSize);

        return 0;
    }

    return -ENOMEM;
}

char* CsvSearchLf(char* p, size_t size, CsvHandle handle)
{
    char* res = NULL;
    char c;
    char* end = p + size;
    char quote = handle->quote;

    for (; p < end; p++)
    {
        c = *p;

        if (c == quote) {
            handle->quotes++;
        } else if (c == '\n' && !(handle->quotes & 1)) {
            res = p;
        }

        if (res != NULL) {
            return res;
        }
    }

    return NULL;
}

/**
 * @brief Reallocates the auxiliary buffer if the required size exceeds the current capacity.
 *
 * @param handle The CSV handle.
 * @param required_size The minimum required size for the auxiliary buffer.
 * @return 0 on success, 1 on memory allocation failure.
 */
static int reallocate_aux_buffer(CsvHandle handle, size_t required_size)
{
    void* mem = NULL;
    if (handle->auxbufSize < required_size)
    {
        mem = realloc(handle->auxbuf, required_size);
        if (!mem)
        {
            return 1; /* Allocation failed */
        }
        handle->auxbuf = mem;
        handle->auxbufSize = required_size;
    }
    return 0; /* Success */
}

/**
 * @brief Processes the found newline, copying data to auxbuf if necessary and terminating the line.
 *
 * @param handle The CSV handle.
 * @param p The pointer to the beginning of the current chunk.
 * @param size The size of the current chunk before the newline.
 * @return The pointer to the processed row, or NULL on error.
 */
static char* process_found_newline(CsvHandle handle, char* p, size_t size)
{
    char* res = NULL;
    size_t newSize;

    if (handle->auxbufPos)
    {
        newSize = handle->auxbufPos + size + 1;
        if (reallocate_aux_buffer(handle, newSize))
        {
            return NULL;
        }

        memcpy((char*)handle->auxbuf + handle->auxbufPos, p, size);
        handle->auxbufPos += size;

        *(char*)((char*)handle->auxbuf + handle->auxbufPos) = '\0';
        p = handle->auxbuf;
        size = handle->auxbufPos;
    }

    /* reset auxbuf position */
    handle->auxbufPos = 0;

    /* terminate line */
    res = p + size - 1;
    if (size >= 2 && *(res - 1) == '\r') /* Corrected to *(res - 1) */
        --res;

    *res = 0;

    return p;
}


char* CsvReadNextRow(CsvHandle handle)
{
    int err;
    char* p = NULL;
    char* found = NULL;
    size_t size;
    size_t newSize;
    char* result_row = NULL;

    do
    {
        err = CsvEnsureMapped(handle);
        handle->context = NULL;

        if (err == -EINVAL)
        {
            /* if this is n-th iteration
             * return auxbuf (remaining bytes of the file) */
            if (p == NULL)
                break;

            return handle->auxbuf;
        }
        else if (err == -ENOMEM)
        {
            break;
        }

        size = handle->size - handle->pos;
        if (!size)
            break;

        /* search this chunk for NL */
        p = (char*)handle->mem + handle->pos;
        found = CsvSearchLf(p, size, handle);

        if (found)
        {
            /* prepare position for next iteration */
            size = (size_t)(found - p) + 1;
            handle->pos += size;
            handle->quotes = 0;
            result_row = process_found_newline(handle, p, size);
            if (result_row == NULL)
            {
                break; /* Error during processing newline */
            }
            return result_row;
        }
        else
        {
            /* reset on next iteration */
            handle->pos = handle->size;
        }

        /* correctly process boundries, storing
         * remaning bytes in aux buffer */
        newSize = handle->auxbufPos + size + 1;
        if (reallocate_aux_buffer(handle, newSize))
        {
            break; /* Allocation failed */
        }

        memcpy((char*)handle->auxbuf + handle->auxbufPos, p, size);
        handle->auxbufPos += size;

        *(char*)((char*)handle->auxbuf + handle->auxbufPos) = '\0';

    } while (!found);

    return NULL;
}

/**
 * @brief Handles parsing of a column that is enclosed in quotes.
 *
 * @param p Current read pointer.
 * @param d Current destination pointer for copying.
 * @param handle CsvHandle.
 * @return Updated read pointer.
 */
static char* handle_quoted_column(char* p, char* d, CsvHandle handle)
{
    int dq;
    for (; *p; p++, d++)
    {
        dq = 0;

        /* skip escape */
        if (*p == handle->escape && p[1])
            p++;

        /* skip double-quote */
        if (*p == handle->quote && p[1] == handle->quote)
        {
            dq = 1;
            p++;
        }

        /* check if we should end */
        if (!dq && *p == handle->quote)
        {
            break; /* End of quoted string */
        }
        else if (*p == handle->delim)
        {
            /* If we encounter a delimiter *inside* a quoted string, it's part of the value unless it's the terminating quote.
             * But we already handled the terminating quote above. So this means the delimiter is part of the content. */
        }

        /* copy if required */
        if (d != p)
            *d = *p;
    }
    return p;
}

/**
 * @brief Handles parsing of a column that is not enclosed in quotes.
 *
 * @param p Current read pointer.
 * @param d Current destination pointer for copying.
 * @param handle CsvHandle.
 * @return Updated read pointer.
 */
static char* handle_unquoted_column(char* p, char* d, CsvHandle handle)
{
    for (; *p; p++, d++)
    {
        if (*p == handle->delim)
        {
            break; /* End of unquoted string */
        }
        /* copy if required */
        if (d != p)
            *d = *p;
    }
    return p;
}

/**
 * @brief Handles the termination of a column, null-terminating and updating the context.
 *
 * @param p Current read pointer.
 * @param b Beginning of the column.
 * @param quoted Flag indicating if the column was quoted.
 * @param handle CsvHandle.
 */
static void handle_column_termination(char* p, char* b, int quoted, CsvHandle handle)
{
    /* Nothing to do if p is at the beginning (empty column) */
    if (p == b && *p == '\0')
    {
        handle->context = p;
        return;
    }

    *p = '\0'; /* Null-terminate the column */

    if (quoted)
    {
        /* After a quoted string, skip any characters until the next delimiter or end of string */
        for (p++; *p; p++)
            if (*p == handle->delim)
                break;

        if (*p) /* If a delimiter was found, advance past it */
            p++;

        handle->context = p;
    }
    else
    {
        /* For unquoted, just advance past the delimiter if it was found */
        handle->context = p + 1;
    }
}


const char* CsvReadNextCol(char* row, CsvHandle handle)
{
    /* return properly escaped CSV col
     * RFC: [https://tools.ietf.org/html/rfc4180]
     */
    char* p = NULL;
    char* d = NULL; /* destination */
    char* b = NULL; /* begin */
    int quoted = 0; /* idicates quoted string */

    p = handle->context ? handle->context : row;
    d = p;
    b = p;

    quoted = *p == handle->quote;
    if (quoted)
        p++;

    if (quoted)
    {
        p = handle_quoted_column(p, d, handle);
    }
    else
    {
        p = handle_unquoted_column(p, d, handle);
    }

    if (!*p && p == b) /* Check for empty column at the end of the row */
    {
        if (handle->context == NULL) /* Only if it's the very first column */
        {
            handle->context = p; /* Mark context to avoid re-reading empty string */
        }
        return NULL;
    }

    handle_column_termination(p, b, quoted, handle);
    return b;
}
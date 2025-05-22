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
    CsvHandle handle = calloc(1, sizeof(struct CsvHandle_));

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
    char* res;
    char c;
    char* end = p + size;
    char quote = handle->quote;

    for (; p < end; p++)
    {
        res = NULL;
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

/* Ensures the auxiliary buffer has enough capacity and reallocates if needed. */
static int ensure_auxiliary_buffer_capacity(CsvHandle handle, size_t required_additional_size)
{
    void* mem;
    size_t new_size = handle->auxbufPos + required_additional_size + 1;

    if (handle->auxbufSize < new_size)
    {
        mem = realloc(handle->auxbuf, new_size);
        if (!mem)
            return -1; // Allocation failed

        handle->auxbuf = mem;
        handle->auxbufSize = new_size;
    }
    return 0; // Success
}

/* Appends a chunk of data to the auxiliary buffer. */
static int append_chunk_to_auxiliary_buffer(CsvHandle handle, const char* chunk, size_t chunk_size)
{
    if (ensure_auxiliary_buffer_capacity(handle, chunk_size) != 0)
        return -1;

    memcpy((char*)handle->auxbuf + handle->auxbufPos, chunk, chunk_size);
    handle->auxbufPos += chunk_size;
    *(char*)((char*)handle->auxbuf + handle->auxbufPos) = '\0';
    return 0;
}

/* Terminates the row with a null character and resets the auxiliary buffer position. */
static char* terminate_row_and_reset_aux_buffer(CsvHandle handle, char* row_start, size_t row_size)
{
    char* res;
    handle->auxbufPos = 0; // reset auxbuf position

    res = row_start + row_size - 1;
    if (row_size >= 2 && row_start[row_size - 2] == '\r')
        --res;

    *res = '\0';
    return row_start;
}

char* CsvReadNextRow(CsvHandle handle)
{
    int err;
    char* p = NULL;
    char* found = NULL;
    size_t size;

    do
    {
        err = CsvEnsureMapped(handle);
        handle->context = NULL;

        if (err == -EINVAL)
        {
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

        p = (char*)handle->mem + handle->pos;
        found = CsvSearchLf(p, size, handle);

        if (found)
        {
            size_t current_chunk_len = (size_t)(found - p) + 1;
            handle->pos += current_chunk_len;
            handle->quotes = 0;

            if (handle->auxbufPos)
            {
                if (append_chunk_to_auxiliary_buffer(handle, p, current_chunk_len) != 0)
                    return NULL;
                p = handle->auxbuf;
                size = handle->auxbufPos;
            }
            else
            {
                size = current_chunk_len;
            }
            return terminate_row_and_reset_aux_buffer(handle, p, size);
        }
        else
        {
            handle->pos = handle->size;
            if (append_chunk_to_auxiliary_buffer(handle, p, size) != 0)
                return NULL;
        }

    } while (!found);

    return NULL;
}

/* Skips escape and double quote characters and returns if a double quote was skipped. */
static int skip_escape_and_double_quote(char** p, CsvHandle handle)
{
    int dq = 0;
    char* current_p = *p;

    if (*current_p == handle->escape && current_p[1])
    {
        (*p)++;
    }

    if (*current_p == handle->quote && current_p[1] == handle->quote)
    {
        dq = 1;
        (*p)++;
    }
    return dq;
}

/* Checks if the current character signifies the end of a column. */
static int is_column_end(char c, int quoted, int dq, CsvHandle handle)
{
    if (quoted && !dq)
    {
        return (c == handle->quote);
    }
    else
    {
        return (c == handle->delim);
    }
}

/* Updates the handle's context and returns the processed column. */
static const char* update_context_and_return_column(char* row_start, char* current_p, int quoted, CsvHandle handle)
{
    if (!*current_p)
    {
        if (current_p == row_start)
            return NULL;
        handle->context = current_p;
    }
    else
    {
        *current_p = '\0';
        if (quoted)
        {
            for (current_p++; *current_p; current_p++)
            {
                if (*current_p == handle->delim)
                    break;
            }
            if (*current_p)
                current_p++;
            handle->context = current_p;
        }
        else
        {
            handle->context = current_p + 1;
        }
    }
    return row_start;
}

const char* CsvReadNextCol(char* row, CsvHandle handle)
{
    char* p = handle->context ? handle->context : row;
    char* d = p;
    char* b = p;
    int dq;
    int quoted = 0;

    quoted = (*p == handle->quote);
    if (quoted)
        p++;

    for (; *p; p++, d++)
    {
        dq = skip_escape_and_double_quote(&p, handle);

        if (is_column_end(*p, quoted, dq, handle))
        {
            break;
        }

        if (d != p)
            *d = *p;
    }

    return update_context_and_return_column(b, p, quoted, handle);
}

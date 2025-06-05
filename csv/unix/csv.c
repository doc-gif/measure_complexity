/* (c) 2019 Jan Doczy
 * This code is licensed under MIT license (see LICENSE.txt for details) */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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
    /* alloc zero-initialized mem */
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

static char* CsvChunkToAuxBuf(CsvHandle handle, char* p, size_t size)
{
    void* mem;
    size_t newSize = handle->auxbufPos + size + 1;
    if (handle->auxbufSize < newSize)
    {
        mem = realloc(handle->auxbuf, newSize);
        if (!mem)
            return NULL;

        handle->auxbuf = mem;
        handle->auxbufSize = newSize;
    }

    memcpy((char*)handle->auxbuf + handle->auxbufPos, p, size);
    handle->auxbufPos += size;
    
    *(char*)((char*)handle->auxbuf + handle->auxbufPos) = '\0';
    return handle->auxbuf;
}

static void CsvTerminateLine(char* p, size_t size)
{
    char* res = p;
    if (size >= 2 && p[-1] == '\r')
        --res;

    *res = 0;
}

char* handle_quote_and_newline(char c, int n, char quote, CsvHandle handle, char* p) {
    if (c == quote) {
        handle->quotes++;
    } else if (c == '\n' && !(handle->quotes & 1)) {
        return p + n;
    }
    return NULL;
}

char* CsvSearchLf(char* p, size_t size, CsvHandle handle)
{
    char* res;
    char* end = p + size;
    char quote = handle->quote;

    for (; p < end; p++)
    {
        res = handle_quote_and_newline(*p, 0, quote, handle, p);
        if (res != NULL) {
            return res;
        }
    }

    return NULL;
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
            
            if (handle->auxbufPos)
            {
                if (!CsvChunkToAuxBuf(handle, p, size))
                    break;
                
                p = handle->auxbuf;
                size = handle->auxbufPos;
            }

            /* reset auxbuf position */
            handle->auxbufPos = 0;

            /* terminate line */
            CsvTerminateLine(p + size - 1, size);
            return p;
        }
        else
        {
            /* reset on next iteration */
            handle->pos = handle->size;
        }

        /* correctly process boundries, storing
         * remaning bytes in aux buffer */
        if (!CsvChunkToAuxBuf(handle, p, size))
            break;

    } while (!found);

    return NULL;
}

const char* CsvReadNextCol(char* row, CsvHandle handle)
{
    /* return properly escaped CSV col
     * RFC: [https://tools.ietf.org/html/rfc4180]
     */
    char* p = handle->context ? handle->context : row;
    char* d = p; /* destination */
    char* b = p; /* begin */
    int quoted = 0; /* idicates quoted string */
    int dq;

    quoted = *p == handle->quote;
    if (quoted)
        p++;

    for (; *p; p++, d++)
    {
        /* double quote is present if (1) */
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
        if (quoted && !dq)
        {
            if (*p == handle->quote)
                break;
        }
        else if (*p == handle->delim)
        {
            break;
        }

        /* copy if required */
        if (d != p)
            *d = *p;
    }
    
    if (!*p)
    {
        /* nothing to do */
        if (p == b)
            return NULL;

        handle->context = p;
    }
    else
    {
        /* end reached, skip */
        *d = '\0';
        if (quoted)
        {
            for (p++; *p; p++)
                if (*p == handle->delim)
                    break;

            if (*p)
                p++;
            
            handle->context = p;
        }
        else
        {
            handle->context = p + 1;
        }
    }
    return b;
}

int main() {
    const char *filename = "sample.csv";
    CsvHandle handle;
    char *row_buffer;
    const char *column_value;
    int row_number;

    handle = CsvOpen(filename);
    if (handle == NULL) {
        fprintf(stderr, "Error: Could not open CSV file '%s'.\n", filename);
        CsvClose(handle);
        return EXIT_FAILURE;
    }
    printf("Successfully opened CSV file: %s\n\n", filename);

    row_number = 0;
    while ((row_buffer = CsvReadNextRow(handle)) != NULL) {
        row_number++;
        printf("--- Row %d ---\n", row_number);

        char *current_row_ptr = row_buffer;
        int column_number = 0;

        while ((column_value = CsvReadNextCol(current_row_ptr, handle)) != NULL) {
            column_number++;
            printf("  Column %d: \"%s\"\n", column_number, column_value);
        }

        if (column_number == 0) {
            if (row_buffer[0] == '\0') {
                printf("  (Empty row or row with only empty unquoted fields parsed as empty)\n");
            } else {
                printf("  (Row data exists but no columns were extracted by CsvReadNextCol, raw: \"%s\")\n", row_buffer);
            }
        }
        printf("\n");
    }

    CsvClose(handle);
    printf("Closed CSV file: %s\n", filename);

    return 0;
}

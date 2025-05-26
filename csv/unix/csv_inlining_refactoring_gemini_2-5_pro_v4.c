/* (c) 2019 Jan Doczy
 * This code is licensed under MIT license (see LICENSE.txt for details) */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "csv.h"

#include <sys/types.h>
typedef off_t file_off_t; /* POSIX type for file sizes/offsets */

/* max allowed buffer */
#define BUFFER_WIDTH_APROX (40 * 1024 * 1024)

/* private csv handle:
 * @mem: pointer to memory mapped region
 * @pos: current reading position in mapped memory
 * @size: size of the current mapped memory chunk
 * @context: context used when processing cols (pointer within the current row)
 * @blockSize: size of block to be mapped
 * @fileSize: total size of the opened file
 * @mapSize: offset in file for the next mmap operation
 * @auxbuf: auxiliary buffer for lines spanning mmap blocks or for file end
 * @auxbufSize: allocated size of aux buffer
 * @auxbufPos: current end position of content in aux buffer
 * @quotes: number of pending quotes parsed (used by CsvSearchLf)
 * @fh: file handle - descriptor
 * @delim: delimiter character (e.g., ',')
 * @quote: quote character (e.g., '"')
 * @escape: escape character (e.g., '\\')
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
    void* auxbuf;
    size_t auxbufSize;
    size_t auxbufPos;
    size_t quotes;
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
    CsvHandle handle;
    long pageSize;
    struct stat fs;

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
    /* map a block of the file into memory */
    handle->mem = mmap(0, handle->blockSize,
                       PROT_READ | PROT_WRITE, /* PROT_WRITE for MAP_PRIVATE allows in-memory modifications */
                       MAP_PRIVATE,
                       handle->fh, handle->mapSize);
    return handle->mem;
}

static void UnmapMem(CsvHandle handle)
{
    /* unmap the memory region if it's currently mapped */
    if (handle->mem) {
        munmap(handle->mem, handle->blockSize);
        handle->mem = NULL; /* Mark as unmapped */
    }
}

void CsvClose(CsvHandle handle)
{
    if (!handle)
        return;

    UnmapMem(handle);

    if (handle->fh >= 0) /* Ensure fh is valid before closing */
        close(handle->fh);
    free(handle->auxbuf);
    free(handle);
}

static int CsvEnsureMapped(CsvHandle handle)
{
    file_off_t newMapOffsetAfterThisBlock;

    /* do not need to map if current position is within the already mapped size */
    if (handle->pos < handle->size)
        return 0;

    UnmapMem(handle);  /* Unmap previous block before mapping a new one */

    /* handle->mem is now NULL */
    if (handle->mapSize >= handle->fileSize) /* If we've already tried to map past the end */
        return -EINVAL; /* No more data to map */

    newMapOffsetAfterThisBlock = handle->mapSize + handle->blockSize;
    if (MapMem(handle)) /* MapMem sets handle->mem; handle->mapSize is the offset for this map */
    {
        handle->pos = 0; /* Reset position to start of new block */

        /* Determine the actual size of data available in this newly mapped block */
        if (newMapOffsetAfterThisBlock > handle->fileSize) {
            /* This block extends beyond EOF, so actual size is remaining file part */
            if (handle->fileSize > handle->mapSize) {
                handle->size = (size_t)(handle->fileSize - handle->mapSize);
            } else {
                handle->size = 0; /* No data if mapSize already at or beyond fileSize */
            }
        } else {
            /* Full block is within file bounds */
            handle->size = handle->blockSize;
        }
        handle->mapSize = newMapOffsetAfterThisBlock; /* Update mapSize for the *next* potential mapping operation */

        return 0; /* Success */
    }

    return -ENOMEM; /* mmap failed */
}

char* CsvSearchLf(char* p, size_t size, CsvHandle handle)
{
    char* end;
    char c;
    char quote_char; /* Renamed to avoid conflict if handle members were directly vars */
    /* char* res; variable `res` was only used to set to p then immediately returned if not null. Simplified. */

    end = p + size;
    quote_char = handle->quote;

    for (; p < end; p++)
    {
        c = *p;

        if (c == quote_char) {
            handle->quotes++;
        } else if (c == '\n' && !(handle->quotes & 1)) { /* Newline outside of quotes */
            return p; /* Found LF */
        }
    }
    return NULL; /* LF not found in this chunk under current quote state */
}

/* Helper function to ensure auxbuf has enough space and append data. */
/* Returns 0 on success, -ENOMEM on realloc failure. */
static int _CsvEnsureAndCopyHandleAuxbuf(CsvHandle handle, const char* data_to_copy, size_t data_size) {
    size_t required_total_size;
    void* new_mem;

    /* Calculate total size needed in auxbuf: current content + new data + null terminator */
    required_total_size = handle->auxbufPos + data_size + 1;

    if (handle->auxbufSize < required_total_size) {
        new_mem = realloc(handle->auxbuf, required_total_size);
        if (!new_mem) {
            return -ENOMEM; /* Indicate memory allocation failure */
        }
        handle->auxbuf = new_mem;
        handle->auxbufSize = required_total_size;
    }

    memcpy((char*)handle->auxbuf + handle->auxbufPos, data_to_copy, data_size);
    handle->auxbufPos += data_size;
    *((char*)handle->auxbuf + handle->auxbufPos) = '\0'; /* Null terminate the combined content */

    return 0; /* Success */
}


char* CsvReadNextRow(CsvHandle handle)
{
    int err;
    /* char* csv_chunk_to_aux_buf; This variable is not strictly needed with the helper function */
    char* res_terminator; /* Used to find the char to null-terminate */
    char* p_mapped_chunk_start; /* Start of current segment in mapped memory */
    char* found_lf; /* Pointer to LF if found */
    /* void* mem; // Used for realloc result, now encapsulated in helper */
    size_t current_mapped_chunk_len; /* Length of data in current mapped block starting at handle->pos */
    size_t segment_len_to_lf; /* Length of segment from p_mapped_chunk_start to LF (inclusive) */
    char* row_to_process; /* Pointer to the start of the line to be finalized */
    size_t len_to_process; /* Length of the line to be finalized */


    if (!handle) return NULL;

    do
    {
        err = CsvEnsureMapped(handle);
        handle->context = NULL; /* Reset column context for new row */

        if (err == -EINVAL) /* EOF or no more data to map */
        {
            /* If auxbuf has pending data from previous chunks, return it as the last line */
            if (handle->auxbufPos > 0) {
                p_mapped_chunk_start = (char*)handle->auxbuf; /* Treat auxbuf as the line */
                handle->auxbufPos = 0; /* Clear auxbuf as it's now consumed */
                /* No CR/LF to strip, it's the remaining part of the file */
                return p_mapped_chunk_start;
            }
            return NULL; /* Truly end of file */
        }
        else if (err == -ENOMEM) /* mmap failed */
        {
            return NULL; /* Critical error */
        }

        current_mapped_chunk_len = handle->size - handle->pos;
        if (!current_mapped_chunk_len && handle->auxbufPos == 0) /* No data in map and no pending data */
            return NULL; /* Should be caught by -EINVAL if fileSize reached, but defensive */

        p_mapped_chunk_start = (char*)handle->mem + handle->pos;
        found_lf = NULL;
        if (current_mapped_chunk_len > 0) { // Only search if there's data in the mapped chunk
            found_lf = CsvSearchLf(p_mapped_chunk_start, current_mapped_chunk_len, handle);
        }


        if (found_lf) /* LF found in the current mapped chunk */
        {
            segment_len_to_lf = (size_t)(found_lf - p_mapped_chunk_start) + 1; /* include LF */
            handle->pos += segment_len_to_lf; /* Advance position in mapped memory */
            handle->quotes = 0; /* Reset quote count for next line search */

            if (handle->auxbufPos > 0) /* There's a prefix in auxbuf, append current segment */
            {
                err = _CsvEnsureAndCopyHandleAuxbuf(handle, p_mapped_chunk_start, segment_len_to_lf);
                if (err) return NULL; /* realloc failed */

                row_to_process = (char*)handle->auxbuf;
                len_to_process = handle->auxbufPos; /* Total length in auxbuf */
            }
            else /* No prefix, this segment is the complete line (or start of it) */
            {
                row_to_process = p_mapped_chunk_start;
                len_to_process = segment_len_to_lf;
            }

            handle->auxbufPos = 0; /* Reset auxbuf, line is now complete */

            /* Terminate line (handle CRLF) */
            res_terminator = row_to_process + len_to_process - 1; /* Points to LF */
            if (len_to_process >= 2 && *(res_terminator - 1) == '\r') {
                res_terminator--; /* Move to CR */
            }
            *res_terminator = '\0'; /* Null-terminate */

            return row_to_process;
        }
        else /* LF not found in current mapped chunk, or chunk was empty */
        {
            if (current_mapped_chunk_len > 0) {
                 /* Append remaining part of mapped chunk to auxbuf */
                err = _CsvEnsureAndCopyHandleAuxbuf(handle, p_mapped_chunk_start, current_mapped_chunk_len);
                if (err) return NULL; /* realloc failed */
            }
            /* Mark current mapped chunk as fully processed */
            handle->pos = handle->size;
            /* Loop will call CsvEnsureMapped again for next chunk or detect EOF */
        }
    } while (1); /* Loop continues until a line is returned or NULL for EOF/error */

    /* Should not be reached due to explicit returns/breaks */
    return NULL;
}

const char* CsvReadNextCol(char* row, CsvHandle handle)
{
    char* p_read;  /* current read pointer in the row string */
    char* p_write; /* current write pointer (for in-place un-escaping) */
    char* p_col_start; /* start of the current column's content */
    int is_double_quote; /* flag for escaped quote ("") */
    int is_quoted_field; /* flag indicates if the field starts with a quote */

    if (!handle || !row) return NULL;

    p_read = handle->context ? handle->context : row;
    if (!*p_read) return NULL; /* End of row string */

    p_write = p_read;
    p_col_start = p_read;

    is_quoted_field = (*p_read == handle->quote);
    if (is_quoted_field) {
        p_read++;    /* Skip initial quote */
        p_write++;   /* Write pointer also skips initial quote space */
        p_col_start = p_read; /* Actual content starts after the quote */
    }

    for (; *p_read; p_read++)
    {
        is_double_quote = 0;

        /* Handle explicit escape character if different from quote char */
        /* Original code's escape logic: if (*p == handle->escape && p[1]) p++; */
        /* This assumes escape is different from quote. If escape IS quote, it's handled by "" */
        if (handle->escape != handle->quote && *p_read == handle->escape && p_read[1]) {
            p_read++; /* Skip escape char, next char is literal */
        }
        /* Handle CSV standard escaped quote "" */
        else if (*p_read == handle->quote && p_read[1] == handle->quote)
        {
            is_double_quote = 1;
            p_read++; /* Skip first quote, the second one will be copied */
        }

        /* Check for end of column */
        if (is_quoted_field && !is_double_quote)
        {
            if (*p_read == handle->quote) /* Closing quote of a quoted field */
                break;
        }
        else if (!is_quoted_field && *p_read == handle->delim) /* Delimiter in unquoted field */
        {
            break;
        }

        /* Copy character if needed (due to p_read advancing past escapes/quotes) */
        if (p_write != p_read)
            *p_write = *p_read;
        p_write++; /* Advance write pointer */
    }

    *p_write = '\0'; /* Null-terminate the extracted column content */

    if (!*p_read) /* Reached end of the row string */
    {
        handle->context = p_read; /* Next call will see the null terminator and return NULL */
    }
    else /* Broke on delimiter or closing quote */
    {
        if (is_quoted_field)
        {
            /* p_read currently points to the closing quote.
             * We need to advance p_read past this closing quote and any subsequent non-delimiter characters
             * (e.g. spaces after quote before delimiter, though RFC4180 is strict)
             * then past the delimiter itself.
             */
            p_read++; /* Move past the closing quote */
            while (*p_read && *p_read != handle->delim) { /* Skip any chars until delimiter */
                p_read++;
            }
            if (*p_read == handle->delim) {
                p_read++; /* Move past the delimiter for next context */
            }
            handle->context = p_read;
        }
        else /* Broke on delimiter for an unquoted field */
        {
            handle->context = p_read + 1; /* Next call starts after the delimiter */
        }
    }
    return p_col_start; /* Return start of the (potentially unescaped) column content */
}

/* (c) 2019 Jan Doczy
 * This code is licensed under MIT license (see LICENSE.txt for details) */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "csv.h"

/* Windows specific */
#ifdef _WIN32
#include <Windows.h>
typedef unsigned long long file_off_t;
#else
#include <sys/types.h>
typedef off_t file_off_t;
#endif

/* max allowed buffer */
#define BUFFER_WIDTH_APROX (40 * 1024 * 1024)

#if defined (__aarch64__) || defined (__amd64__) || defined (_M_AMD64)
/* unpack csv newline search */
#define CSV_UNPACK_64_SEARCH
#endif

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

#if defined ( __unix__ )
    int fh;
#elif defined ( _WIN32 )
    HANDLE fh;
    HANDLE fm;
#else
    #error Wrong platform definition
#endif

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

/* thin platform dependent layer so we can use file mapping
 * with winapi and oses following posix specs.
 */
#ifdef __unix__
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

#else

/* extra Windows specific implementations
 */
CsvHandle CsvOpen2(const char* filename,
                   char delim,
                   char quote,
                   char escape)
{
    LARGE_INTEGER fsize;
    SYSTEM_INFO info;
    size_t pageSize = 0;
    CsvHandle handle = calloc(1, sizeof(struct CsvHandle_));
    if (!handle)
        return NULL;

    handle->delim = delim;
    handle->quote = quote;
    handle->escape = escape;

    GetSystemInfo(&info);
    handle->blockSize = GET_PAGE_ALIGNED(BUFFER_WIDTH_APROX, info.dwPageSize);
    handle->fh = CreateFile(filename,
                            GENERIC_READ,
                            FILE_SHARE_READ,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);

    if (handle->fh == INVALID_HANDLE_VALUE)
        goto fail;

    if (GetFileSizeEx(handle->fh, &fsize) == FALSE)
        goto fail;

    handle->fileSize = fsize.QuadPart;
    if (!handle->fileSize)
        goto fail;

    handle->fm = CreateFileMapping(handle->fh, NULL, PAGE_WRITECOPY, 0, 0, NULL);
    if (handle->fm == NULL)
        goto fail;

    return handle;

fail:
    if (handle->fh != INVALID_HANDLE_VALUE)
        CloseHandle(handle->fh);

    free(handle);
    return NULL;
}

static void* MapMem(CsvHandle handle)
{
    size_t size = handle->blockSize;
    if (handle->mapSize + size > handle->fileSize)
        size = 0;  /* last chunk, extend to file mapping max */

    handle->mem = MapViewOfFileEx(handle->fm,
                                  FILE_MAP_COPY,
                                  (DWORD)(handle->mapSize >> 32),
                                  (DWORD)(handle->mapSize & 0xFFFFFFFF),
                                  size,
                                  NULL);
    return handle->mem;
}

static void UnmapMem(CsvHandle handle)
{
    if (handle->mem)
        UnmapViewOfFileEx(handle->mem, 0);
}

void CsvClose(CsvHandle handle)
{
    if (!handle)
        return;

    UnmapMem(handle);

    CloseHandle(handle->fm);
    CloseHandle(handle->fh);
    free(handle->auxbuf);
    free(handle);
}

#endif

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
            handle->size = (size_t)(handle->fileSize - (handle->mapSize - handle->blockSize)); // Corrected calculation
            if (handle->size > handle->blockSize) handle->size = handle->blockSize; // Cap at block size
            if (handle->mapSize > handle->fileSize && handle->fileSize > (handle->mapSize - handle->blockSize)) {
                 handle->size = (size_t)(handle->fileSize - (handle->mapSize - handle->blockSize));
            } else if (handle->mapSize > handle->fileSize && handle->fileSize <= (handle->mapSize - handle->blockSize)) {
                handle->size = 0; // Should not happen with correct logic, but safeguard
            } else {
                 handle->size = handle->blockSize;
            }

        return 0;
    }

    return -ENOMEM;
}

// Helper function to process a single character for newline/quote detection
// Returns the pointer to the character if it's an unquoted newline, otherwise NULL.
static char* process_char_for_newline(char* p, char c, CsvHandle handle) {
    if (c == handle->quote) {
        handle->quotes++;
    } else if (c == '\n' && !(handle->quotes & 1)) {
        return p; // Found unquoted newline
    }
    return NULL; // No unquoted newline at this position
}

char* CsvSearchLf(char* p, size_t size, CsvHandle handle)
{
    /* TODO: this can be greatly optimized by
     * using modern SIMD instructions, but for now
     * we only fetch 8Bytes "at once"
     */
    int i;
    char* res;
    char c;
    char* end = p + size;

#ifdef CSV_UNPACK_64_SEARCH
    uint64_t* pd = (uint64_t*)p;
    uint64_t* pde = pd + (size / sizeof(uint64_t));

    for (; pd < pde; pd++)
    {
        p = (char*)pd;
        for (i = 0; i < 8; i++) {
            // Process each byte in the 64-bit word using the helper
            res = process_char_for_newline(p + i, p[i], handle);
            if (res != NULL) {
                return res;
            }
        }
    }
    // Continue the byte-by-byte search from where 64-bit processing stopped
    p = (char*)pde;
#endif

    // Process remaining bytes one by one using the helper
    for (; p < end; p++)
    {
        res = process_char_for_newline(p, *p, handle);
        if (res != NULL) {
            return res;
        }
    }

    return NULL;
}

// Helper function to reallocate the auxiliary buffer if needed
// Returns 0 on success, -1 on failure.
static int realloc_aux_buffer(CsvHandle handle, size_t required_size) {
    if (handle->auxbufSize < required_size) {
        void* mem = realloc(handle->auxbuf, required_size);
        if (!mem) return -1; // Reallocation failed

        handle->auxbuf = mem;
        handle->auxbufSize = required_size;
    }
    return 0; // Success
}

// Helper to process the case when a newline is found in the current chunk.
// It handles merging with auxiliary buffer data, copying, null-termination,
// and updating the handle state. Returns the allocated row or NULL on error.
static char* process_found_row(CsvHandle handle, char* chunk_start, char* newline_pos) {
    // +1 to include the newline character itself
    size_t row_size_in_chunk = (size_t)(newline_pos - chunk_start) + 1;
    // Total size of the row including any accumulated data and the current chunk part
    size_t total_row_size = handle->auxbufPos + row_size_in_chunk;
    // Required size for the auxiliary buffer to hold the complete row + null terminator
    size_t required_aux_size = total_row_size + 1;

    // Ensure the auxiliary buffer is large enough
    if (realloc_aux_buffer(handle, required_aux_size) != 0) {
        return NULL; // Reallocation failed
    }

    // Copy the part of the current chunk up to and including the newline
    // Append it to the data already in the auxiliary buffer
    memcpy((char*)handle->auxbuf + handle->auxbufPos, chunk_start, row_size_in_chunk);
    // Update auxbufPos to reflect the total size of the complete row now stored in auxbuf
    handle->auxbufPos += row_size_in_chunk;

    // Null-terminate the complete row string in the auxiliary buffer
    // Handle potential carriage return just before the newline
    char* end_ptr = (char*)handle->auxbuf + total_row_size - 1; // Points to the last character of the row (which is '\n')
    if (total_row_size >= 1 && *end_ptr == '\n') {
        if (total_row_size >= 2 && *(end_ptr - 1) == '\r') {
            end_ptr--; // Point before the '\r'
        }
    }
     *end_ptr = '\0';


    // Update the handle's position in the mapped memory
    handle->pos += row_size_in_chunk;
    // Reset the quote count for the next row
    handle->quotes = 0;
    // Reset auxbufPos as the complete row has been extracted and is ready to be returned
    handle->auxbufPos = 0;

    // The complete row is now in the auxiliary buffer starting from the beginning
    return handle->auxbuf;
}

// Helper to accumulate the data of the current chunk into the auxiliary buffer
// when no newline is found. Updates the handle state. Returns 0 on success, -1 on failure.
static int accumulate_chunk(CsvHandle handle, char* chunk_start, size_t chunk_size) {
    // Required size for the auxiliary buffer to hold existing data + current chunk + null terminator
    size_t required_aux_size = handle->auxbufPos + chunk_size + 1;

    // Ensure the auxiliary buffer is large enough
    if (realloc_aux_buffer(handle, required_aux_size) != 0) {
        return -1; // Reallocation failed
    }

    // Copy the entire current chunk to the end of the auxiliary buffer
    memcpy((char*)handle->auxbuf + handle->auxbufPos, chunk_start, chunk_size);
    // Update auxbufPos to reflect the newly added data
    handle->auxbufPos += chunk_size;

    // Null-terminate the accumulated data in aux buffer (optional, but matches original logic)
    *(char*)((char*)handle->auxbuf + handle->auxbufPos) = '\0';

    // Update the handle's position in the mapped memory to indicate the entire chunk was consumed
    handle->pos += chunk_size;

    return 0; // Success
}


char* CsvReadNextRow(CsvHandle handle)
{
    int err;
    char* p = NULL; // Pointer to the start of the current chunk being processed
    char* found = NULL;
    size_t size;

    // Reset context for the new row
    handle->context = NULL;

    do
    {
        // Ensure that memory is mapped, mapping the next block if necessary
        err = CsvEnsureMapped(handle);

        // Handle errors during mapping
        if (err == -ENOMEM)
        {
            // Memory mapping failed
            return NULL;
        }

        // Check if the end of the file has been reached and if there's remaining data in the aux buffer
        if (err == -EINVAL)
        {
            // End of file reached. If there is any data in the auxiliary buffer,
            // treat it as the last (possibly incomplete) row.
            if (handle->auxbufPos > 0)
            {
                // Null-terminate the remaining data in auxbuf and return it.
                // Transfer ownership of the aux buffer memory.
                *(char*)((char*)handle->auxbuf + handle->auxbufPos) = '\0';
                char* last_row = handle->auxbuf;
                handle->auxbuf = NULL; // Avoid freeing transferred memory in CsvClose
                handle->auxbufSize = 0;
                handle->auxbufPos = 0; // Reset position
                return last_row;
            }
            else
            {
                // No data left in the aux buffer and end of file reached.
                // Break the loop and return NULL.
                break;
            }
        }

        // Get the size of the remaining data in the current mapped block
        size = handle->size - handle->pos;
        // Point to the start of the unread data in the mapped memory
        p = (char*)handle->mem + handle->pos;

        // If there's no data left in the current mapped block,
        // continue to the next iteration to try and map the next block.
        if (!size)
            continue;

        /* search this chunk for NL */
        found = CsvSearchLf(p, size, handle);

        if (found)
        {
            // Newline found: process the complete row.
            // This involves potentially merging with data from the auxiliary buffer.
            char* row = process_found_row(handle, p, found);
            if (!row) {
                // Error occurred during row processing (e.g., realloc failed)
                 return NULL;
            }
            return row; // Return the successfully processed row
        }
        else
        {
            // Newline not found in this chunk: accumulate the entire chunk
            // into the auxiliary buffer.
            if (accumulate_chunk(handle, p, size) != 0) {
                 // Error occurred during accumulation (e.g., realloc failed)
                 return NULL;
            }
            // The loop continues to the next iteration to map the next chunk.
        }

    } while (1); // Loop until explicitly broken or a row is returned

    // Return NULL if the loop breaks (end of file without remaining aux data or error).
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
    int dq;
    int quoted = 0; /* idicates quoted string */

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
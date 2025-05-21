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
#include <errno.h> /* For EINVAL, ENOMEM */
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

/* trivial macro used to get page-aligned buffer size */
#define GET_PAGE_ALIGNED( orig, page ) \
    (((orig) + ((page) - 1)) & ~((page) - 1))

/* Helper function to allocate and initialize common CsvHandle members */
static CsvHandle AllocateAndInitializeCsvHandle(char delim, char quote, char escape) {
    CsvHandle handle;

    handle = calloc(1, sizeof(struct CsvHandle_));
    if (!handle) {
        return NULL;
    }
    handle->delim = delim;
    handle->quote = quote;
    handle->escape = escape;

#ifdef _WIN32
    handle->fh = INVALID_HANDLE_VALUE; /* Initialize for Windows error path */
    handle->fm = NULL;                 /* Initialize for Windows error path */
#else
    handle->fh = -1; /* Initialize for Unix error path */
#endif
    return handle;
}


/* thin platform dependent layer so we can use file mapping
 * with winapi and oses following posix specs.
 */
#ifdef __unix__
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


CsvHandle CsvOpen2(const char* filename,
                   char delim,
                   char quote,
                   char escape)
{
    long pageSize;
    struct stat fs;
    CsvHandle handle;

    handle = AllocateAndInitializeCsvHandle(delim, quote, escape);
    if (!handle) {
        return NULL; 
    }

    /* page size */
    pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize < 0) {
        goto fail;
    }

    /* align to system page size */
    handle->blockSize = GET_PAGE_ALIGNED(BUFFER_WIDTH_APROX, pageSize);
    
    /* open new fd */
    handle->fh = open(filename, O_RDONLY);
    if (handle->fh < 0) {
        goto fail;
    }

    /* get real file size */
    if (fstat(handle->fh, &fs)) {
       close(handle->fh);
       handle->fh = -1; /* Mark as closed */
       goto fail;
    }
    
    handle->fileSize = fs.st_size;
    if (handle->fileSize == 0) { // Handle empty file case gracefully
        close(handle->fh);
        handle->fh = -1;
        goto fail; // Or return a handle that knows it's empty? For now, fail.
    }
    return handle;
    
  fail:
    if (handle && handle->fh >= 0) { /* Check if fh was opened before closing */
        close(handle->fh);
    }
    free(handle);
    return NULL;
}

static void* MapMem(CsvHandle handle)
{
    handle->mem = mmap(0, handle->blockSize,
                       PROT_READ | PROT_WRITE, /* PROT_WRITE for MAP_PRIVATE */
                       MAP_PRIVATE,
                       handle->fh, handle->mapSize);
    return handle->mem;
}

static void UnmapMem(CsvHandle handle)
{
    if (handle->mem) {
        munmap(handle->mem, handle->blockSize);
        handle->mem = NULL; /* Good practice after unmap/free */
    }
}

void CsvClose(CsvHandle handle)
{
    if (!handle) {
        return;
    }

    UnmapMem(handle);

    if (handle->fh >= 0) { /* Check if fh is valid before closing */
        close(handle->fh);
    }
    free(handle->auxbuf);
    free(handle);
}

#else /* _WIN32 */

/* extra Windows specific implementations
 */
CsvHandle CsvOpen2(const char* filename,
                   char delim,
                   char quote,
                   char escape)
{
    LARGE_INTEGER fsize;
    SYSTEM_INFO info;
    CsvHandle handle;

    handle = AllocateAndInitializeCsvHandle(delim, quote, escape);
    if (!handle) {
        return NULL;
    }
    
    GetSystemInfo(&info);
    handle->blockSize = GET_PAGE_ALIGNED(BUFFER_WIDTH_APROX, info.dwPageSize);
    handle->fh = CreateFile(filename, 
                            GENERIC_READ, 
                            FILE_SHARE_READ, 
                            NULL, 
                            OPEN_EXISTING, 
                            FILE_ATTRIBUTE_NORMAL, 
                            NULL);

    if (handle->fh == INVALID_HANDLE_VALUE) {
        goto fail;
    }

    if (GetFileSizeEx(handle->fh, &fsize) == FALSE) {
        goto fail;
    }

    handle->fileSize = fsize.QuadPart;
    if (!handle->fileSize) { /* Handle empty file case */
        goto fail;
    }

    handle->fm = CreateFileMapping(handle->fh, NULL, PAGE_WRITECOPY, 0, 0, NULL);
    if (handle->fm == NULL) {
        goto fail;
    }

    return handle;

fail:
    if (handle) { /* Ensure handle exists before trying to access its members */
        if (handle->fm != NULL) { /* fm created before fh for cleanup order */
             CloseHandle(handle->fm);
        }
        if (handle->fh != INVALID_HANDLE_VALUE) {
            CloseHandle(handle->fh);
        }
    }
    free(handle);
    return NULL;
}

static void* MapMem(CsvHandle handle)
{
    size_t size_to_map;
    
    size_to_map = handle->blockSize;
    if (handle->mapSize + size_to_map > handle->fileSize) {
        /* last chunk, map only remaining. If mapSize already >= fileSize, map 0. */
        if (handle->mapSize >= handle->fileSize) {
            size_to_map = 0;
        } else {
            size_to_map = (size_t)(handle->fileSize - handle->mapSize);
        }
    }
    /* The original logic: if (handle->mapSize + size > handle->fileSize) size = 0;
     * This `size = 0` for MapViewOfFileEx means map up to end of file mapping object.
     * If fileSize is small, mapSize might be 0, and we want to map fileSize.
     * The size parameter of MapViewOfFileEx:
     * "The number of bytes of a file mapping to map to the view.
     *  All bytes must be within the maximum size specified by CreateFileMapping.
     *  If this parameter is 0 (zero), the mapping extends from the specified offset to the end of the file mapping."
     * So if we provide a specific size, it should be actual remaining, or block size.
     * If last chunk, we want to map up to fileSize.
     * The original `size = 0` might be relying on CreateFileMapping with dwMaximumSizeHigh/Low as 0 (map entire file).
     */

    handle->mem = MapViewOfFileEx(handle->fm, 
                                  FILE_MAP_COPY,
                                  (DWORD)(handle->mapSize >> 32),
                                  (DWORD)(handle->mapSize & 0xFFFFFFFF),
                                  size_to_map, /* Use calculated size_to_map or 0 for "to end" */
                                  NULL);
    return handle->mem;
}

static void UnmapMem(CsvHandle handle)
{
    if (handle->mem) {
        UnmapViewOfFileEx(handle->mem, 0); /* 0 for dwNumberOfBytesToUnmap is not documented standard,
                                             but often implies unmap whole view.
                                             It's safer to unmap the size it was mapped with if known,
                                             or use UnmapViewOfFile(handle->mem) if mapping address only.
                                             Since MapViewOfFileEx doesn't require size on unmap, this is fine.
                                          */
        handle->mem = NULL; /* Good practice */
    }
}

void CsvClose(CsvHandle handle)
{
    if (!handle) {
        return;
    }

    UnmapMem(handle);

    if (handle->fm != NULL) {
        CloseHandle(handle->fm);
    }
    if (handle->fh != INVALID_HANDLE_VALUE) {
        CloseHandle(handle->fh);
    }
    free(handle->auxbuf);
    free(handle);
}

#endif

static int CsvEnsureMapped(CsvHandle handle)
{
    file_off_t newMapSizeAttempt;
    size_t actual_read_size;
    
    /* do not need to map */
    if (handle->pos < handle->size) {
        return 0;
    }

    UnmapMem(handle);  
    /* handle->mem is now NULL */

    if (handle->mapSize >= handle->fileSize) {
        return -EINVAL; /* No more data to map */
    }

    /* Try to map a new block */
    /* newMapSizeAttempt is where the next block would logically END if mapped */
    /* handle->mapSize is the current OFFSET for mapping */
    if (MapMem(handle)) { /* MapMem uses handle->mapSize as offset and handle->blockSize as desired size */
        handle->pos = 0;
        
        /* Calculate how much was actually made available by MapMem */
        /* If MapMem mapped less than blockSize (e.g., end of file) */
        if (handle->mapSize + handle->blockSize > handle->fileSize) {
            actual_read_size = (size_t)(handle->fileSize - handle->mapSize);
        } else {
            actual_read_size = handle->blockSize;
        }
        handle->size = actual_read_size;
        handle->mapSize += actual_read_size; /* Update mapSize to reflect new end of mapped region */
        
        return 0;
    }
    
    return -ENOMEM; /* Failed to map memory */
}

/* Helper function to process a character and find a newline */
static char* ProcessCharacterAndFindNewline(char current_char, char* char_pos, CsvHandle handle) {
    if (current_char == handle->quote) {
        handle->quotes++;
    } else if (current_char == '\n' && !(handle->quotes & 1)) {
        return char_pos; /* Newline found */
    }
    return NULL; /* Newline not found by this char */
}

char* CsvSearchLf(char* p_arg, size_t size_arg, CsvHandle handle)
{
    char* p;
    char* end;
    char* found_nl;
    int i;
#ifdef CSV_UNPACK_64_SEARCH
    uint64_t* pd;
    uint64_t* pde;
#endif

    p = p_arg;
    end = p_arg + size_arg;
    found_nl = NULL;

#ifdef CSV_UNPACK_64_SEARCH
    pd = (uint64_t*)p;
    pde = pd + (size_arg / sizeof(uint64_t));

    for (; pd < pde; pd++) {
        char* current_chunk_ptr = (char*)pd;
        for (i = 0; i < 8; i++) {
            found_nl = ProcessCharacterAndFindNewline(current_chunk_ptr[i], current_chunk_ptr + i, handle);
            if (found_nl != NULL) {
                return found_nl;
            }
        }
    }
    p = (char*)pde; /* Advance p to the end of the 64-bit processed part */
#endif
    
    for (; p < end; p++) {
        found_nl = ProcessCharacterAndFindNewline(*p, p, handle);
        if (found_nl != NULL) {
            return found_nl;
        }
    }

    return NULL;
}

/* Helper function to append data to the auxiliary buffer */
static int AppendToAuxBuffer(CsvHandle handle, const char* chunk_to_append, size_t chunk_size) {
    size_t new_required_size;
    void* new_auxbuf;

    new_required_size = handle->auxbufPos + chunk_size + 1; /* +1 for null terminator */
    if (handle->auxbufSize < new_required_size) {
        new_auxbuf = realloc(handle->auxbuf, new_required_size);
        if (!new_auxbuf) {
            return -1; /* Indicate realloc failure */
        }
        handle->auxbuf = new_auxbuf;
        handle->auxbufSize = new_required_size;
    }

    memcpy((char*)handle->auxbuf + handle->auxbufPos, chunk_to_append, chunk_size);
    handle->auxbufPos += chunk_size;
    *((char*)handle->auxbuf + handle->auxbufPos) = '\0';

    return 0; /* Indicate success */
}

/* Helper function to terminate a row string, handling CRLF and LF */
static void TerminateRowString(char* row_data, size_t length_with_newline) {
    char* terminator_pos;

    if (length_with_newline == 0) {
        if (row_data) *row_data = '\0'; /* Handle empty string case */
        return;
    }

    terminator_pos = row_data + length_with_newline - 1; /* Points to \n */
    if (length_with_newline >= 2 && *(row_data + length_with_newline - 2) == '\r') {
        terminator_pos--; /* Move back to \r if CRNL */
    }
    *terminator_pos = '\0';
}


char* CsvReadNextRow(CsvHandle handle)
{
    int err;
    char* current_chunk_ptr; /* Renamed from p for clarity inside loop */
    char* newline_found_pos; /* Renamed from found */
    size_t current_chunk_scan_size; /* Renamed from size for clarity */
    size_t segment_len;


    current_chunk_ptr = NULL; /* Initialize to avoid using uninitialized 'p' in first 'if (p == NULL)' check */
    newline_found_pos = NULL;

    do
    {
        err = CsvEnsureMapped(handle);
        handle->context = NULL; /* Reset column context for new row */

        if (err == -EINVAL) { /* End of file reached, or no more data can be mapped */
            if (handle->auxbufPos > 0) { /* If there's remaining data in auxbuf */
                /* Null termination is already handled by AppendToAuxBuffer */
                current_chunk_ptr = handle->auxbuf; /* Use current_chunk_ptr for return */
                handle->auxbufPos = 0; /* Consume auxbuf */
                return current_chunk_ptr;
            }
            return NULL; /* No more rows */
        } else if (err == -ENOMEM) {
            return NULL; /* Memory mapping failed */
        }

        current_chunk_scan_size = handle->size - handle->pos;
        if (!current_chunk_scan_size && !handle->auxbufPos) { /* No data in current map, no data in auxbuf */
             if (handle->auxbufPos > 0) { /* Should have been caught by EINVAL case if auxbufPos was > 0 */
                current_chunk_ptr = handle->auxbuf;
                handle->auxbufPos = 0;
                return current_chunk_ptr;
            }
            return NULL;
        }
        if (!current_chunk_scan_size && handle->auxbufPos > 0) { // No more data from file, but auxbuf has content (partial last line)
            current_chunk_ptr = handle->auxbuf;
            handle->auxbufPos = 0; // Consume auxbuf
            // No newline, so it's the last partial line. Terminate it as is.
            // AppendToAuxBuffer already null-terminates.
            return current_chunk_ptr;
        }


        current_chunk_ptr = (char*)handle->mem + handle->pos;
        newline_found_pos = CsvSearchLf(current_chunk_ptr, current_chunk_scan_size, handle);

        if (newline_found_pos) {
            segment_len = (size_t)(newline_found_pos - current_chunk_ptr) + 1; /* Length including \n */
            handle->pos += segment_len;
            handle->quotes = 0; /* Reset for next CsvSearchLf call in this row, if any (not typical) or next row */

            if (handle->auxbufPos > 0) { /* If there's data in auxbuf, prepend it */
                if (AppendToAuxBuffer(handle, current_chunk_ptr, segment_len) != 0) {
                    return NULL; /* realloc failed */
                }
                /* TerminateRowString will operate on handle->auxbuf */
                TerminateRowString(handle->auxbuf, handle->auxbufPos);
                current_chunk_ptr = handle->auxbuf; /* Point to the combined string */
                 /* The content of auxbuf is the full row. Set auxbufPos to 0 to indicate it's consumed for this row. */
                handle->auxbufPos = 0;
            } else {
                /* No auxbuf, use current_chunk_ptr directly. Need to make a writable copy or terminate in place if possible. */
                /* Since mem is mapped MAP_PRIVATE (effectively copy-on-write) or PAGE_WRITECOPY, we can write. */
                TerminateRowString(current_chunk_ptr, segment_len);
            }
            return current_chunk_ptr; /* Return pointer to the start of the row string */
        }
        else { /* Newline not found in current_chunk_scan_size */
            /* Append entire current_chunk_scan_size to auxbuf */
            if (AppendToAuxBuffer(handle, current_chunk_ptr, current_chunk_scan_size) != 0) {
                return NULL; /* realloc failed */
            }
            handle->pos = handle->size; /* Mark entire mapped chunk as processed */
        }
    } while (!newline_found_pos); /* Loop if newline not found, to map more data */

    /* Should be unreachable if logic is correct, CsvEnsureMapped handles EOF with auxbuf */
    if (handle->auxbufPos > 0) { // If loop exited due to some other reason but auxbuf has data
        current_chunk_ptr = handle->auxbuf;
        handle->auxbufPos = 0;
        return current_chunk_ptr; // Return remaining content
    }

    return NULL;
}

/*
 * Helper function to parse the content of a field, handling escapes and quotes.
 * Advances p_read_ptr and p_write_ptr.
 * Returns the character that terminated the field parsing (*p_read_ptr at the end).
 */
static char ParseFieldContentAndAdvance(char** p_read_ptr, char** p_write_ptr, CsvHandle handle, int is_quoted_field) {
    char* p; /* Current read position */
    char* d; /* Current write position */
    char current_char_val;
    int is_double_quote_char; /* Indicates if current quote is part of a double quote "" */

    p = *p_read_ptr;
    d = *p_write_ptr;

    for (;; p++, d++) {
        current_char_val = *p;
        is_double_quote_char = 0;

        if (current_char_val == '\0') { /* End of the row string */
            break;
        }

        if (current_char_val == handle->escape && p[1]) {
            p++; /* Skip escape character */
            current_char_val = *p; /* Get the escaped character */
        } else if (current_char_val == handle->quote && p[1] == handle->quote) {
            is_double_quote_char = 1;
            p++; /* Skip the first quote of the pair */
            current_char_val = *p; /* Get the second quote (which is the data) */
        }

        /* Check for field termination conditions */
        if (is_quoted_field) {
            if (!is_double_quote_char && current_char_val == handle->quote) {
                break; /* End of quoted field (closing quote) */
            }
        } else { /* Not a quoted field */
            if (current_char_val == handle->delim) {
                break; /* End of field (delimiter) */
            }
        }
        
        /* Copy the character to the destination buffer */
        /* If d!=p was for optimization, this simplified direct copy is fine.
           If d points to where data should be written and p is source.
        */
        *d = current_char_val;
    }

    *p_read_ptr = p;   /* Update the caller's read pointer */
    *p_write_ptr = d;  /* Update the caller's write pointer */
    return *p;         /* Return the character p stopped on (terminator or '\0') */
}


const char* CsvReadNextCol(char* row, CsvHandle handle)
{
    char* p_read;       /* Read pointer */
    char* p_write;      /* Write pointer (destination) */
    char* field_start;  /* Beginning of the current field in the row buffer (return value) */
    int is_quoted;    /* Indicates if the field is quoted */
    char terminating_char;
    char* initial_p_read;


    p_read = handle->context ? handle->context : row;
    initial_p_read = p_read; /* Store initial p_read for empty check at EOL */

    if (*p_read == '\0') { /* Already at the end of the row string */
        return NULL;
    }

    p_write = p_read; /* Destination writes over the source buffer */
    field_start = p_write;

    is_quoted = (*p_read == handle->quote);
    if (is_quoted) {
        p_read++; /* Source pointer skips the opening quote */
                  /* p_write (destination) does not skip; content will overwrite the opening quote */
    }

    terminating_char = ParseFieldContentAndAdvance(&p_read, &p_write, handle, is_quoted);
    
    /* p_read now points to the terminating char (delim, quote, or '\0') */
    /* p_write now points to where the null terminator for the extracted field should go */

    if (terminating_char == '\0') { /* End of row string reached */
        /* If p_read is still at its initial position AND it's EOL, means the input context was empty. */
        if (p_read == initial_p_read) { 
            return NULL; /* No more fields */
        }
        handle->context = p_read; /* Context is now at the end of the string */
    } else { /* Field ended by a delimiter or a closing quote */
        *p_write = '\0'; /* Terminate the extracted field. p_write is already positioned after last char.*/
        
        if (is_quoted) { 
            /* p_read is currently on the closing quote. Need to advance past it. */
            p_read++; 
            /* Skip any characters between the closing quote and the next delimiter or EOL */
            /* RFC4180 is strict: nothing should follow quote before delimiter. This parser is more lenient. */
            while (*p_read && *p_read != handle->delim) {
                p_read++;
            }
            if (*p_read == handle->delim) {
                p_read++; /* Advance past delimiter for next call */
            }
            handle->context = p_read;
        } else { /* Field ended with a delimiter (terminating_char == handle->delim) */
            /* p_read is currently on the delimiter. */
            handle->context = p_read + 1; /* Next field starts after the delimiter */
        }
    }
    *p_write = '\0'; /* Ensure termination, p_write is where it should be */
    return field_start;
}

CsvHandle CsvOpen(const char* filename)
{
    /* defaults */
    return CsvOpen2(filename, ',', '"', '\\');
}
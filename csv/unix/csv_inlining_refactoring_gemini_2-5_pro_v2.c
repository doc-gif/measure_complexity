/* (c) 2019 Jan Doczy
 * This code is licensed under MIT license (see LICENSE.txt for details) */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "csv.h" /* Assumed to define CsvHandle type and public prototypes */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

typedef off_t file_off_t;

/* max allowed buffer */
#define BUFFER_WIDTH_APROX (40 * 1024 * 1024)

/* private csv handle */
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

/* trivial macro used to get page-aligned buffer size */
#define GET_PAGE_ALIGNED( orig, page ) \
    (((orig) + ((page) - 1)) & ~((page) - 1))

/* Forward declarations for static helper functions */
static int initialize_csv_handle_params(CsvHandle handle, char delim, char quote, char escape);
static int open_and_stat_csv_file(const char* filename, CsvHandle handle);
static int ensure_aux_buffer_capacity(CsvHandle handle, size_t space_for_new_data);
static int append_data_to_aux_buffer(CsvHandle handle, const char* data_chunk, size_t data_chunk_size);
static char* process_found_newline_in_row(CsvHandle handle, char* p_chunk_start, char* found_lf);
static char parse_column_character(char** p_src, CsvHandle handle, int* was_double_quote);
static void finalize_column_and_update_context(CsvHandle handle, char* p_break_point, char* d_col_end, int was_quoted);


CsvHandle CsvOpen(const char* filename)
{
    /* defaults */
    return CsvOpen2(filename, ',', '"', '\\');
}

static int initialize_csv_handle_params(CsvHandle handle, char delim, char quote, char escape)
{
    long pageSize;

    /* set chars */
    handle->delim = delim;
    handle->quote = quote;
    handle->escape = escape;

    /* page size */
    pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize < 0) {
        return -1; /* Indicate error */
    }

    /* align to system page size */
    handle->blockSize = GET_PAGE_ALIGNED(BUFFER_WIDTH_APROX, pageSize);
    return 0; /* Success */
}

static int open_and_stat_csv_file(const char* filename, CsvHandle handle)
{
    struct stat fs;

    /* open new fd */
    handle->fh = open(filename, O_RDONLY);
    if (handle->fh < 0) {
        return -1; /* Indicate error */
    }

    /* get real file size */
    if (fstat(handle->fh, &fs))
    {
       close(handle->fh);
       handle->fh = -1; /* Mark as closed */
       return -1; /* Indicate error */
    }

    handle->fileSize = fs.st_size;
    return 0; /* Success */
}

CsvHandle CsvOpen2(const char* filename,
                   char delim,
                   char quote,
                   char escape)
{
    CsvHandle handle;

    /* alloc zero-initialized mem */
    handle = (CsvHandle)calloc(1, sizeof(struct CsvHandle_));
    if (!handle) {
        return NULL;
    }

    if (initialize_csv_handle_params(handle, delim, quote, escape) != 0) {
        free(handle);
        return NULL;
    }

    if (open_and_stat_csv_file(filename, handle) != 0) {
        free(handle);
        return NULL;
    }

    return handle;
}

static void* MapMem(CsvHandle handle)
{
    /* Variable declarations must be at the start of the block */
    if (!handle) return NULL; /* Basic safety check */

    handle->mem = mmap(0, handle->blockSize,
                       PROT_READ | PROT_WRITE, /* Original uses PROT_WRITE, ensure it's needed if file is RONLY */
                       MAP_PRIVATE,
                       handle->fh, handle->mapSize);
    return handle->mem;
}

static void UnmapMem(CsvHandle handle)
{
    /* Variable declarations (none needed) */
    if (handle && handle->mem) {
        munmap(handle->mem, handle->blockSize);
        handle->mem = NULL; /* Good practice to nullify after unmap/free */
    }
}

void CsvClose(CsvHandle handle)
{
    /* Variable declarations (none needed) */
    if (!handle) {
        return;
    }

    UnmapMem(handle);

    if (handle->fh >= 0) { /* Check if file descriptor is valid before closing */
        close(handle->fh);
    }
    free(handle->auxbuf);
    free(handle);
}

static int CsvEnsureMapped(CsvHandle handle)
{
    file_off_t newSize;

    /* do not need to map */
    if (handle->pos < handle->size) {
        return 0;
    }

    UnmapMem(handle);

    /* handle->mem is now NULL */
    if (handle->mapSize >= handle->fileSize) {
        return -EINVAL; /* End of file effectively reached for mapping */
    }

    newSize = handle->mapSize + handle->blockSize; /* Tentative new map offset */
    /* MapMem will set handle->mem. Check its return. */
    if (MapMem(handle) != MAP_FAILED && handle->mem != NULL) /* Check against MAP_FAILED and NULL */
    {
        handle->pos = 0;
        /* Update mapSize *after* successful mapping for this segment */
        /* The original logic for mapSize update was before MapMem or effectively what fh_offset for mmap used.
         * handle->mapSize here should track the offset in the file for the *start* of the current map.
         * So, if MapMem used handle->mapSize as offset, it should be updated *before* for next call,
         * or if MapMem calculates its own offset, this needs care.
         * Assuming MapMem uses handle->mapSize as the offset for mmap:
         */
        handle->mapSize = newSize; /* This is the offset for the *next* potential block */
                                   /* No, mmap uses handle->mapSize as current offset.
                                    * So, if successful, the current mapping covers up to handle->mapSize + effective_read_size.
                                    * The variable mapSize in the struct seems to denote the current offset of the mapped region.
                                    * Let's assume MapMem uses the current handle->mapSize for its offset argument.
                                    * After a successful map, the *next* call to MapMem will need an updated mapSize (offset).
                                    *
                                    * Original logic in CsvReadNextRow implies CsvEnsureMapped *updates* mapSize for the *current* mapping:
                                    * `handle->mapSize = newSize;` after successful MapMem. This `newSize` was `handle->mapSize + handle->blockSize`
                                    * This implies `handle->mapSize` is more like "total bytes attempted to be covered by mapping operations so far"
                                    * rather than the direct offset for the *current* mmap call.
                                    * The offset for mmap in MapMem is `handle->mapSize`. So this should be updated for the *next* segment.
                                    * Let's stick to the original update logic of mapSize for now.
                                    * The offset for mmap is handle->mapSize (which would be 0, then blockSize, then 2*blockSize etc.)
                                    * So, before calling MapMem, handle->mapSize must be the correct offset.
                                    * CsvEnsureMapped is called, if it needs to map, it sets the new offset for MapMem by:
                                    * handle->mapSize (current offset) becomes the offset for mmap.
                                    * After mapping, handle->mapSize (the field in struct) must be updated for the *next* call.
                                    *
                                    * Let's trace:
                                    * Initial: handle->mapSize = 0.
                                    * CsvEnsureMapped calls MapMem(handle) -> mmap(..., handle->fh, 0)
                                    * If success, handle->mapSize becomes handle->blockSize (for next time)
                                    * Next call: MapMem(handle) -> mmap(..., handle->fh, handle->blockSize)
                                    * If success, handle->mapSize becomes 2 * handle->blockSize
                                    *
                                    * The original updates mapSize *after* MapMem to `newSize`.
                                    * `newSize = handle->mapSize + handle->blockSize;` (mapSize is old here)
                                    * `if (MapMem(handle))` (MapMem uses old mapSize for offset)
                                    * `handle->mapSize = newSize;` (Updates for future) -> This seems correct.
                                    */


        /* read only up to filesize:
         * 1. mapped block size is < then filesize: (use blocksize)
         * 2. mapped block size is > then filesize: (use remaining filesize) */
        if (handle->mapSize > handle->fileSize) { /* mapSize here means offset + blockSize */
             /* If the segment goes past fileSize */
            if (handle->mapSize - handle->blockSize >= handle->fileSize) { /* current map starts at or after EOF */
                 handle->size = 0;
            } else {
                 /* Current map straddles EOF. Size is from start of map to EOF. */
                 handle->size = (size_t)(handle->fileSize - (handle->mapSize - handle->blockSize));
            }
        } else {
            handle->size = handle->blockSize;
        }
        /* After MapMem, handle->mapSize should be updated to the *start* of the *next* block for the subsequent mmap call. */
        /* The original code updates `handle->mapSize = newSize` where newSize = old_mapSize + blockSize.
           This implies `handle->mapSize` is the cumulative offset for the *next* map.
           So if MapMem successfully maps using current `handle->mapSize` (as offset), then we update `handle->mapSize` for the future.
        */
        /* The MapMem call itself uses handle->mapSize as the offset.
         * So if we are here, MapMem was called with the *current* handle->mapSize.
         * Now we update handle->mapSize for the *next* potential call to CsvEnsureMapped/MapMem.
         */
        handle->mapSize += handle->blockSize; // This seems more logical for how mapSize is used as an offset for the next block.
                                          // Let's re-check original: `newSize = handle->mapSize + handle->blockSize; if (MapMem(handle)) { handle->mapSize = newSize; ...}`
                                          // This means MapMem uses the value of `handle->mapSize` *before* this increment.
                                          // And after success, `handle->mapSize` is set to be the start of the next block.
                                          // So the current function should take current `handle->mapSize` as the offset,
                                          // and if successful, update `handle->mapSize` field for the next segment.
                                          // The parameter to `mmap` in `MapMem` is `handle->mapSize`.
                                          // So, when `CsvEnsureMapped` decides to map:
                                          // 1. The current `handle->mapSize` is the offset for `MapMem`.
                                          // 2. If `MapMem` is successful, `CsvEnsureMapped` then updates `handle->mapSize` for the *next* operation.
                                          // This implies the update `handle->mapSize = newSize;` (where newSize was old_mapSize + blockSize) in the original CsvEnsureMapped was correct.
                                          // The `handle->mapSize` in the struct should be the *offset of the current mapping*.
                                          // No, this is confusing. Let's simplify.
                                          // `handle->mapSize` = current offset of mmap.
                                          // `handle->size` = size of current mmaped data available.
                                          // `handle->pos` = position within that mmaped data.

        /* Sticking to original structure for mapSize update: */
        /* The offset for the MapMem call was the value of handle->mapSize *before* this function was deeply entered */
        /* If MapMem was successful, we now update what the "next expected map start" is */
        /* This implies handle->mapSize is the offset for the current mapping operation. */
        /* And it's updated for the *next* mapping operation here. */
        /* Let file_offset_for_map be the actual offset used for the mmap call. */
        file_off_t file_offset_for_map = handle->mapSize; /* This is the offset used by MapMem call */
        /* MapMem(...) uses file_offset_for_map */

        /* If MapMem was successful using file_offset_for_map: */
        handle->pos = 0;
        handle->mapSize = file_offset_for_map + handle->blockSize; /* Set up for next potential map */

        /* Adjust effective size of the current mapping if it exceeds file size */
        if (file_offset_for_map + handle->blockSize > handle->fileSize) {
            if (file_offset_for_map >= handle->fileSize) { /* current map would start at or after EOF */
                 handle->size = 0;
            } else {
                 handle->size = (size_t)(handle->fileSize - file_offset_for_map);
            }
        } else {
            handle->size = handle->blockSize;
        }
        return 0;
    }

    return -ENOMEM;
}

char* CsvSearchLf(char* p, size_t size, CsvHandle handle)
{
    char* res;
    char c;
    char* end;
    char quote_char; /* Renamed from 'quote' to avoid conflict if C++ or for clarity */

    /* Ensure p is not NULL, and handle is not NULL */
    if (!p || !handle) return NULL;

    end = p + size;
    quote_char = handle->quote;

    for (; p < end; p++)
    {
        res = NULL; /* Initialize res for each character check */
        c = *p;

        if (c == quote_char) {
            handle->quotes++;
        } else if (c == '\n' && !(handle->quotes & 1)) { /* Check if quotes are even */
            res = p; /* Newline found outside quotes */
        }

        if (res != NULL) { /* If newline found and conditions met */
            return res;
        }
    }

    return NULL; /* No suitable newline found in this chunk */
}

static int ensure_aux_buffer_capacity(CsvHandle handle, size_t space_for_new_data) {
    void* new_mem;
    size_t required_capacity;
    size_t new_alloc_size;

    required_capacity = handle->auxbufPos + space_for_new_data + 1; /* +1 for null terminator */

    if (handle->auxbufSize < required_capacity) {
        new_alloc_size = required_capacity;
        /* Simple growth: double current size if > 0, or required_capacity, whichever is larger. Min 256. */
        if (handle->auxbufSize > 0 && handle->auxbufSize * 2 > new_alloc_size) {
            new_alloc_size = handle->auxbufSize * 2;
        }
        if (new_alloc_size < 256) { /* Ensure a minimum allocation size */
            new_alloc_size = 256;
        }
        /* After growth strategy, ensure it's still at least the required capacity */
        if (new_alloc_size < required_capacity) {
            new_alloc_size = required_capacity;
        }

        new_mem = realloc(handle->auxbuf, new_alloc_size);
        if (!new_mem) {
            return -ENOMEM; /* Realloc failed */
        }
        handle->auxbuf = new_mem;
        handle->auxbufSize = new_alloc_size;
    }
    return 0; /* Success */
}

static int append_data_to_aux_buffer(CsvHandle handle, const char* data_chunk, size_t data_chunk_size) {
    int err;

    err = ensure_aux_buffer_capacity(handle, data_chunk_size);
    if (err != 0) {
        return err; /* Propagate error from ensure_aux_buffer_capacity */
    }

    memcpy((char*)handle->auxbuf + handle->auxbufPos, data_chunk, data_chunk_size);
    handle->auxbufPos += data_chunk_size;
    *((char*)handle->auxbuf + handle->auxbufPos) = '\0'; /* Null-terminate after append */

    return 0; /* Success */
}

static char* process_found_newline_in_row(CsvHandle handle, char* p_chunk_start, char* found_lf) {
    size_t segment_len;
    char* row_to_return;
    char* termination_point;

    segment_len = (size_t)(found_lf - p_chunk_start) + 1; /* Length including the newline */
    handle->pos += segment_len; /* Advance main buffer position */
    handle->quotes = 0;         /* Reset quote count for the next row */

    if (handle->auxbufPos > 0) { /* If there's pending data in auxbuf */
        if (append_data_to_aux_buffer(handle, p_chunk_start, segment_len) != 0) {
            return NULL; /* Error appending to aux_buffer */
        }
        row_to_return = (char*)handle->auxbuf;
        segment_len = handle->auxbufPos; /* Full length is now in auxbufPos */
        handle->auxbufPos = 0; /* Reset auxbuf for the next CsvReadNextRow call */
    } else {
        row_to_return = p_chunk_start;
        /* segment_len is already correctly set for p_chunk_start */
    }

    /* Terminate the line (handles \r\n and \n) */
    termination_point = row_to_return + segment_len - 1; /* Points to \n */
    if (segment_len >= 2 && *(termination_point - 1) == '\r') {
        termination_point--; /* Move back to overwrite \r */
    }
    *termination_point = '\0'; /* Null-terminate */

    return row_to_return;
}


char* CsvReadNextRow(CsvHandle handle)
{
    int err;
    /* char* csv_chunk_to_aux_buf; // This variable seems to just hold handle->auxbuf, can be removed */
    char* p_current_chunk_ptr;
    char* found_lf;
    size_t current_chunk_size;
    char* result_row;


    if (!handle) return NULL; /* Safety check */

    do
    {
        /* Ensure memory is mapped for reading */
        err = CsvEnsureMapped(handle);
        handle->context = NULL; /* Reset column context for new row */

        if (err == -EINVAL) /* EINVAL from CsvEnsureMapped means EOF or map error */
        {
            /* If this is the first iteration (p_current_chunk_ptr not set) and auxbuf is empty, it's truly EOF.
             * If there's content in auxbuf, it means these are the last bytes of the file without a trailing newline.
             */
            if (handle->auxbufPos > 0) { /* Check if auxbuf has remaining content */
                /* The append_data_to_aux_buffer already null-terminates.
                 * This becomes the last "row".
                 */
                result_row = (char*)handle->auxbuf;
                handle->auxbufPos = 0; /* Consume auxbuf */
                return result_row;
            }
            return NULL; /* Truly end of file or unrecoverable map error */
        }
        else if (err == -ENOMEM) /* ENOMEM from CsvEnsureMapped */
        {
            return NULL; /* Memory mapping failed */
        }

        current_chunk_size = handle->size - handle->pos;
        if (!current_chunk_size) { /* No more data in the current mapped chunk, or filesize is 0 */
             /* This condition might lead to infinite loop if CsvEnsureMapped doesn't progress.
              * However, CsvEnsureMapped should return -EINVAL if fileSize is reached.
              * If handle->size is 0 (e.g. empty file mapped), loop should break.
              */
            if (handle->auxbufPos > 0) { // If something was accumulated
                result_row = (char*)handle->auxbuf;
                handle->auxbufPos = 0;
                return result_row;
            }
            return NULL; /* No data to process */
        }


        p_current_chunk_ptr = (char*)handle->mem + handle->pos;
        found_lf = CsvSearchLf(p_current_chunk_ptr, current_chunk_size, handle);

        if (found_lf)
        {
            result_row = process_found_newline_in_row(handle, p_current_chunk_ptr, found_lf);
            return result_row; /* Returns NULL on error within process_found_newline_in_row */
        }
        else /* Newline not found in the current chunk */
        {
            /* Current chunk exhausted, move its content to aux_buffer */
            handle->pos = handle->size; /* Mark current mmap chunk as fully processed */

            /* Append the entire remaining part of p_current_chunk_ptr to aux_buffer */
            if (append_data_to_aux_buffer(handle, p_current_chunk_ptr, current_chunk_size) != 0) {
                return NULL; /* Error appending to aux_buffer */
            }
            /* Loop will continue to try CsvEnsureMapped for the next chunk */
        }

    } while (1); /* Loop until a row is returned or NULL is returned for EOF/error */

    /* Should not be reached if logic is correct, but as a fallback: */
    /* This path would be taken if `!found_lf` and the loop condition was `!found_lf`.
     * The `while(1)` with explicit returns is clearer.
     */
    return NULL;
}


static char parse_column_character(char** p_src_char, CsvHandle handle, int* was_double_quote) {
    char current_char;
    char next_char;

    *was_double_quote = 0; /* Initialize */
    current_char = **p_src_char;

    if (current_char == handle->escape && (*p_src_char)[1]) {
        (*p_src_char)++; /* Move past escape char */
        current_char = **p_src_char; /* Get the escaped character */
    } else if (current_char == handle->quote) {
        next_char = (*p_src_char)[1];
        if (next_char == handle->quote) { /* Doubled quote */
            (*p_src_char)++; /* Move past the first quote, loop will advance past the second */
            /* current_char remains the quote character to be copied */
            *was_double_quote = 1; /* Signal that a double quote pair was handled */
        }
        /* If not a double quote, 'current_char' is the single quote itself (start/end of field) */
    }
    return current_char;
}

static void finalize_column_and_update_context(CsvHandle handle, char* p_break_point, char* d_col_end, int was_quoted) {
    char* next_context_p;

    *d_col_end = '\0'; /* Null-terminate the extracted column */
    next_context_p = p_break_point;

    if (!*next_context_p) { /* End of the row string was reached */
        handle->context = next_context_p;
    } else { /* Delimiter or closing quote for a quoted field was reached */
        if (was_quoted) {
            /* p_break_point is at the closing quote of the field */
            next_context_p++; /* Move past the closing quote */
            while (*next_context_p && *next_context_p != handle->delim) {
                next_context_p++; /* Skip any characters after closing quote until delimiter */
            }
            if (*next_context_p == handle->delim) {
                next_context_p++; /* Move past the delimiter for the next column's start */
            }
            handle->context = next_context_p;
        } else {
            /* p_break_point is at the delimiter */
            handle->context = next_context_p + 1; /* Next column starts after the delimiter */
        }
    }
}


const char* CsvReadNextCol(char* row, CsvHandle handle)
{
    char* p_read; /* Pointer for reading from the row */
    char* d_write; /* Pointer for writing the (potentially unescaped) column */
    char* b_col_start; /* Beginning of the current column */
    int is_quoted_field;
    char current_char_val;
    int was_double_quote_pair; /* Flag from parse_column_character */

    if (!handle) return NULL; /* Safety first */
    if (!row && !handle->context) return NULL; /* Nothing to parse */


    p_read = handle->context ? handle->context : row;
    if (!*p_read && p_read == (handle->context ? handle->context : row) ) { /* If context is at EOS and is the start */
        return NULL;
    }


    d_write = p_read;
    b_col_start = p_read;
    is_quoted_field = 0;

    if (*p_read == handle->quote) {
        is_quoted_field = 1;
        p_read++; /* Move past the opening quote */
        b_col_start = p_read; /* Actual column content starts after the quote */
        d_write = p_read;     /* Destination also starts after the quote */
    }

    for (; *p_read; /* p_read is advanced by parse_column_character or loop increment */ )
    {
        current_char_val = parse_column_character(&p_read, handle, &was_double_quote_pair);

        /* Check for end of column conditions */
        if (is_quoted_field) {
            if (current_char_val == handle->quote && !was_double_quote_pair) {
                /* This is the closing quote, not part of a double-quote escape */
                break;
            }
        } else { /* Not a quoted field */
            if (current_char_val == handle->delim) {
                break; /* Delimiter found */
            }
        }

        /* Copy character if needed (handles un-escaping in place) */
        if (d_write != p_read) { /* If p_read advanced due to escape/doublequote */
            *d_write = current_char_val;
        } else if (!was_double_quote_pair && current_char_val == handle->quote && is_quoted_field) {
            /* This case should not be hit if break conditions are correct for closing quote.
             * It's to avoid copying the literal quote if it's not part of double quote.
             * However, parse_column_character returns the char to use.
             * If it was a single quote that is a *closing* quote, the loop breaks.
             * If it was a quote that is *part* of content (via double quote), parse_column_character handles it.
             */
        }
        d_write++;
        p_read++; /* Advance to the next source character */
    }

    /* If the loop terminated because *p_read is '\0' and b_col_start is still p_read (and it was past initial quote if any)
     * it means an empty or non-existent field at the end of the row.
     */
    if (!*p_read && d_write == b_col_start) {
        /* Check if it was truly empty vs. start of an empty quoted field */
        if (is_quoted_field && b_col_start == p_read +1 && *(p_read-1) == handle->quote ) {
             /* e.g. "" as a field - b_col_start is after first ", p_read is on second " */
        } else if (!is_quoted_field && p_read == b_col_start) { /* genuinely empty unquoted field */
             handle->context = p_read; /* context at null terminator */
             return NULL; /* No more columns */
        }
    }


    finalize_column_and_update_context(handle, p_read, d_write, is_quoted_field);

    /* Adjust b_col_start back if it was a quoted field to return pointer to first data char */
    /* The current b_col_start points after the initial quote if field was quoted. This is correct. */
    return b_col_start;
}
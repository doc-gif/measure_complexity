/* (c) 2019 Jan Doczy
 * This code is licensed under MIT license (see LICENSE.txt for details) */

#include <stdbool.h>
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

// 指定されたポインタが指す文字を処理し、有効な改行が見つかった場合はそのポインタを返すヘルパー関数
static char* _CsvProcessChar(CsvHandle handle, char* p) {
    char c = *p;
    char quote = handle->quote;

    if (c == quote) {
        handle->quotes++;
    } else if (c == '\n' && !(handle->quotes & 1)) {
        return p; // 有効な改行が見つかった位置を返す
    }

    return NULL; // 有効な改行ではない
}

char* CsvSearchLf(char* p, size_t size, CsvHandle handle) {
    char* res = NULL;
    char* end = p + size;

#ifdef CSV_UNPACK_64_SEARCH
    uint64_t* pd = (uint64_t*)p;
    uint64_t* pde = pd + (size / sizeof(uint64_t));

    for (; pd < pde; pd++) {
        // 64ビットを8バイトに展開し、各バイトを処理
        char* byte_p = (char*)pd;
        for (int i = 0; i < 8; i++) {
            res = _CsvProcessChar(handle, byte_p + i);
            if (res != NULL) {
                return res; // 有効な改行が見つかったら即座に返す
            }
        }
    }
    // 64ビット単位で処理しきれなかった残りのバイトから、p の位置を更新して処理を再開
    p = (char*)pde;
#endif

    // 残りのバイト、または CSV_UNPACK_64_SEARCH が無効な場合のバイト走査
    for (; p < end; p++) {
        res = _CsvProcessChar(handle, p);
        if (res != NULL) {
            return res; // 有効な改行が見つかったら即座に返す
        }
    }

    return NULL; // 指定範囲内に有効な改行が見つからなかった
}

// ファイルマッピング/確保を行い、エラーコードを返すヘルパー関数
static int _CsvHandleMapping(CsvHandle handle) {
    return CsvEnsureMapped(handle);
}

// 指定されたチャンクを auxbuf に追加し、必要なら再確保するヘルパー関数
// 成功した場合は true を、失敗した場合は false を返す。
static bool _CsvAppendChunkToAuxBuf(CsvHandle handle, const char* chunk, size_t chunkSize) {
    size_t newSize = handle->auxbufPos + chunkSize + 1;
    if (handle->auxbufSize < newSize) {
        void* mem = realloc(handle->auxbuf, newSize);
        if (!mem) {
            return false; // 再確保失敗
        }
        handle->auxbuf = mem;
        handle->auxbufSize = newSize;
    }

    memcpy((char*)handle->auxbuf + handle->auxbufPos, chunk, chunkSize);
    handle->auxbufPos += chunkSize;

    *(char*)((char*)handle->auxbuf + handle->auxbufPos) = '\0'; // ヌル終端
    return true; // 成功
}

// 改行が見つかった場合の処理を行うヘルパー関数
// 見つかった行のポインタを返す。補助バッファの再確保等に失敗した場合は NULL を返す。
static char* _CsvProcessFoundLine(CsvHandle handle, char* chunkStart, size_t chunkSize, char* foundLf) {
    // チャンク内での行の長さ (\n を含む)
    size_t lineLengthInChunk = (size_t)(foundLf - chunkStart) + 1;

    // 次回の読み取り開始位置を設定
    handle->pos += lineLengthInChunk;
    handle->quotes = 0; // 必要に応じて quotes をリセット

    char* resultLine;
    size_t resultLength;

    if (handle->auxbufPos > 0) {
        // auxbuf にデータが残っている場合、auxbuf と現在のチャンクから切り出した行部分を結合
        if (!_CsvAppendChunkToAuxBuf(handle, chunkStart, lineLengthInChunk)) {
            return NULL; // auxbuf への追加失敗
        }
        resultLine = handle->auxbuf;
        resultLength = handle->auxbufPos;
        handle->auxbufPos = 0; // auxbuf の位置をリセット
    } else {
        // auxbuf にデータがない場合、現在のチャンク内で完結した行を返す
        resultLine = chunkStart;
        resultLength = lineLengthInChunk;
    }

    // 行の末尾をヌル終端する (\r\n の場合は \r も考慮)
    char* res = resultLine + resultLength - 1;
    if (resultLength >= 2 && *(res - 1) == '\r') {
        --res;
    }
    *res = '\0';

    return resultLine; // 処理済みの行へのポインタを返す
}

// 改行が見つからなかった場合の処理を行うヘルパー関数
// 成功した場合は true を、失敗した場合は false を返す。
static bool _CsvProcessRemainingChunk(CsvHandle handle, char* chunkStart, size_t chunkSize) {
    // チャンク全体を auxbuf にコピー
    if (!_CsvAppendChunkToAuxBuf(handle, chunkStart, chunkSize)) {
        return false; // auxbuf への追加失敗
    }

    // 現在のチャンクは auxbuf に移動したので、ファイル位置を更新して次のチャンクに備える
    handle->pos = handle->size;

    return true; // 成功
}


char* CsvReadNextRow(CsvHandle handle) {
    char* p = NULL;
    char* found = NULL;
    size_t size;

    do {
        // 1. ファイルマッピング/確保とエラーチェック
        int err = _CsvHandleMapping(handle);
        handle->context = NULL; // 元のロジックの位置を維持

        if (err == -EINVAL) {
            // 最初のイテレーション (-EINVAL かつ p == NULL) の場合、元のロジックに従い auxbuf を返す
            if (p == NULL) {
                 return handle->auxbuf;
            }
            // 2回目以降のイテレーションで -EINVAL の場合 (通常は EOF 近く) は break
            break;
        } else if (err == -ENOMEM) {
            break; // メモリエラー
        }

        size = handle->size - handle->pos;
        if (!size) {
            break; // ファイルの終端
        }

        // 現在のチャンクの先頭ポインタ
        p = (char*)handle->mem + handle->pos;

        // 2. チャンク内での改行検索
        found = CsvSearchLf(p, size, handle);

        if (found) {
            // 3. 改行が見つかった場合の処理
            char* result = _CsvProcessFoundLine(handle, p, size, found);
            // _CsvProcessFoundLine が NULL を返した場合 (auxbuf の realloc 失敗)
            if (!result) {
                 return NULL;
            }
            return result; // 見つかった行を返す
        } else {
            // 4. 改行が見つからなかった場合、チャンク全体を auxbuf にコピー
            if (!_CsvProcessRemainingChunk(handle, p, size)) {
                 return NULL; // auxbuf への追加失敗
            }
            // ループ継続 (found がまだ NULL なので)
        }

    } while (true); // break または return でループを抜ける

    // ループを抜けた場合 (エラー、EOF など)
    // 元のコードの最後の return NULL に従う。
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

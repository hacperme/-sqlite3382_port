/*
** 2004 May 22
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
******************************************************************************
**
** This file contains code that is specific to Windows.
*/
#include "config.h"
#if SQLITE_OS_CUS_WINDOWS               /* This file is used for Windows only */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#ifdef WIN32
#include <windows.h>
#include <memory.h>
#include <process.h>
#endif

#include "sqlite3.h"




typedef sqlite3_int64 i64;
typedef sqlite3_uint64 u64;
typedef unsigned char u8;

# define OSTRACE(X)       printf (X)
#define SQLITE_WIN32_MAX_PATH_BYTES (256)




#define NO_LOCK         0
#define SHARED_LOCK     1
#define RESERVED_LOCK   2
#define PENDING_LOCK    3
#define EXCLUSIVE_LOCK  4





/*
** The winVfsAppData structure is used for the pAppData member for all of the
** Win32 VFS variants.
*/
typedef struct winVfsAppData winVfsAppData;

typedef struct
{
    sqlite3_io_methods const *pMethod;
    sqlite3_vfs *pvfs;
    int fd;
    int eFileLock;
    int szChunk;
    void * sem;
} winFile_t;

typedef winFile_t  winFile;

struct winVfsAppData {
  const sqlite3_io_methods *pMethod; /* The file I/O methods to use. */
  void *pAppData;                    /* The extra pAppData, if any. */
  BOOL bNoLock;                      /* Non-zero if locking is disabled. */
};


#if SQLITE_MUTEX_CUS_WINDOWS

/*
* rt-thread mutex
*/
struct sqlite3_mutex {
    void *mutex;          /* Mutex controlling the lock */
    int id;                         /* Mutex type */
#ifdef SQLITE_DEBUG
    volatile int nRef;         /* Number of enterances */
    volatile DWORD owner;      /* Thread holding this mutex */
#endif
};


void sqlite3MemoryBarrier(void)
{

}

/*
** Initialize and deinitialize the mutex subsystem.
The argument to sqlite3_mutex_alloc() must one of these integer constants:
    SQLITE_MUTEX_FAST
    SQLITE_MUTEX_RECURSIVE
    SQLITE_MUTEX_STATIC_MASTER
    SQLITE_MUTEX_STATIC_MEM
    SQLITE_MUTEX_STATIC_OPEN
    SQLITE_MUTEX_STATIC_PRNG
    SQLITE_MUTEX_STATIC_LRU
    SQLITE_MUTEX_STATIC_PMEM
    SQLITE_MUTEX_STATIC_APP1
    SQLITE_MUTEX_STATIC_APP2
    SQLITE_MUTEX_STATIC_APP3
    SQLITE_MUTEX_STATIC_VFS1
    SQLITE_MUTEX_STATIC_VFS2
    SQLITE_MUTEX_STATIC_VFS3
The first two constants (SQLITE_MUTEX_FAST and SQLITE_MUTEX_RECURSIVE)
cause sqlite3_mutex_alloc() to create a new mutex. The new mutex is recursive
when SQLITE_MUTEX_RECURSIVE is used but not necessarily so when SQLITE_MUTEX_FAST
is used. The mutex implementation does not need to make a distinction between
SQLITE_MUTEX_RECURSIVE and SQLITE_MUTEX_FAST if it does not want to.
SQLite will only request a recursive mutex in cases where it really needs one.
If a faster non-recursive mutex implementation is available on the host platform,
the mutex subsystem might return such a mutex in response to SQLITE_MUTEX_FAST.

The other allowed parameters to sqlite3_mutex_alloc()
(anything other than SQLITE_MUTEX_FAST and SQLITE_MUTEX_RECURSIVE) each return
a pointer to a static preexisting mutex. Nine static mutexes are used by the
current version of SQLite. Future versions of SQLite may add additional static
mutexes. Static mutexes are for internal use by SQLite only. Applications that
use SQLite mutexes should use only the dynamic mutexes returned by SQLITE_MUTEX_FAST
or SQLITE_MUTEX_RECURSIVE.

Note that if one of the dynamic mutex parameters (SQLITE_MUTEX_FAST or SQLITE_MUTEX_RECURSIVE)
is used then sqlite3_mutex_alloc() returns a different mutex on every call.
For the static mutex types, the same mutex is returned on every call that has the same type number.

*/
static sqlite3_mutex _static_mutex[12];

static int _win_mtx_init(void)
{
    int i;

    for (i = 0; i < sizeof(_static_mutex) / sizeof(_static_mutex[0]); i++)
    {
        _static_mutex[i].mutex = CreateMutex(NULL, FALSE, NULL);

        if (NULL ==  _static_mutex[i].mutex)
        {
            return SQLITE_ERROR;
        }
    }

    return SQLITE_OK;
}

static int _win_mtx_end(void)
{
    int i;
    int err;

    for (i = 0; i < sizeof(_static_mutex) / sizeof(_static_mutex[0]); i++)
    {
        err = CloseHandle(_static_mutex[i].mutex);
#ifdef SQLITE_DEBUG
        _static_mutex[i].mutex.owner = 0;
        _static_mutex[i].mutex.nRef = 0;
#endif

        if (err != 0)
        {
            return SQLITE_ERROR;
        }
    }

    return SQLITE_OK;
}

static sqlite3_mutex * _win_mtx_alloc(int id)
{
    sqlite3_mutex *p = NULL;

    switch (id)
    {
    case SQLITE_MUTEX_FAST:
    case SQLITE_MUTEX_RECURSIVE:
        p = sqlite3_malloc(sizeof(sqlite3_mutex));

        if (p != NULL)
        {
            p->mutex = CreateMutex(NULL, FALSE, NULL);
            p->id = id;
        }
        break;

    default:
        assert(id - 2 >= 0);
        assert(id - 2 < ArraySize(_static_mutex) );
        p = &_static_mutex[id - 2];
        p->id = id;
        break;
    }

    return p;
}

static void _win_mtx_free(sqlite3_mutex * p)
{
    assert(p != 0);

    CloseHandle(p->mutex);

    switch (p->id)
    {
    case SQLITE_MUTEX_FAST:
    case SQLITE_MUTEX_RECURSIVE:
        sqlite3_free(p);
        break;

    default:
        break;
    }
}

static void _win_mtx_enter(sqlite3_mutex *p)
{
    assert(p != 0);
#if defined(SQLITE_DEBUG) || defined(SQLITE_TEST)
  DWORD tid = GetCurrentThreadId();
#endif

    WaitForSingleObject(p->mutex, INFINITE);
#ifdef SQLITE_DEBUG
  assert( p->nRef>0 || p->owner==0 );
  p->owner = tid;
  p->nRef++;
 #endif
}

static int _win_mtx_try(sqlite3_mutex *p)
{
    assert(p != 0);

    if (ReleaseMutex(p->mutex) != 0)
    {
        return SQLITE_BUSY;
    }

    return SQLITE_OK;
}

static void _win_mtx_leave(sqlite3_mutex *p)
{
    assert(p != 0);

    ReleaseMutex(p->mutex);
}

#ifdef SQLITE_DEBUG

/*
    If the argument to sqlite3_mutex_held() is a NULL pointer then the routine
    should return 1. This seems counter-intuitive since clearly the mutex cannot
    be held if it does not exist. But the reason the mutex does not exist is
    because the build is not using mutexes. And we do not want the assert()
    containing the call to sqlite3_mutex_held() to fail, so a non-zero return
    is the appropriate thing to do. The sqlite3_mutex_notheld() interface should
    also return 1 when given a NULL pointer.
*/
static int _win_mtx_held(sqlite3_mutex *p)
{
    if (p != 0)
    {
        if ((GetCurrentThreadId() == p->mutex.owner) && (p->mutex.nRef > 0))
        {
            return 1;
        }

        return 0;
    }

    return 1;
}

static int _win_mtx_noheld(sqlite3_mutex *p)
{
    if (_win_mtx_held(p))
    {
        return 0;
    }

    return 1;
}

#endif  /* SQLITE_DEBUG */

sqlite3_mutex_methods const *sqlite3DefaultMutex(void)
{
    static const sqlite3_mutex_methods sMutex = {
        _win_mtx_init,
        _win_mtx_end,
        _win_mtx_alloc,
        _win_mtx_free,
        _win_mtx_enter,
        _win_mtx_try,
        _win_mtx_leave,
    #ifdef SQLITE_DEBUG
        _win_mtx_held,
        _win_mtx_noheld
    #else
        0,
        0
    #endif
    };

    return &sMutex;
}

#endif


static int win_get_temp_name(int nBuf, char *zBuf);



/*
** Read data from a file into a buffer.  Return SQLITE_OK if all
** bytes were read successfully and SQLITE_IOERR if anything goes
** wrong.
*/
static int winRead(
  sqlite3_file *file_id,          /* File to read from */
  void *pBuf,                /* Write content into this buffer */
  int amt,                   /* Number of bytes to read */
  sqlite3_int64 offset       /* Begin reading at this offset */
){
    winFile_t *file = (winFile_t*)file_id;
    sqlite3_int64 new_offset;
    int r_cnt;

    assert(file_id);
    assert(offset >= 0);
    assert(cnt > 0);

    new_offset = lseek(file->fd, offset, SEEK_SET);

    if (new_offset != offset)
    {
        return SQLITE_IOERR_READ;
    }

    do {
        r_cnt = read(file->fd, pBuf, amt);

        if (r_cnt == amt)
        {
            break;
        }

        if (r_cnt < 0)
        {
            if (errno != EINTR)
            {
                return SQLITE_IOERR_READ;
            }

            r_cnt = 1;
            continue;
        }
        else if (r_cnt > 0)
        {
            amt -= r_cnt;
            pBuf = (void*)(r_cnt + (char*)pBuf);
        }
    } while (r_cnt > 0);

    if (r_cnt != amt)
    {
        memset(&((char*)pBuf)[r_cnt], 0, amt - r_cnt);
        return SQLITE_IOERR_SHORT_READ;
    }

    return SQLITE_OK;

}

/*
** Write data from a buffer into a file.  Return SQLITE_OK on success
** or some other error code on failure.
*/
static int winWrite(
        sqlite3_file *file_id,               /* File to write into */
        const void *pBuf,               /* The bytes to be written */
        int amt,                        /* Number of bytes to write */
        sqlite3_int64 offset            /* Offset into the file to begin writing at */
        ){
    winFile_t *file = (winFile_t*)file_id;
    sqlite3_int64 new_offset;
    int w_cnt;

    assert(file_id);
    assert(cnt > 0);

    new_offset = lseek(file->fd, offset, SEEK_SET);

    if (new_offset != offset)
    {
        return SQLITE_IOERR_WRITE;
    }

    do {
        w_cnt = write(file->fd, pBuf, amt);

        if (w_cnt == amt)
        {
            break;
        }

        if (w_cnt < 0)
        {
            if (errno != EINTR)
            {
                return SQLITE_IOERR_WRITE;
            }

            w_cnt = 1;
            continue;
        }
        else if (w_cnt > 0)
        {
            amt -= w_cnt;
            pBuf = (void*)(w_cnt + (char*)pBuf);
        }
    } while (w_cnt > 0);

    if (w_cnt != amt)
    {
        return SQLITE_FULL;
    }

    return SQLITE_OK;

    return SQLITE_OK;
}

/*
** Truncate an open file to a specified size
*/
static int winTruncate(sqlite3_file *id, sqlite3_int64 nByte){
    return SQLITE_IOERR_TRUNCATE;
}

/*
** Determine the current size of a file in bytes
*/
static int winFileSize(sqlite3_file *file_id, sqlite3_int64 *pSize){
    int rc;
    struct stat buf;
    winFile_t *file = (winFile_t*)file_id;

    assert(file_id);

    rc = fstat(file->fd, &buf);

    if (rc != 0)
    {
        return SQLITE_IOERR_FSTAT;
    }

    *pSize = buf.st_size;

    /* When opening a zero-size database, the findInodeInfo() procedure
        ** writes a single byte into that file in order to work around a bug
        ** in the OS-X msdos filesystem.  In order to avoid problems with upper
        ** layers, we need to report this file size as zero even though it is
        ** really 1.   Ticket #3260.
        */
    if (*pSize == 1) *pSize = 0;

    return SQLITE_OK;
}


static int winSync(sqlite3_file *id, int flags){

    winFile_t *file = (winFile_t*)id;

    assert((flags & 0x0F) == SQLITE_SYNC_NORMAL
           || (flags & 0x0F) == SQLITE_SYNC_FULL);

//    fsync(file->fd);

    return SQLITE_OK;
}

/*
** LOCKFILE_FAIL_IMMEDIATELY is undefined on some Windows systems.
*/
#ifndef LOCKFILE_FAIL_IMMEDIATELY
# define LOCKFILE_FAIL_IMMEDIATELY 1
#endif

#ifndef LOCKFILE_EXCLUSIVE_LOCK
# define LOCKFILE_EXCLUSIVE_LOCK 2
#endif

/*
** Historically, SQLite has used both the LockFile and LockFileEx functions.
** When the LockFile function was used, it was always expected to fail
** immediately if the lock could not be obtained.  Also, it always expected to
** obtain an exclusive lock.  These flags are used with the LockFileEx function
** and reflect those expectations; therefore, they should not be changed.
*/
#ifndef SQLITE_LOCKFILE_FLAGS
# define SQLITE_LOCKFILE_FLAGS   (LOCKFILE_FAIL_IMMEDIATELY | \
                                  LOCKFILE_EXCLUSIVE_LOCK)
#endif

/*
** Currently, SQLite never calls the LockFileEx function without wanting the
** call to fail immediately if the lock cannot be obtained.
*/
#ifndef SQLITE_LOCKFILEEX_FLAGS
# define SQLITE_LOCKFILEEX_FLAGS (LOCKFILE_FAIL_IMMEDIATELY)
#endif

/*
** Acquire a reader lock.
** Different API routines are called depending on whether or not this
** is Win9x or WinNT.
*/
static int winGetReadLock(winFile *pFile){
  return SQLITE_OK;
}

/*
** Undo a readlock
*/
static int winUnlockReadLock(winFile *pFile){
  return SQLITE_OK;
}

/*
** Lock the file with the lock specified by parameter locktype - one
** of the following:
**
**     (1) SHARED_LOCK
**     (2) RESERVED_LOCK
**     (3) PENDING_LOCK
**     (4) EXCLUSIVE_LOCK
**
** Sometimes when requesting one lock state, additional lock states
** are inserted in between.  The locking might fail on one of the later
** transitions leaving the lock state different from what it started but
** still short of its goal.  The following chart shows the allowed
** transitions and the inserted intermediate states:
**
**    UNLOCKED -> SHARED
**    SHARED -> RESERVED
**    SHARED -> (PENDING) -> EXCLUSIVE
**    RESERVED -> (PENDING) -> EXCLUSIVE
**    PENDING -> EXCLUSIVE
**
** This routine will only increase a lock.  The winUnlock() routine
** erases all locks at once and returns us immediately to locking level 0.
** It is not possible to lower the locking level one step at a time.  You
** must go straight to locking level 0.
*/
static int winLock(sqlite3_file *id, int locktype){
    winFile_t *file = (winFile_t*)id;
    void *psem = file->sem;
    int rc = SQLITE_OK;

    /* if we already have a lock, it is exclusive.
    ** Just adjust level and punt on outta here. */
    if (file->eFileLock > NO_LOCK)
    {
        file->eFileLock = locktype;
        rc = SQLITE_OK;
        goto sem_end_lock;
    }

    /* lock semaphore now but bail out when already locked. */
    if (WaitForSingleObject(psem, 0) != 0)
    {
        rc = SQLITE_BUSY;
        goto sem_end_lock;
    }

    /* got it, set the type and return ok */
    file->eFileLock = locktype;

sem_end_lock:
    return rc;
}

/*
** This routine checks if there is a RESERVED lock held on the specified
** file by this or any other process. If such a lock is held, return
** non-zero, otherwise zero.
*/
static int winCheckReservedLock(sqlite3_file *id, int *pResOut){

    winFile_t *file = (winFile_t*)id;
    void *psem = file->sem;
    int reserved = 0;

    /* Check if a thread in this process holds such a lock */
    if (file->eFileLock > SHARED_LOCK)
    {
        reserved = 1;
    }

    /* Otherwise see if some other process holds it. */
    if (!reserved)
    {
        if (WaitForSingleObject(psem, 0) != 0)
        {
            /* someone else has the lock when we are in NO_LOCK */
            reserved = (file->eFileLock < SHARED_LOCK);
        }
        else
        {
            /* we could have it if we want it */
            ReleaseSemaphore(psem, 1, NULL);
        }
    }

    *pResOut = reserved;
    return SQLITE_OK;
}

/*
** Lower the locking level on file descriptor id to locktype.  locktype
** must be either NO_LOCK or SHARED_LOCK.
**
** If the locking level of the file descriptor is already at or below
** the requested locking level, this routine is a no-op.
**
** It is not possible for this routine to fail if the second argument
** is NO_LOCK.  If the second argument is SHARED_LOCK then this routine
** might return SQLITE_IOERR;
*/
static int winUnlock(sqlite3_file *id, int locktype){
    winFile_t *file = (winFile_t*)id;
        void *psem = file->sem;

        assert(locktype <= SHARED_LOCK);

        /* no-op if possible */
        if (file->eFileLock == locktype)
        {
            return SQLITE_OK;
        }

        /* shared can just be set because we always have an exclusive */
        if (locktype == SHARED_LOCK)
        {
            file->eFileLock = SHARED_LOCK;
            return SQLITE_OK;
        }

        /* no, really unlock. */
        ReleaseSemaphore(psem, 1, NULL);

        file->eFileLock = NO_LOCK;
        return SQLITE_OK;
}




static int win_fcntl_size_hint(sqlite3_file *file_id, i64 nByte)
{
    winFile_t *file = (winFile_t*)file_id;

    if (file->szChunk > 0)
    {
        i64 nSize;                    /* Required file size */
        struct stat buf;              /* Used to hold return values of fstat() */

        if (fstat(file->fd, &buf))
        {
            return SQLITE_IOERR_FSTAT;
        }

        nSize = ((nByte + file->szChunk - 1) / file->szChunk) * file->szChunk;

        if (nSize > (i64)buf.st_size)
        {
            /* If the OS does not have posix_fallocate(), fake it. Write a
            ** single byte to the last byte in each block that falls entirely
            ** within the extended region. Then, if required, a single byte
            ** at offset (nSize-1), to set the size of the file correctly.
            ** This is a similar technique to that used by glibc on systems
            ** that do not have a real fallocate() call.
            */
            int nBlk = 512;  /* File-system block size */
            int nWrite = 0;             /* Number of bytes written by seekAndWrite */
            i64 iWrite;                 /* Next offset to write to */

            iWrite = (buf.st_size / nBlk) * nBlk + nBlk - 1;
            assert(iWrite >= buf.st_size);
            assert(((iWrite + 1) % nBlk) == 0);

            for (/*no-op*/; iWrite < nSize + nBlk - 1; iWrite += nBlk)
            {
                if (iWrite >= nSize)
                {
                    iWrite = nSize - 1;
                }

                nWrite = winWrite(file_id, "", 1, iWrite);

                if (nWrite != 1)
                {
                    return SQLITE_IOERR_WRITE;
                }
            }
        }
    }

    return SQLITE_OK;
}

/*
** Control and query of the open file handle.
*/
static int winFileControl(sqlite3_file *id, int op, void *pArg){
    winFile_t *file = (winFile_t*)id;

    switch( op )
    {
    case SQLITE_FCNTL_LOCKSTATE: {
        *(int*)pArg = file->eFileLock;
        return SQLITE_OK;
    }

    case SQLITE_LAST_ERRNO: {
        *(int*)pArg = 0;
        return SQLITE_OK;
    }

    case SQLITE_FCNTL_CHUNK_SIZE: {
        file->szChunk = *(int *)pArg;
        return SQLITE_OK;
    }

    case SQLITE_FCNTL_SIZE_HINT: {
        int rc;
        rc = win_fcntl_size_hint(id, *(i64 *)pArg);
        return rc;
    }

    case SQLITE_FCNTL_PERSIST_WAL: {
        return SQLITE_OK;
    }

    case SQLITE_FCNTL_POWERSAFE_OVERWRITE: {
        return SQLITE_OK;
    }

    case SQLITE_FCNTL_VFSNAME: {
        *(char**)pArg = sqlite3_mprintf("%s", file->pvfs->zName);
        return SQLITE_OK;
    }

    case SQLITE_FCNTL_TEMPFILENAME: {
        char *zTFile = sqlite3_malloc(file->pvfs->mxPathname );

        if( zTFile )
        {
            win_get_temp_name(file->pvfs->mxPathname, zTFile);
            *(char**)pArg = zTFile;
        }
        return SQLITE_OK;
    }
    }

    return SQLITE_NOTFOUND;
}

/*
** Return the sector size in bytes of the underlying block device for
** the specified file. This is almost always 512 bytes, but may be
** larger for some devices.
**
** SQLite code assumes this function cannot fail. It also assumes that
** if two files are created in the same file-system directory (i.e.
** a database and its journal file) that the sector size will be the
** same for both.
*/
static int winSectorSize(sqlite3_file *id){
  (void)id;
  return SQLITE_DEFAULT_SECTOR_SIZE;
}

/*
** Return a vector of device characteristics.
*/
static int winDeviceCharacteristics(sqlite3_file *id){
  return SQLITE_OK;
}

/*
** If possible, return a pointer to a mapping of file fd starting at offset
** iOff. The mapping must be valid for at least nAmt bytes.
**
** If such a pointer can be obtained, store it in *pp and return SQLITE_OK.
** Or, if one cannot but no error occurs, set *pp to 0 and return SQLITE_OK.
** Finally, if an error does occur, return an SQLite error code. The final
** value of *pp is undefined in this case.
**
** If this function does return a pointer, the caller must eventually
** release the reference by calling winUnfetch().
*/
static int winFetch(sqlite3_file *fd, i64 iOff, int nAmt, void **pp){

  return SQLITE_OK;
}


/*
** If the third argument is non-NULL, then this function releases a
** reference obtained by an earlier call to winFetch(). The second
** argument passed to this function must be the same as the corresponding
** argument that was passed to the winFetch() invocation.
**
** Or, if the third argument is NULL, then this function is being called
** to inform the VFS layer that, according to POSIX, any existing mapping
** may now be invalid and should be unmapped.
*/
static int winUnfetch(sqlite3_file *fd, i64 iOff, void *p){

  return SQLITE_OK;
}


/*
** Close a file.
**
** It is reported that an attempt to close a handle might sometimes
** fail.  This is a very unreasonable result, but Windows is notorious
** for being unreasonable so I do not doubt that it might happen.  If
** the close fails, we pause for 100 milliseconds and try again.  As
** many as MX_CLOSE_ATTEMPT attempts to close the handle are made before
** giving up and returning an error.
*/

static int winClose(sqlite3_file *id){
    int rc = 0;
    winFile_t *file = (winFile_t*)id;

    if (file->fd >= 0)
    {
        winUnlock(id, NO_LOCK);
        CloseHandle(file->sem);
        rc = close(file->fd);
        file->fd = -1;
    }

    return rc;
}


/*
** This vector defines all the methods that can operate on an
** sqlite3_file for win32.
*/
static const sqlite3_io_methods winIoMethod = {
  3,                              /* iVersion */
  winClose,                       /* xClose */
  winRead,                        /* xRead */
  winWrite,                       /* xWrite */
  winTruncate,                    /* xTruncate */
  winSync,                        /* xSync */
  winFileSize,                    /* xFileSize */
  winLock,                        /* xLock */
  winUnlock,                      /* xUnlock */
  winCheckReservedLock,           /* xCheckReservedLock */
  winFileControl,                 /* xFileControl */
  winSectorSize,                  /* xSectorSize */
  winDeviceCharacteristics,       /* xDeviceCharacteristics */
  0,                      /* xShmMap */
  0,                     /* xShmLock */
  0,                  /* xShmBarrier */
  0,                    /* xShmUnmap */
  winFetch,                       /* xFetch */
  winUnfetch                      /* xUnfetch */
};


static winVfsAppData winAppData = {
  &winIoMethod,       /* pMethod */
  0,                  /* pAppData */
  0                   /* bNoLock */
};


static const char* win_temp_file_dir(void)
{
    const char *azDirs[] = {
        0,
        "/sql",
        "/sql/tmp"
        "/tmp",
        0        /* List terminator */
    };
    unsigned int i;
    struct stat buf;
    const char *zDir = 0;

    azDirs[0] = sqlite3_temp_directory;

    for (i = 0; i < sizeof(azDirs) / sizeof(azDirs[0]); zDir = azDirs[i++])
    {
        if( zDir == 0 ) continue;
        if( stat(zDir, &buf) ) continue;
        if( !S_ISDIR(buf.st_mode) ) continue;
        break;
    }

    return zDir;
}


/*
** Create a temporary file name in zBuf.  zBuf must be allocated
** by the calling process and must be big enough to hold at least
** pVfs->mxPathname bytes.
*/
static int win_get_temp_name(int nBuf, char *zBuf)
{
    const unsigned char zChars[] = "abcdefghijklmnopqrstuvwxyz"
                                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                    "0123456789";
    unsigned int i, j;
    const char *zDir;

    zDir = win_temp_file_dir();

    if (zDir == 0)
    {
        zDir = ".";
    }

    /* Check that the output buffer is large enough for the temporary file
    ** name. If it is not, return SQLITE_ERROR.
    */
    if ((strlen(zDir) + strlen(SQLITE_TEMP_FILE_PREFIX) + 18) >= (size_t)nBuf)
    {
        return SQLITE_ERROR;
    }

    do {
        sqlite3_snprintf(nBuf-18, zBuf, "%s/"SQLITE_TEMP_FILE_PREFIX, zDir);
        j = (int)strlen(zBuf);
        sqlite3_randomness(15, &zBuf[j]);

        for (i = 0; i < 15; i++, j++)
        {
            zBuf[j] = (char)zChars[((unsigned char)zBuf[j]) % (sizeof(zChars) - 1)];
        }

        zBuf[j] = 0;
        zBuf[j + 1] = 0;
    } while (access(zBuf, F_OK) == 0);

    return SQLITE_OK;
}




/*
** Open a file.
*/
static int winOpen(
        sqlite3_vfs *pVfs,        /* Used to get maximum path length and AppData */
        const char *zName,        /* Name of the file (UTF-8) */
        sqlite3_file *id,         /* Write the SQLite file handle here */
        int flags,                /* Open mode flags */
        int *pOutFlags            /* Status return flags */
        ){
    winFile_t *p;
    int fd;
    int eType = flags & 0xFFFFFF00;  /* Type of file to open */
    int rc = SQLITE_OK;            /* Function Return Code */
    int openFlags = 0;
    mode_t openMode = 0;

    int isExclusive  = (flags & SQLITE_OPEN_EXCLUSIVE);
    int isDelete     = (flags & SQLITE_OPEN_DELETEONCLOSE);
    int isCreate     = (flags & SQLITE_OPEN_CREATE);
    int isReadonly   = (flags & SQLITE_OPEN_READONLY);
    int isReadWrite  = (flags & SQLITE_OPEN_READWRITE);

    /* If argument zPath is a NULL pointer, this function is required to open
        ** a temporary file. Use this buffer to store the file name in.
        */
    char zTmpname[SQLITE_WIN32_MAX_PATH_BYTES + 2];

    p = (winFile_t *)id;

    /* Check the following statements are true:
        **
        **   (a) Exactly one of the READWRITE and READONLY flags must be set, and
        **   (b) if CREATE is set, then READWRITE must also be set, and
        **   (c) if EXCLUSIVE is set, then CREATE must also be set.
        **   (d) if DELETEONCLOSE is set, then CREATE must also be set.
        */
    assert((isReadonly==0 || isReadWrite==0) && (isReadWrite || isReadonly));
    assert(isCreate==0 || isReadWrite);
    assert(isExclusive==0 || isCreate);
    assert(isDelete==0 || isCreate);

    /* The main DB, main journal, WAL file and master journal are never
        ** automatically deleted. Nor are they ever temporary files.  */
    assert( (!isDelete && file_path) || eType!=SQLITE_OPEN_MAIN_DB );
    assert( (!isDelete && file_path) || eType!=SQLITE_OPEN_MAIN_JOURNAL );
    assert( (!isDelete && file_path) || eType!=SQLITE_OPEN_MASTER_JOURNAL );
    assert( (!isDelete && file_path) || eType!=SQLITE_OPEN_WAL );

    /* Assert that the upper layer has set one of the "file-type" flags. */
    assert( eType==SQLITE_OPEN_MAIN_DB      || eType==SQLITE_OPEN_TEMP_DB
            || eType==SQLITE_OPEN_MAIN_JOURNAL || eType==SQLITE_OPEN_TEMP_JOURNAL
            || eType==SQLITE_OPEN_SUBJOURNAL   || eType==SQLITE_OPEN_MASTER_JOURNAL
            || eType==SQLITE_OPEN_TRANSIENT_DB || eType==SQLITE_OPEN_WAL
            );

    /* Database filenames are double-zero terminated if they are not
        ** URIs with parameters.  Hence, they can always be passed into
        ** sqlite3_uri_parameter(). */
    assert((eType != SQLITE_OPEN_MAIN_DB) || (flags & SQLITE_OPEN_URI) || file_path[strlen(file_path) + 1] == 0);

    memset(p, 0, sizeof(winFile_t));
    if (!zName)
    {
        rc = win_get_temp_name(SQLITE_WIN32_MAX_PATH_BYTES + 2, zTmpname);
        if (rc != SQLITE_OK )
        {
            return rc;
        }
        zName = zTmpname;

        /* Generated temporary filenames are always double-zero terminated
            ** for use by sqlite3_uri_parameter(). */
        assert(zName[strlen(zName) + 1] == 0);
    }

    /* Determine the value of the flags parameter passed to POSIX function
        ** open(). These must be calculated even if open() is not called, as
        ** they may be stored as part of the file handle and used by the
        ** 'conch file' locking functions later on.  */
    if (isReadonly)  openFlags |= O_RDONLY;
    if (isReadWrite) openFlags |= O_RDWR;
    if (isCreate)    openFlags |= O_CREAT;
    if (isExclusive) openFlags |= (O_EXCL /*| O_NOFOLLOW*/);
    openFlags |= (/*O_LARGEFILE |*/ O_BINARY);

    // todo 后面改成fopen 兼容其他平台
    fd = open(zName, openFlags, openMode);

    if (fd < 0 && (errno != -EISDIR) && isReadWrite && !isExclusive)
    {
        /* Failed to open the file for read/write access. Try read-only. */
        flags &= ~(SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
        openFlags &= ~(O_RDWR | O_CREAT);
        flags |= SQLITE_OPEN_READONLY;
        openFlags |= O_RDONLY;
        isReadonly = 1;
        fd = open(zName, openFlags, openMode);
    }

    if (fd < 0)
    {
        rc = SQLITE_ERROR;
        return rc;
    }

    if (pOutFlags)
    {
        *pOutFlags = flags;
    }

    if (isDelete)
    {
        unlink(zName);
    }

    p->fd = fd;
    p->pMethod = winAppData.pMethod;
    p->eFileLock = NO_LOCK;
    p->szChunk = 0;
    p->pvfs = pVfs;
    p->sem = CreateSemaphore(NULL, 1, 1, NULL);

    return rc;
}

/*
** Delete the named file.
**
** Note that Windows does not allow a file to be deleted if some other
** process has it open.  Sometimes a virus scanner or indexing program
** will open a journal file shortly after it is created in order to do
** whatever it does.  While this other process is holding the
** file open, we will be unable to delete it.  To work around this
** problem, we delay 100 milliseconds and try to delete again.  Up
** to MX_DELETION_ATTEMPTs deletion attempts are run before giving
** up and returning an error.
*/
static int winDelete(
  sqlite3_vfs *pVfs,          /* Not used on win32 */
  const char *zFilename,      /* Name of file to delete */
  int syncDir                 /* Not used on win32 */
){
  int rc = SQLITE_OK;            /* Function Return Code */
  return rc;
}

#if 0
static int _Access(const char *pathname, int mode)
{
    int fd;

    fd = open(pathname, O_RDONLY, mode);

    if (fd >= 0)
    {
        close(fd);
        return 0;
    }

    return -1;
}
#endif

/*
** Check the existence and status of a file.
*/
static int winAccess(
  sqlite3_vfs *pVfs,         /* Not used on win32 */
  const char *zFilename,     /* Name of file to check */
  int flags,                 /* Type of test to make on this file */
  int *pResOut               /* OUT: Result */
){
    int amode = 0;

#ifndef F_OK
# define F_OK 0
#endif
#ifndef R_OK
# define R_OK 4
#endif
#ifndef W_OK
# define W_OK 2
#endif

    switch (flags)
    {
    case SQLITE_ACCESS_EXISTS:
        amode = F_OK;
        break;

    case SQLITE_ACCESS_READWRITE:
        amode = W_OK | R_OK;
        break;

    case SQLITE_ACCESS_READ:
        amode = R_OK;
        break;

    default:
        return -1;
    }

    *pResOut = (access(zFilename, amode) == 0);

    if (flags == SQLITE_ACCESS_EXISTS && *pResOut)
    {
        struct stat buf;

        if (0 == stat(zFilename, &buf) && (buf.st_size == 0))
        {
            *pResOut = 0;
        }
    }

  return SQLITE_OK;
}

/*
** Turn a relative pathname into a full pathname.  Write the full
** pathname into zOut[].  zOut[] will be at least pVfs->mxPathname
** bytes in size.
*/
static int winFullPathname(
  sqlite3_vfs *pVfs,            /* Pointer to vfs object */
  const char *zRelative,        /* Possibly relative input path */
  int nFull,                    /* Size of output buffer in bytes */
  char *zFull                   /* Output buffer */
){
    // 将相对路径转换为绝对路径
    assert(pvfs->mxPathname == SQLITE_WIN32_MAX_PATH_BYTES);

    zFull[nFull - 1] = '\0';

    if (zRelative[0] == '/')
    {
        sqlite3_snprintf(nFull, zFull, "%s", zRelative);
    }
    else
    {
        int nCwd;

        if (getcwd(zFull, nFull - 1) == 0)
        {
            return SQLITE_ERROR;
        }

        nCwd = (int)strlen(zFull);
        sqlite3_snprintf(nFull - nCwd, &zFull[nCwd], "/%s", zRelative);
    }

    return SQLITE_OK;

}




/*
** Write up to nBuf bytes of randomness into zBuf.
*/
static int winRandomness(sqlite3_vfs *pVfs, int nBuf, char *zBuf){

    assert((size_t)nByte >= (sizeof(time_t) + sizeof(int)));

    memset(zBuf, 0, nBuf);
    {
        int i;
        char tick8, tick16;

        tick8 = (char)GetTickCount();
        tick16 = (char)(GetTickCount() >> 8);

        for (i = 0; i < nBuf; i++)
        {
            zBuf[i] = (char)(i ^ tick8 ^ tick16);
            tick8 = zBuf[i];
            tick16 = ~(tick8 ^ tick16);
        }
    }

    return nBuf;
}



static int winSleep(sqlite3_vfs *pVfs, int microsec){

    int millisecond = (microsec + 999) / 1000;
    Sleep(millisecond);
    return ((microsec+999)/1000)*1000;
}

static int winCurrentTimeInt64(sqlite3_vfs *pVfs, sqlite3_int64 *piNow);

/*
** Find the current time (in Universal Coordinated Time).  Write the
** current time and date as a Julian Day number into *prNow and
** return 0.  Return 1 if the time and date cannot be found.
*/
static int winCurrentTime(sqlite3_vfs *pVfs, double *prNow){
  int rc;
  sqlite3_int64 i;
  rc = winCurrentTimeInt64(pVfs, &i);
  if( !rc ){
    *prNow = i/86400000.0;
  }
  return rc;
}

/*
** Find the current time (in Universal Coordinated Time).  Write into *piNow
** the current time and date as a Julian Day number times 86_400_000.  In
** other words, write into *piNow the number of milliseconds since the Julian
** epoch of noon in Greenwich on November 24, 4714 B.C according to the
** proleptic Gregorian calendar.
**
** On success, return SQLITE_OK.  Return SQLITE_ERROR if the time and date
** cannot be found.
*/
static int winCurrentTimeInt64(sqlite3_vfs *pVfs, sqlite3_int64 *piNow){
    static const sqlite3_int64 winFiletimeEpoch = 24405875*(sqlite3_int64)8640000;

    //  static const sqlite3_int64 max32BitValue =
    //        (sqlite3_int64)2000000000 + (sqlite3_int64)2000000000 +
    //        (sqlite3_int64)294967296;

    time_t t;
    time(&t);
    *piNow = ((sqlite3_int64)t) * 1000 + winFiletimeEpoch;

    return SQLITE_OK;
}


/*
** The idea is that this function works like a combination of
** GetLastError() and FormatMessage() on Windows (or errno and
** strerror_r() on Unix). After an error is returned by an OS
** function, SQLite calls this function with zBuf pointing to
** a buffer of nBuf bytes. The OS layer should populate the
** buffer with a nul-terminated UTF-8 encoded error message
** describing the last IO error to have occurred within the calling
** thread.
**
** If the error message is too large for the supplied buffer,
** it should be truncated. The return value of xGetLastError
** is zero if the error message fits in the buffer, or non-zero
** otherwise (if the message was truncated). If non-zero is returned,
** then it is not necessary to include the nul-terminator character
** in the output buffer.
**
** Not supplying an error message will have no adverse effect
** on SQLite. It is fine to have an implementation that never
** returns an error message:
**
**   int xGetLastError(sqlite3_vfs *pVfs, int nBuf, char *zBuf){
**     assert(zBuf[0]=='\0');
**     return 0;
**   }
**
** However if an error message is supplied, it will be incorporated
** by sqlite into the error message available to the user using
** sqlite3_errmsg(), possibly making IO errors easier to debug.
*/
static int winGetLastError(sqlite3_vfs *pVfs, int nBuf, char *zBuf){

  return 0;
}







/*
** This is the xSetSystemCall() method of sqlite3_vfs for all of the
** "win32" VFSes.  Return SQLITE_OK opon successfully updating the
** system call pointer, or SQLITE_NOTFOUND if there is no configurable
** system call named zName.
*/
static int winSetSystemCall(
  sqlite3_vfs *pNotUsed,        /* The VFS pointer.  Not used */
  const char *zName,            /* Name of system call to override */
  sqlite3_syscall_ptr pNewFunc  /* Pointer to new system call value */
){
  int rc = SQLITE_NOTFOUND;

  return rc;
}

/*
** Return the value of a system call.  Return NULL if zName is not a
** recognized system call name.  NULL is also returned if the system call
** is currently undefined.
*/
static sqlite3_syscall_ptr winGetSystemCall(
  sqlite3_vfs *pNotUsed,
  const char *zName
){

  return 0;
}


/*
** Return the name of the first system call after zName.  If zName==NULL
** then return the name of the first system call.  Return NULL if zName
** is the last system call or if zName is not the name of a valid
** system call.
*/
static const char *winNextSystemCall(sqlite3_vfs *p, const char *zName){

  return 0;
}

/*
** Initialize and deinitialize the operating system interface.
*/
int sqlite3_os_init(void){

    static sqlite3_vfs winVfs = {
        3,                     /* iVersion */
        sizeof(winFile),       /* szOsFile */
        SQLITE_WIN32_MAX_PATH_BYTES, /* mxPathname */
        0,                     /* pNext */
        "win32",               /* zName */
        &winAppData,           /* pAppData */
        winOpen,               /* xOpen */
        winDelete,             /* xDelete */
        winAccess,             /* xAccess */
        winFullPathname,       /* xFullPathname */
        0,             /* xDlOpen */
        0,            /* xDlError */
        0,              /* xDlSym */
        0,            /* xDlClose */
        winRandomness,         /* xRandomness */
        winSleep,              /* xSleep */
        winCurrentTime,        /* xCurrentTime */
        winGetLastError,       /* xGetLastError */
        winCurrentTimeInt64,   /* xCurrentTimeInt64 */
        winSetSystemCall,      /* xSetSystemCall */
        winGetSystemCall,      /* xGetSystemCall */
        winNextSystemCall,     /* xNextSystemCall */
    };


    sqlite3_vfs_register(&winVfs, 1);



    return SQLITE_OK;
}

int sqlite3_os_end(void){


    return SQLITE_OK;
}

#endif /* SQLITE_OS_WIN */

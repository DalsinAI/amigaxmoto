/*
 * SQLite's file system for AmigaOS 3.x (SQLITE_OS_OTHER=1), on dos.library
 * itself: Open, Read, Write, Seek, SetFileSize, DeleteFile, Lock.
 *
 * Seek() returns the position it leaves (the OLD one), -1 on failure, and
 * will not go past the end of a file; SetFileSize() returns the NEW size,
 * and a file it makes longer holds whatever was on the disk. So:
 * - the size is Seek(OFFSET_END) and then Seek(OFFSET_CURRENT)'s answer;
 * - a read past the end is a short read, zero-filled, without seeking;
 * - a write past the end first writes zeros up to it;
 * - a truncate that would lengthen the file writes zeros instead.
 * (The first version went through libnix's ftruncate(), which returns
 * SetFileSize()'s new size, so every successful truncate looked like an
 * error; and it took no locks, so X-Moto's connections from its SDL
 * threads wrote over each other's pages.)
 *
 * Locks: SQLite's five levels (NONE, SHARED, RESERVED, PENDING, EXCLUSIVE)
 * kept in a table in memory, one entry per database file, shared by every
 * connection of the program, as the Unix VFS keeps them per inode, under
 * SQLite's own SQLITE_MUTEX_STATIC_VFS1. Another program opening the same
 * database at the same time is not seen (AmigaDOS has no byte-range
 * locks); X-Moto runs once.
 *
 * Names are used as given (PROGDIR:, volumes and assigns are absolute);
 * the lock table knows a file by its full name from NameFromLock() when it
 * exists, else by its name, case-insensitively. Temporary files go in T:.
 * The AmiCradle Team; public domain, like SQLite.
 */
#include "sqlite3.h"
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

/* dos.library through libamiga's stubs (libnix opens DOSBase), under our
 * own names so that nothing in the amalgamation can meet them. */
typedef long am_BPTR;
extern am_BPTR amDosOpen(const char *name, long mode) __asm__("_Open");
extern long amDosClose(am_BPTR fh) __asm__("_Close");
extern long amDosRead(am_BPTR fh, void *buf, long len) __asm__("_Read");
extern long amDosWrite(am_BPTR fh, const void *buf, long len) __asm__("_Write");
extern long amDosSeek(am_BPTR fh, long pos, long mode) __asm__("_Seek");
extern long amDosSetFileSize(am_BPTR fh, long pos, long mode) __asm__("_SetFileSize");
extern long amDosDeleteFile(const char *name) __asm__("_DeleteFile");
extern am_BPTR amDosLock(const char *name, long mode) __asm__("_Lock");
extern void amDosUnLock(am_BPTR lock) __asm__("_UnLock");
extern long amDosIoErr(void) __asm__("_IoErr");
extern void amDosDelay(long ticks) __asm__("_Delay");
extern long amDosNameFromLock(am_BPTR lock, char *buf, long len) __asm__("_NameFromLock");

#define AM_MODE_OLDFILE 1005
#define AM_MODE_READWRITE 1004
#define AM_OFFSET_BEGINNING (-1)
#define AM_OFFSET_CURRENT 0
#define AM_OFFSET_END 1
#define AM_SHARED_LOCK (-2)
#define AM_ERROR_OBJECT_NOT_FOUND 205

#define AMIGA_MAXPATH 512

/* One per database file any connection has open (journals and temporary
 * files get a private one). */
typedef struct AmLockInfo {
  struct AmLockInfo *next;
  char name[AMIGA_MAXPATH];
  int nRef;    /* open AmigaFiles on it */
  int nShared; /* AmigaFiles holding SHARED or more */
  int eLock;   /* the highest lock any of them holds */
} AmLockInfo;

typedef struct AmigaFile {
  sqlite3_file base;
  am_BPTR fh;
  int eLock;
  AmLockInfo *info;
  char *zDelete; /* name to remove on close, for delete-on-close files */
} AmigaFile;

static AmLockInfo *amLockList = 0;

static sqlite3_mutex *amTableMutex(void) {
  return sqlite3_mutex_alloc(SQLITE_MUTEX_STATIC_VFS1);
}

static int amSameName(const char *a, const char *b) {
  for (;; a++, b++) {
    char ca = *a, cb = *b;
    if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
    if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
    if (ca != cb) return 0;
    if (!ca) return 1;
  }
}

/* The file's full name, for the lock table. */
static void amKeyName(const char *zName, char *out, int n) {
  am_BPTR l = amDosLock(zName, AM_SHARED_LOCK);
  if (l) {
    long ok = amDosNameFromLock(l, out, n);
    amDosUnLock(l);
    if (ok) return;
  }
  sqlite3_snprintf(n, out, "%s", zName);
}

static AmLockInfo *amLockInfoGet(const char *zName) {
  char key[AMIGA_MAXPATH];
  AmLockInfo *p;
  sqlite3_mutex *m = amTableMutex();
  amKeyName(zName, key, sizeof(key));
  sqlite3_mutex_enter(m);
  for (p = amLockList; p; p = p->next)
    if (amSameName(p->name, key)) break;
  if (!p) {
    p = (AmLockInfo *)sqlite3_malloc(sizeof(*p));
    if (p) {
      memset(p, 0, sizeof(*p));
      sqlite3_snprintf(sizeof(p->name), p->name, "%s", key);
      p->next = amLockList;
      amLockList = p;
    }
  }
  if (p) p->nRef++;
  sqlite3_mutex_leave(m);
  return p;
}

static void amLockInfoRelease(AmLockInfo *info) {
  AmLockInfo **pp;
  sqlite3_mutex *m = amTableMutex();
  if (!info) return;
  sqlite3_mutex_enter(m);
  if (--info->nRef == 0) {
    for (pp = &amLockList; *pp; pp = &(*pp)->next)
      if (*pp == info) {
        *pp = info->next;
        break;
      }
    sqlite3_free(info);
  }
  sqlite3_mutex_leave(m);
}

/* Size of an open file: Seek() answers with the position it left, so the
 * second Seek() gives the end. Leaves the position at the end. */
static long amSize(am_BPTR fh) {
  if (amDosSeek(fh, 0, AM_OFFSET_END) == -1) return -1;
  return amDosSeek(fh, 0, AM_OFFSET_CURRENT);
}

/* Writes n zeros where the position is. */
static int amWriteZeros(am_BPTR fh, long n) {
  static const char zeros[1024];
  while (n > 0) {
    long k = n > (long)sizeof(zeros) ? (long)sizeof(zeros) : n;
    if (amDosWrite(fh, zeros, k) != k) return 0;
    n -= k;
  }
  return 1;
}

static int amClose(sqlite3_file *pFile) {
  AmigaFile *p = (AmigaFile *)pFile;
  if (p->fh) amDosClose(p->fh);
  p->fh = 0;
  if (p->info) {
    /* a connection closing with locks held lets them go */
    sqlite3_mutex *m = amTableMutex();
    sqlite3_mutex_enter(m);
    if (p->eLock > SQLITE_LOCK_SHARED) p->info->eLock = SQLITE_LOCK_SHARED;
    if (p->eLock >= SQLITE_LOCK_SHARED && --p->info->nShared == 0)
      p->info->eLock = SQLITE_LOCK_NONE;
    sqlite3_mutex_leave(m);
    p->eLock = SQLITE_LOCK_NONE;
    amLockInfoRelease(p->info);
    p->info = 0;
  }
  if (p->zDelete) {
    amDosDeleteFile(p->zDelete);
    sqlite3_free(p->zDelete);
    p->zDelete = 0;
  }
  return SQLITE_OK;
}

static int amRead(sqlite3_file *pFile, void *zBuf, int iAmt, sqlite_int64 iOfst) {
  AmigaFile *p = (AmigaFile *)pFile;
  long size = amSize(p->fh);
  long got = 0, n;
  if (size < 0) return SQLITE_IOERR_READ;
  if (iOfst < size) {
    if (amDosSeek(p->fh, (long)iOfst, AM_OFFSET_BEGINNING) == -1) return SQLITE_IOERR_READ;
    while (got < iAmt) {
      n = amDosRead(p->fh, (char *)zBuf + got, iAmt - got);
      if (n < 0) return SQLITE_IOERR_READ;
      if (n == 0) break;
      got += n;
    }
  }
  if (got < iAmt) {
    memset((char *)zBuf + got, 0, iAmt - got);
    return SQLITE_IOERR_SHORT_READ;
  }
  return SQLITE_OK;
}

static int amWrite(sqlite3_file *pFile, const void *zBuf, int iAmt, sqlite_int64 iOfst) {
  AmigaFile *p = (AmigaFile *)pFile;
  long size = amSize(p->fh); /* leaves the position at the end */
  long done = 0, n;
  if (size < 0) return SQLITE_IOERR_WRITE;
  if ((long)iOfst > size) {
    if (!amWriteZeros(p->fh, (long)iOfst - size)) return SQLITE_IOERR_WRITE;
  } else if (amDosSeek(p->fh, (long)iOfst, AM_OFFSET_BEGINNING) == -1) {
    return SQLITE_IOERR_WRITE;
  }
  while (done < iAmt) {
    n = amDosWrite(p->fh, (const char *)zBuf + done, iAmt - done);
    if (n <= 0) return SQLITE_IOERR_WRITE;
    done += n;
  }
  return SQLITE_OK;
}

static int amTruncate(sqlite3_file *pFile, sqlite_int64 nByte) {
  AmigaFile *p = (AmigaFile *)pFile;
  long size = amSize(p->fh);
  if (size < 0) return SQLITE_IOERR_TRUNCATE;
  if ((long)nByte > size) {
    /* longer: zeros, not what SetFileSize() would leave there */
    return amWriteZeros(p->fh, (long)nByte - size) ? SQLITE_OK : SQLITE_IOERR_TRUNCATE;
  }
  if ((long)nByte == size) return SQLITE_OK;
  /* SetFileSize() answers with the new size, or -1 */
  if (amDosSetFileSize(p->fh, (long)nByte, AM_OFFSET_BEGINNING) == -1) return SQLITE_IOERR_TRUNCATE;
  return SQLITE_OK;
}

static int amSync(sqlite3_file *pFile, int flags) {
  /* dos.library writes through to the file system; it has no fsync() */
  (void)pFile; (void)flags;
  return SQLITE_OK;
}

static int amFileSize(sqlite3_file *pFile, sqlite_int64 *pSize) {
  AmigaFile *p = (AmigaFile *)pFile;
  long size = amSize(p->fh);
  if (size < 0) return SQLITE_IOERR_FSTAT;
  *pSize = size;
  return SQLITE_OK;
}

/* As unixLock(): SHARED is counted, RESERVED/PENDING/EXCLUSIVE belong to
 * one file; EXCLUSIVE waits (BUSY, with PENDING kept) for the other
 * SHARED holders to go. SQLite's busy handler retries. */
static int amLock(sqlite3_file *pFile, int eLock) {
  AmigaFile *p = (AmigaFile *)pFile;
  AmLockInfo *info = p->info;
  sqlite3_mutex *m;
  int rc = SQLITE_OK;
  if (p->eLock >= eLock) return SQLITE_OK;
  if (!info) return SQLITE_IOERR_LOCK;
  m = amTableMutex();
  sqlite3_mutex_enter(m);
  if (p->eLock != info->eLock &&
      (info->eLock >= SQLITE_LOCK_PENDING || eLock > SQLITE_LOCK_SHARED)) {
    rc = SQLITE_BUSY;
  } else if (eLock == SQLITE_LOCK_SHARED) {
    if (info->eLock == SQLITE_LOCK_NONE) info->eLock = SQLITE_LOCK_SHARED;
    info->nShared++;
    p->eLock = SQLITE_LOCK_SHARED;
  } else if (eLock == SQLITE_LOCK_EXCLUSIVE && info->nShared > 1) {
    info->eLock = SQLITE_LOCK_PENDING;
    p->eLock = SQLITE_LOCK_PENDING;
    rc = SQLITE_BUSY;
  } else {
    info->eLock = eLock;
    p->eLock = eLock;
  }
  sqlite3_mutex_leave(m);
  return rc;
}

static int amUnlock(sqlite3_file *pFile, int eLock) {
  AmigaFile *p = (AmigaFile *)pFile;
  AmLockInfo *info = p->info;
  sqlite3_mutex *m;
  if (p->eLock <= eLock) return SQLITE_OK;
  if (!info) return SQLITE_OK;
  m = amTableMutex();
  sqlite3_mutex_enter(m);
  if (p->eLock > SQLITE_LOCK_SHARED) {
    info->eLock = SQLITE_LOCK_SHARED;
    p->eLock = SQLITE_LOCK_SHARED;
  }
  if (eLock == SQLITE_LOCK_NONE && p->eLock == SQLITE_LOCK_SHARED) {
    if (--info->nShared == 0) info->eLock = SQLITE_LOCK_NONE;
    p->eLock = SQLITE_LOCK_NONE;
  }
  sqlite3_mutex_leave(m);
  return SQLITE_OK;
}

static int amCheckReservedLock(sqlite3_file *pFile, int *pResOut) {
  AmigaFile *p = (AmigaFile *)pFile;
  sqlite3_mutex *m = amTableMutex();
  sqlite3_mutex_enter(m);
  *pResOut = p->info && p->info->eLock > SQLITE_LOCK_SHARED;
  sqlite3_mutex_leave(m);
  return SQLITE_OK;
}

static int amFileControl(sqlite3_file *pFile, int op, void *pArg) {
  AmigaFile *p = (AmigaFile *)pFile;
  if (op == SQLITE_FCNTL_LOCKSTATE) {
    *(int *)pArg = p->eLock;
    return SQLITE_OK;
  }
  return SQLITE_NOTFOUND;
}

static int amSectorSize(sqlite3_file *pFile) { (void)pFile; return 512; }
static int amDeviceCharacteristics(sqlite3_file *pFile) { (void)pFile; return 0; }

static const sqlite3_io_methods amIo = {
  1, amClose, amRead, amWrite, amTruncate, amSync, amFileSize,
  amLock, amUnlock, amCheckReservedLock, amFileControl,
  amSectorSize, amDeviceCharacteristics,
  0, 0, 0, 0, 0, 0
};

static int amExists(const char *zName) {
  am_BPTR l = amDosLock(zName, AM_SHARED_LOCK);
  if (!l) return 0;
  amDosUnLock(l);
  return 1;
}

static int amOpen(sqlite3_vfs *pVfs, const char *zName, sqlite3_file *pFile, int flags, int *pOutFlags) {
  AmigaFile *p = (AmigaFile *)pFile;
  static unsigned tmpCount = 0;
  char tmpName[64];
  (void)pVfs;
  memset(p, 0, sizeof(*p));

  if (zName == 0) {
    sqlite3_snprintf(sizeof(tmpName), tmpName, "T:sqlite_%08lx_%u",
                     (unsigned long)time(0), ++tmpCount);
    zName = tmpName;
    flags |= SQLITE_OPEN_DELETEONCLOSE | SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE;
  }

  if ((flags & SQLITE_OPEN_EXCLUSIVE) && amExists(zName)) return SQLITE_CANTOPEN;
  /* MODE_READWRITE: an old file as it is, else a new one; MODE_OLDFILE
   * (which may be written too) when SQLite must not create it */
  p->fh = amDosOpen(zName, (flags & SQLITE_OPEN_CREATE) ? AM_MODE_READWRITE : AM_MODE_OLDFILE);
  if (!p->fh) return SQLITE_CANTOPEN;

  if (flags & SQLITE_OPEN_MAIN_DB) {
    p->info = amLockInfoGet(zName);
  } else {
    /* journals and temporary files belong to one connection: a private
     * entry, never in the table (amLockInfoRelease() frees it) */
    p->info = (AmLockInfo *)sqlite3_malloc(sizeof(AmLockInfo));
    if (p->info) {
      memset(p->info, 0, sizeof(AmLockInfo));
      p->info->nRef = 1;
    }
  }
  if (!p->info) {
    amDosClose(p->fh);
    p->fh = 0;
    return SQLITE_NOMEM;
  }
  if (flags & SQLITE_OPEN_DELETEONCLOSE) p->zDelete = sqlite3_mprintf("%s", zName);
  if (pOutFlags) *pOutFlags = flags;
  p->base.pMethods = &amIo;
  return SQLITE_OK;
}

static int amDelete(sqlite3_vfs *pVfs, const char *zPath, int dirSync) {
  (void)pVfs; (void)dirSync;
  if (!amDosDeleteFile(zPath)) {
    if (amDosIoErr() == AM_ERROR_OBJECT_NOT_FOUND) return SQLITE_IOERR_DELETE_NOENT;
    return SQLITE_IOERR_DELETE;
  }
  return SQLITE_OK;
}

static int amAccess(sqlite3_vfs *pVfs, const char *zPath, int flags, int *pResOut) {
  (void)pVfs; (void)flags; /* exists; read and write access are not checked */
  *pResOut = amExists(zPath);
  return SQLITE_OK;
}

static int amFullPathname(sqlite3_vfs *pVfs, const char *zPath, int nOut, char *zOut) {
  (void)pVfs;
  sqlite3_snprintf(nOut, zOut, "%s", zPath);
  return SQLITE_OK;
}

static int amRandomness(sqlite3_vfs *pVfs, int nByte, char *zOut) {
  struct timeval tv;
  unsigned long x;
  int i;
  (void)pVfs;
  gettimeofday(&tv, 0);
  x = (unsigned long)tv.tv_sec * 1000003UL ^ (unsigned long)tv.tv_usec ^ (unsigned long)(size_t)zOut;
  for (i = 0; i < nByte; i++) {
    x = x * 1103515245UL + 12345UL;
    zOut[i] = (char)(x >> 16);
  }
  return nByte;
}

static int amSleep(sqlite3_vfs *pVfs, int microseconds) {
  long ticks = (microseconds + 19999) / 20000; /* 50 ticks a second */
  (void)pVfs;
  if (ticks < 1) ticks = 1;
  amDosDelay(ticks);
  return (int)(ticks * 20000);
}

static int amCurrentTime(sqlite3_vfs *pVfs, double *pTime) {
  struct timeval tv;
  (void)pVfs;
  gettimeofday(&tv, 0);
  *pTime = 2440587.5 + ((double)tv.tv_sec + tv.tv_usec / 1000000.0) / 86400.0;
  return SQLITE_OK;
}

static int amGetLastError(sqlite3_vfs *pVfs, int n, char *z) {
  (void)pVfs; (void)n; (void)z;
  return (int)amDosIoErr();
}

static sqlite3_vfs amVfs = {
  1, sizeof(AmigaFile), AMIGA_MAXPATH, 0, "amiga", 0,
  amOpen, amDelete, amAccess, amFullPathname,
  0, 0, 0, 0,
  amRandomness, amSleep, amCurrentTime, amGetLastError,
  0, 0, 0, 0
};

int sqlite3_os_init(void) {
  return sqlite3_vfs_register(&amVfs, 1);
}

int sqlite3_os_end(void) {
  return SQLITE_OK;
}

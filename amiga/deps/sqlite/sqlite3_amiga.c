/*
 * SQLite for AmigaOS 3.x: the amalgamation, its mutexes on Exec's signal
 * semaphores (recursive by nature, as SQLite's recursive mutexes must be)
 * and its file system (amiga_vfs.c), in one unit, because the amalgamation
 * keeps sqlite3DefaultMutex() static. sqlite3_patched.c is sqlite3.c with
 * one change, made by build-m68k-libs.sh: SQLITE_MUTEX_APPDEF stops it
 * choosing its no-op mutexes for an OS it does not know. Programs that run
 * SQLite from more than one task (SDL threads) are then safe, as with
 * SQLITE_THREADSAFE=1 elsewhere.
 * The AmiCradle Team; public domain, like SQLite.
 */
#include "sqlite3_patched.c"

/* Exec through libamiga's stubs (libnix gives SysBase): no NDK headers,
 * which would meet the amalgamation's own names. */
extern void InitSemaphore(void *sem);
extern void ObtainSemaphore(void *sem);
extern unsigned long AttemptSemaphore(void *sem);
extern void ReleaseSemaphore(void *sem);

struct sqlite3_mutex {
  union {
    long align;
    unsigned char bytes[64]; /* a struct SignalSemaphore is 46 */
  } sem;
  int id;
};

#define AMIGA_STATIC_MUTEXES 12 /* SQLITE_MUTEX_STATIC_MAIN .. _VFS3 */
static sqlite3_mutex amigaStatic[AMIGA_STATIC_MUTEXES];
static int amigaMutexReady = 0;

static int amigaMutexInit(void) {
  int i;
  if (!amigaMutexReady) {
    for (i = 0; i < AMIGA_STATIC_MUTEXES; i++) {
      memset(&amigaStatic[i], 0, sizeof(amigaStatic[i]));
      InitSemaphore(&amigaStatic[i].sem);
      amigaStatic[i].id = i + 2;
    }
    amigaMutexReady = 1;
  }
  return SQLITE_OK;
}

static int amigaMutexEnd(void) { return SQLITE_OK; }

static sqlite3_mutex *amigaMutexAlloc(int id) {
  sqlite3_mutex *p;
  if (id == SQLITE_MUTEX_FAST || id == SQLITE_MUTEX_RECURSIVE) {
    p = (sqlite3_mutex *)sqlite3MallocZero(sizeof(*p));
    if (p) {
      InitSemaphore(&p->sem);
      p->id = id;
    }
    return p;
  }
  if (id < 2 || id - 2 >= AMIGA_STATIC_MUTEXES) return 0;
  if (!amigaMutexReady) amigaMutexInit();
  return &amigaStatic[id - 2];
}

static void amigaMutexFree(sqlite3_mutex *p) {
  if (p && (p->id == SQLITE_MUTEX_FAST || p->id == SQLITE_MUTEX_RECURSIVE)) {
    sqlite3_free(p);
  }
}

static void amigaMutexEnter(sqlite3_mutex *p) { ObtainSemaphore(&p->sem); }
static int amigaMutexTry(sqlite3_mutex *p) {
  return AttemptSemaphore(&p->sem) ? SQLITE_OK : SQLITE_BUSY;
}
static void amigaMutexLeave(sqlite3_mutex *p) { ReleaseSemaphore(&p->sem); }

/* One 68k CPU: a compiler barrier is all a memory barrier needs to be. */
SQLITE_PRIVATE void sqlite3MemoryBarrier(void) {
  __asm__ __volatile__("" ::: "memory");
}

SQLITE_PRIVATE sqlite3_mutex_methods const *sqlite3DefaultMutex(void) {
  static const sqlite3_mutex_methods sMutex = {
    amigaMutexInit, amigaMutexEnd, amigaMutexAlloc, amigaMutexFree,
    amigaMutexEnter, amigaMutexTry, amigaMutexLeave, 0, 0
  };
  return &sMutex;
}

#include "amiga_vfs.c"

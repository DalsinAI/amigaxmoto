/*
 * SQLiteTest: the Amiga SQLite build (amiga_vfs.c) on its own, as X-Moto
 * uses it. Run from a Shell:
 *
 *     SQLiteTest [DIR]      (DIR defaults to T:, e.g. PROGDIR: or RAM:)
 *
 * It removes DIR/sqltest.db, then: creates a table and puts 200 rows in
 * it in one transaction; checks the file starts "SQLite format 3"; reads
 * the rows back through a second connection, as X-Moto's threads do;
 * writes from the second while the first holds a read open (it must get
 * SQLITE_BUSY, then succeed after); runs PRAGMA integrity_check; closes,
 * reopens and checks the sum again; grows and shrinks the file (VACUUM).
 * Each step prints ok or FAIL; the last line is PASS or FAIL, and the
 * return code 0 or 20.
 * The AmiCradle Team. GPL 2 or later, like X-Moto.
 */
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>

static int failures = 0;

static void check(int ok, const char *what, sqlite3 *db) {
  printf("%-52s %s", what, ok ? "ok" : "FAIL");
  if (!ok && db) printf(" (%s)", sqlite3_errmsg(db));
  printf("\n");
  if (!ok) failures++;
}

static int exec(sqlite3 *db, const char *sql) {
  char *err = 0;
  int rc = sqlite3_exec(db, sql, 0, 0, &err);
  if (rc != SQLITE_OK) {
    printf("  sql: %s\n  error %d: %s\n", sql, rc, err ? err : "?");
    sqlite3_free(err);
  }
  return rc;
}

static long long one(sqlite3 *db, const char *sql, int *rcOut) {
  sqlite3_stmt *st = 0;
  long long v = -1;
  int rc = sqlite3_prepare_v2(db, sql, -1, &st, 0);
  if (rc == SQLITE_OK) {
    rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) {
      v = sqlite3_column_int64(st, 0);
      rc = SQLITE_OK;
    }
  }
  sqlite3_finalize(st);
  if (rcOut) *rcOut = rc;
  return v;
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "T:";
  char path[512], journal[540];
  size_t n = strlen(dir);
  sqlite3 *a = 0, *b = 0;
  sqlite3_stmt *rd = 0;
  unsigned char head[16];
  FILE *f;
  int rc, i;
  long long v;

  snprintf(path, sizeof(path), "%s%s%s", dir,
           (n && dir[n - 1] != ':' && dir[n - 1] != '/') ? "/" : "",
           "sqltest.db");
  snprintf(journal, sizeof(journal), "%s-journal", path);
  remove(path);
  remove(journal);
  printf("SQLiteTest %s, SQLite %s, threadsafe %d\n", path,
         sqlite3_libversion(), sqlite3_threadsafe());

  rc = sqlite3_open(path, &a);
  check(rc == SQLITE_OK, "open (new file)", a);
  if (rc != SQLITE_OK) goto done;
  sqlite3_busy_timeout(a, 0);

  check(exec(a, "CREATE TABLE t(id INTEGER PRIMARY KEY, name TEXT, x REAL);") == SQLITE_OK,
        "create table", a);
  check(exec(a, "BEGIN;") == SQLITE_OK, "begin", a);
  for (i = 1; i <= 200; i++) {
    char sql[160];
    snprintf(sql, sizeof(sql),
             "INSERT INTO t(id,name,x) VALUES(%d,'row %d, a name long enough to fill pages',%d.5);",
             i, i, i);
    if (exec(a, sql) != SQLITE_OK) break;
  }
  check(i == 201, "insert 200 rows", a);
  check(exec(a, "COMMIT;") == SQLITE_OK, "commit", a);

  memset(head, 0, sizeof(head));
  f = fopen(path, "rb");
  if (f) {
    fread(head, 1, sizeof(head), f);
    fclose(f);
  }
  check(memcmp(head, "SQLite format 3", 16) == 0, "file starts \"SQLite format 3\"", 0);
  if (memcmp(head, "SQLite format 3", 16) != 0) {
    printf("  first bytes:");
    for (i = 0; i < 16; i++) printf(" %02x", head[i]);
    printf("\n");
  }

  v = one(a, "SELECT sum(id) FROM t;", &rc);
  check(rc == SQLITE_OK && v == 20100, "read back, same connection (sum 20100)", a);

  rc = sqlite3_open(path, &b);
  check(rc == SQLITE_OK, "second connection", b);
  if (rc != SQLITE_OK) goto done;
  sqlite3_busy_timeout(b, 0);
  v = one(b, "SELECT count(*) FROM t;", &rc);
  check(rc == SQLITE_OK && v == 200, "read back, second connection (200 rows)", b);
  v = one(b, "SELECT name FROM t WHERE id=7;", &rc) ;
  (void)v;

  /* a holds a read open: b may not commit a write until it ends */
  rc = sqlite3_prepare_v2(a, "SELECT id FROM t;", -1, &rd, 0);
  if (rc == SQLITE_OK) rc = sqlite3_step(rd);
  check(rc == SQLITE_ROW, "first connection starts a read", a);
  rc = sqlite3_exec(b, "INSERT INTO t(id,name,x) VALUES(201,'from b',0);", 0, 0, 0);
  check(rc == SQLITE_BUSY, "second connection's write waits (SQLITE_BUSY)", b);
  sqlite3_finalize(rd);
  rd = 0;
  check(exec(b, "INSERT INTO t(id,name,x) VALUES(201,'from b',0);") == SQLITE_OK,
        "second connection's write after the read", b);
  v = one(a, "SELECT count(*) FROM t;", &rc);
  check(rc == SQLITE_OK && v == 201, "first connection sees it (201 rows)", a);

  v = one(a, "SELECT x*2 FROM t WHERE id=100;", &rc);
  check(rc == SQLITE_OK && v == 201, "a REAL column (100.5*2 = 201)", a);

  {
    sqlite3_stmt *st = 0;
    int ok = 0;
    if (sqlite3_prepare_v2(a, "PRAGMA integrity_check;", -1, &st, 0) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW) {
      const unsigned char *t = sqlite3_column_text(st, 0);
      ok = t && strcmp((const char *)t, "ok") == 0;
      if (!ok) printf("  integrity_check: %s\n", t ? (const char *)t : "?");
    }
    sqlite3_finalize(st);
    check(ok, "integrity_check", a);
  }

  sqlite3_close(b);
  b = 0;
  check(exec(a, "DELETE FROM t WHERE id > 20;") == SQLITE_OK, "delete most rows", a);
  check(exec(a, "VACUUM;") == SQLITE_OK, "vacuum (the file shrinks)", a);
  sqlite3_close(a);
  a = 0;

  rc = sqlite3_open(path, &a);
  check(rc == SQLITE_OK, "reopen", a);
  if (rc != SQLITE_OK) goto done;
  v = one(a, "SELECT sum(id) FROM t;", &rc);
  check(rc == SQLITE_OK && v == 210, "after reopening (sum 210)", a);
  f = fopen(journal, "rb");
  check(f == 0, "no journal left behind", 0);
  if (f) fclose(f);

done:
  if (rd) sqlite3_finalize(rd);
  if (b) sqlite3_close(b);
  if (a) sqlite3_close(a);
  printf("%s\n", failures ? "FAIL" : "PASS");
  return failures ? 20 : 0;
}

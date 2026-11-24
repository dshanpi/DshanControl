package main

/*
#cgo LDFLAGS: -lsqlite3
#include <sqlite3.h>
#include <stdlib.h>
static int bind_text(sqlite3_stmt *s, int n, const char *v) {
    return sqlite3_bind_text(s, n, v, -1, SQLITE_TRANSIENT);
}
*/
import "C"
import (
	"errors"
	"os"
	"path/filepath"
	"sync"
	"syscall"
	"unsafe"
)

// The original gateway's SQLite schema is retained, including unknown commands.
// Reservations commit with synchronous=FULL before any upstream request.
type commandStore struct {
	mu sync.Mutex
	db *C.sqlite3
}

func privatePath(path string, directory bool) error {
	st, err := os.Lstat(path)
	if err != nil {
		return err
	}
	unix, ok := st.Sys().(*syscall.Stat_t)
	if !ok || int(unix.Uid) != os.Geteuid() || st.Mode().Perm()&0077 != 0 || st.Mode()&os.ModeSymlink != 0 || st.IsDir() != directory || (!directory && !st.Mode().IsRegular()) {
		return errors.New("private owner-only path required")
	}
	return nil
}
func openStore(path string) (*commandStore, error) {
	if !filepath.IsAbs(path) {
		return nil, errors.New("absolute command database required")
	}
	if err := privatePath(filepath.Dir(path), true); err != nil {
		return nil, err
	}
	if _, err := os.Lstat(path); err == nil {
		if err = privatePath(path, false); err != nil {
			return nil, err
		}
	} else if !os.IsNotExist(err) {
		return nil, err
	}
	s := &commandStore{}
	p := C.CString(path)
	defer C.free(unsafe.Pointer(p))
	if C.sqlite3_open_v2(p, &s.db, C.SQLITE_OPEN_READWRITE|C.SQLITE_OPEN_CREATE|C.SQLITE_OPEN_FULLMUTEX|C.SQLITE_OPEN_NOFOLLOW, nil) != C.SQLITE_OK {
		s.close()
		return nil, errors.New("database open failed")
	}
	C.sqlite3_busy_timeout(s.db, 5000)
	for _, sql := range []string{"PRAGMA journal_mode=WAL", "PRAGMA synchronous=FULL", "CREATE TABLE IF NOT EXISTS commands (key TEXT PRIMARY KEY, digest TEXT NOT NULL, state TEXT NOT NULL)"} {
		if err := s.exec(sql); err != nil {
			s.close()
			return nil, err
		}
	}
	if err := os.Chmod(path, 0600); err != nil {
		s.close()
		return nil, err
	}
	return s, nil
}
func (s *commandStore) close() {
	if s.db != nil {
		C.sqlite3_close(s.db)
		s.db = nil
	}
}
func (s *commandStore) exec(sql string, values ...string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	text := C.CString(sql)
	defer C.free(unsafe.Pointer(text))
	var stmt *C.sqlite3_stmt
	if C.sqlite3_prepare_v2(s.db, text, -1, &stmt, nil) != C.SQLITE_OK {
		return errors.New("database prepare failed")
	}
	defer C.sqlite3_finalize(stmt)
	for i, value := range values {
		p := C.CString(value)
		rc := C.bind_text(stmt, C.int(i+1), p)
		C.free(unsafe.Pointer(p))
		if rc != C.SQLITE_OK {
			return errors.New("database bind failed")
		}
	}
	for {
		rc := C.sqlite3_step(stmt)
		if rc == C.SQLITE_DONE {
			return nil
		}
		if rc == C.SQLITE_CONSTRAINT {
			return errDuplicate
		}
		if rc != C.SQLITE_ROW {
			return errors.New("database write failed")
		}
	}
}

var errDuplicate = errors.New("duplicate or outcome unknown")

func (s *commandStore) reserve(key, digest string) error {
	return s.exec("INSERT INTO commands VALUES (?,?,?)", key, digest, "unknown")
}
func (s *commandStore) accepted(key string) error {
	return s.exec("UPDATE commands SET state=? WHERE key=?", "accepted", key)
}

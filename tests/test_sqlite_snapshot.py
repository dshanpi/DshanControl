import contextlib
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from snapshot_sqlite import snapshot


class SQLiteSnapshotTests(unittest.TestCase):
    def test_live_wal_is_copied_closed_and_independent_of_the_source(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, destination = root / 'live #1.db', root / 'sealed.db'
            with contextlib.closing(sqlite3.connect(str(source))) as writer:
                writer.execute('PRAGMA journal_mode=WAL')
                writer.execute('PRAGMA wal_autocheckpoint=0')
                writer.execute('CREATE TABLE records(value TEXT)')
                writer.executemany('INSERT INTO records VALUES(?)', [('one',), ('two',)])
                writer.commit()
                self.assertGreater(Path(str(source) + '-wal').stat().st_size, 0)
                writer.execute("INSERT INTO records VALUES('uncommitted')")
                snapshot(source, destination)
                self.assertEqual(0o600, destination.stat().st_mode & 0o777)
                for suffix in ('-wal', '-shm', '-journal'):
                    self.assertFalse(Path(str(destination) + suffix).exists())
                with contextlib.closing(sqlite3.connect(str(destination))) as saved:
                    self.assertEqual([('one',), ('two',)], saved.execute('SELECT * FROM records').fetchall())
                    self.assertEqual('delete', saved.execute('PRAGMA journal_mode').fetchone()[0])
                self.assertEqual(3, writer.execute('SELECT count(*) FROM records').fetchone()[0])
                writer.rollback()
            source.unlink()
            with contextlib.closing(sqlite3.connect(str(destination))) as saved:
                self.assertEqual(2, saved.execute('SELECT count(*) FROM records').fetchone()[0])

    def test_existing_backup_and_linked_source_are_preserved(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, destination = root / 'source.db', root / 'backup.db'
            with contextlib.closing(sqlite3.connect(str(source))) as db:
                db.execute('CREATE TABLE records(value TEXT)')
            destination.write_bytes(b'previous backup')
            with self.assertRaises(FileExistsError):
                snapshot(source, destination)
            self.assertEqual(b'previous backup', destination.read_bytes())
            linked = root / 'linked.db'
            linked.symlink_to(source)
            with self.assertRaises(ValueError):
                snapshot(linked, root / 'new.db')
            self.assertFalse((root / 'new.db').exists())

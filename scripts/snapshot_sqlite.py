#!/usr/bin/env python3
"""Seal a private SQLite lab backup; never installed as a production service."""
import contextlib
import os
from pathlib import Path
import sqlite3
import time


def snapshot(source, destination, timeout_seconds=30):
    source, destination = Path(source), Path(destination)
    if source.is_symlink() or not source.is_file():
        raise ValueError('snapshot source must be an existing regular database')
    if timeout_seconds <= 0:
        raise ValueError('snapshot deadline must be positive')
    # Exclusive creation preserves any existing backup, including dangling links.
    descriptor = os.open(str(destination), os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
    os.close(descriptor)
    deadline = time.monotonic() + timeout_seconds

    def progress(status, remaining, total):
        if time.monotonic() >= deadline:
            raise TimeoutError('SQLite snapshot deadline exceeded')

    uri = source.resolve().as_uri() + '?mode=ro'
    # Connection.__exit__ commits/rolls back but does not close the connection.
    with contextlib.closing(sqlite3.connect(uri, uri=True, timeout=2)) as original:
        with contextlib.closing(sqlite3.connect(str(destination), timeout=2)) as copy:
            original.backup(copy, pages=128, progress=progress, sleep=0.05)
            copy.commit()
            # This changes only the new snapshot, never the operational source.
            if copy.execute('PRAGMA journal_mode=DELETE').fetchone()[0] != 'delete':
                raise ValueError('snapshot could not be sealed without WAL')
            if copy.execute('PRAGMA quick_check').fetchall() != [('ok',)]:
                raise ValueError('snapshot consistency check failed')
    for suffix in ('-wal', '-shm', '-journal'):
        if Path(str(destination) + suffix).exists():
            raise ValueError('snapshot has unsealed SQLite auxiliary files')
    with destination.open('rb') as stream:
        os.fsync(stream.fileno())
    directory = os.open(str(destination.parent), os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(directory)
    finally:
        os.close(directory)

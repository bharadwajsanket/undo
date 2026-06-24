# Security Policy

## Data Integrity Goals

Data integrity is the highest priority in Undo. The codebase follows the order of priority:
1. **Data Integrity** (Never lose or corrupt a user's file).
2. **Reliability & Crash Safety** (Survive crashes, power failures, or interrupts).
3. **Storage Efficiency** (Compress files and save disk space).
4. **Performance** (Acceptable 200ms latency to guarantee safety).

To fulfill these priorities, Undo implements the following mechanisms:
- **Copy-Before-Delete**: Original files are only unlinked from their original paths *after* they are successfully written to the local object store and flushed to disk.
- **Synchronized Writes**: All files and directories written to the objects database are synced to physical media using `fsync()` or directory file descriptor synchronization before the transaction is finalized.

## Journal-Based Recovery

Undo uses an append-only journal (`~/.undo/journal.log`) to log transaction lifecycle states:
- When a delete command starts, a `START` record along with target `FILE` records is written and synced to disk. This functions as a **Write-Ahead Log (WAL)**.
- Once files are successfully stored in `~/.undo/objects/`, a `COMMIT` record is written and synced.
- If the utility is aborted, killed, or loses power *after* writing the `START` entry but *before* writing the `COMMIT` entry, the transaction is considered `PENDING`.
- Upon next execution, Undo automatically scans the journal, detects any `PENDING` transaction, restores any moved files back to their original paths, deletes partial objects, and logs an `ABORT` record to return the system to its initial state.

## Collision Protection

Each deletion operation is assigned a unique random 6-character hexadecimal ID (e.g. `a81f92`). 
- IDs are generated from high-entropy system random streams via `/dev/urandom` (falling back to a pseudo-random generator seeded with PID and time if `/dev/urandom` is unavailable).
- Before completing a deletion, the program parses the active transaction history to guarantee that the generated ID does not collide with any committed or pending transaction. If a collision is detected, a new ID is generated.

## Security Limitations

- **Local Multi-User Isolation**: Undo configures `~/.undo/` with permission mode `0700` (read, write, execute permissions restricted only to the owner). While this protects deleted files from standard local users, files are still readable by system administrators (`root`).
- **No Encryption**: Undo does not encrypt deleted files stored in the object directory. If a storage device is stolen or compromised, files can be read directly unless full-disk encryption (e.g., FileVault or LUKS) is active.
- **Disk Space Depletion**: Undo relies on the local disk space of the user's home folder. If the home directory partition is filled, deletions of files will fail. Undo fails safely in this case, leaving original files untouched in their original paths.

## Reporting Vulnerabilities

If you identify a security issue or vulnerability in Undo, please report it immediately:
- Email: security@example.com (Placeholder)
- Please do not open public GitHub issues for security-sensitive bugs. Send details and reproduction steps directly via email to allow a coordinated disclosure.

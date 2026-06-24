# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [v0.1.0] - 2026-06-24

### Added
- **rm Command Interception**: Intercepts deletions transparently via shell aliases (`undo install`) for Bash and Zsh.
- **Write-Ahead Logging (WAL)**: Journal file (`~/.undo/journal.log`) logs all actions (`START`, `FILE`, `COMMIT`, `ABORT`, `UNDO`) with atomic `fsync()` flushes to guarantee reliability.
- **Interruption Rollback**: On startup, scans the journal and rolls back any pending (partially completed) transaction.
- **zlib Compression**: Deflate compression for regular files exceeding the threshold when storage-beneficial.
- **Frictionless Restoration**: Restores files back to their original paths, recreating missing parent directories.
- **Overwrite Protection**: Safely halts restoration if a file already exists at the target path.
- **Symbolic Link & Directory Support**: Full support for recursive folder deletions (`rm -r`) and symbolic links.
- **Interactive CLI Configuration**: Terminal configuration menu (`undo config`) to tune limits and toggle settings.
- **Log & History Reporting**: History lists (`undo history`) and storage efficiency metrics (`undo stats`).
- **Clean Storage Utility**: Purges objects database and clears history log (`undo clean`).

### Changed
- Initial release of the Undo utility.

### Known Limitations
- Supports only standard file and directory deletions executed through standard `rm` terminal commands.
- Performance on cross-device deletes is subject to system drive copying speeds.
- Does not run background services for auto-purging (storage cleanups must be run manually).

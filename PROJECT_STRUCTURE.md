# Project Structure

This document describes the organization of the Undo repository to assist future developers.

## Folder Organization

```
undo/
├── src/
│   ├── main.c           # CLI entry point, rm arg parsing, shell integration
│   ├── config.c/h       # Configuration parser and interactive setup menu
│   ├── storage.c/h      # Write-Ahead Log journal engine & rollback routines
│   └── utils.c/h        # Atomic copying, recursive deleting, and hex encoding
├── tests/
│   ├── test_undo.sh     # Integration and recovery test cases
│   └── stress_test.sh   # Performance and load testing script
├── assets/
│   └── screenshots/     # Screenshots and visual media descriptors
├── Makefile             # Compiler options and zlib linking configuration
├── LICENSE              # Open-source MIT License terms
├── README.md            # High-level overview and user-facing instructions
├── CHANGELOG.md         # Release history matching semantic versioning
├── SECURITY.md          # Technical analysis of security parameters
├── CONTRIBUTING.md      # Development setup and pull request expectations
└── RELEASE_CHECKLIST.md # Release procedure sanity checklist
```

---

## Component Responsibilities

### 1. CLI Router & Shell Interceptor (`src/main.c`)
- **CLI Parsing**: Receives commands and routes them to subcommands (`history`, `stats`, `config`, `clean`, `install`, `uninstall`).
- **Option Interception**: Intercepts `rm` invocations via `--rm`. Parses flags (`-f`, `-r`, `-R`, `-d`, `-v`, `-i`, `--`) to construct file target list.
- **Installer**: Configures shell aliases within Bash and Zsh RC configuration profiles.

### 2. Transaction Engine (`src/storage.c` / `src/storage.h`)
- **Journal Manager**: Appends synchronized logs (`START`, `FILE`, `COMMIT`, `ABORT`, `UNDO`) to the write-ahead log (`~/.undo/journal.log`).
- **History Loader**: Parses the journal in a single pass to build transaction logs and determine which blocks are active, restored, or rolled back.
- **Interruption Sweeper**: Runs on startup to sweep for `PENDING` states (incomplete transactions due to a process crash) and restore partially deleted files.
- **Decompressor**: Decodes and restores zlib-deflated regular files during undo recovery.

### 3. Preferences (`src/config.c` / `src/config.h`)
- **Config Storage**: Manages settings inside `~/.undo/config`.
- **Setup Wizard**: Implements the console interactive interface to modify thresholds, toggle compression, or invoke system text editors.

### 4. System Utilities (`src/utils.c` / `src/utils.h`)
- **Safe Path Resolver**: Resolves paths without following symbolic links. Enables handling of broken symlinks.
- **Recursive Handlers**: Implements depth-first recursive copying and directory deletion.
- **Hex Encoder**: Encodes path strings to safe hexadecimal form for the journal log, preventing path characters from interfering with delimiters.
- **Entropy ID Generator**: Generates 6-character random hex transaction IDs via `/dev/urandom`.

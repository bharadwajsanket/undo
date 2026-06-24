# Undo

Bring Ctrl+Z to terminal file deletions.

Undo is a lightweight, daemonless command-line utility written in C17 that intercepts the standard `rm` command, backs up deleted files locally, and enables instant restoration. It provides a simple recovery mechanism for terminal file deletions without background services, database servers, or filesystem monitors.

---

## Overview

Undo works by substituting or aliasing the standard shell `rm` call. When you delete a file, Undo moves it into a private directory structure (`~/.undo/objects/`) and records the transaction in an append-only log file (`~/.undo/journal.log`). If you make a mistake, you can recover the file back to its original location by running `undo`.

---

## Motivation

Command-line environments lack a trash bin by default. Destructive operations like `rm` are permanent and unforgiving. While trash-cli and snapshot-based tools exist, they are often complex, require background services, or depend on external database libraries (e.g. SQLite). Undo is a self-contained C utility that does one thing: make `rm` safe and reversible, with zero background processes and minimal resource usage.

---

## Features

- **Transaction Journaling**: Employs a Write-Ahead Logging (WAL) state machine. File metadata and states are synchronized to disk before the original file is unlinked.
- **Interruption Recovery**: On startup, Undo detects and rolls back any pending (partially completed) transaction resulting from system crashes or power failures.
- **Adaptive Compression**: Compresses files on-the-fly using `zlib` if they exceed the compression threshold and the compression check determines it is storage-beneficial.
- **Collision Protection**: Generates random 6-character hex transaction IDs (e.g., `a81f92`) and verifies uniqueness against history before write operations.
- **Overwrite Safeguard**: Prioritizes data safety. If a target file already exists during restoration, the command fails safely to prevent accidental overwrites.
- **No Background Daemon**: Runs, performs its filesystem tasks, and exits immediately. No persistent CPU or memory overhead.

---

## Installation

### Prerequisites
- macOS or Linux
- A C compiler supporting standard C17 (`clang` or `gcc`)
- `zlib` library and headers (standard on macOS; installable via `libz-dev` or `zlib-devel` on Linux)

### Compiling
Clone the repository and compile using the provided Makefile:
```bash
git clone https://github.com/bharadwajsanket/undo.git
cd undo
make
```
This generates the standalone binary `undo` in the root directory. Place the binary in your system `PATH` (for example, `/usr/local/bin/`).

---

## Shell Integration

To enable transparent interception of `rm` commands, run:
```bash
undo install
```
This detects your active shell (Bash or Zsh) and appends the integration block to your RC file (`~/.bashrc` or `~/.zshrc`):
```bash
# >>> UNDO Shell Integration >>>
alias rm="undo --rm"
# <<< UNDO Shell Integration <<<
```
Source your shell configuration or restart the terminal to activate.

To disable integration, run:
```bash
undo uninstall
```

---

## Usage Examples

### Delete a File
```bash
rm thesis.pdf
```
Output:
```
Stored in Undo
ID: a81f92
```

### Restore the Most Recent Deletion
```bash
undo
```
Output:
```
Restored thesis.pdf
```

### Restore a Specific Transaction
```bash
undo a81f92
```

### Display Deletion History
```bash
undo history
```
Output:
```
=== UNDO Deletion History ===
ID       Date & Time          Size         Deleted Path(s)
--------------------------------------------------------------------------------
a81f92   2026-06-24 13:51:36  12.4 MB      /Users/sanket/thesis.pdf
b6ce44   2026-06-24 13:40:12  45.2 KB      /Users/sanket/notes.txt
```

### Show Storage Statistics
```bash
undo stats
```

### Configure Settings
Adjust thresholds and toggle compression interactively:
```bash
undo config
```

### Purge Storage
```bash
undo clean
```

---

## Screenshots

### File Deletion
![File Deletion](assets/screenshots/delete-file.png)

### File Restoration
![File Restoration](assets/screenshots/restore-file.png)

### History Output
![History Output](assets/screenshots/history.png)

### Storage Statistics
![Storage Statistics](assets/screenshots/stats.png)

---

## Storage Architecture

Undo maintains data in the user's home directory:
```
~/.undo/
├── config         # Key-value configuration parameters
├── journal.log    # Append-only transactional state log
└── objects/       # Deleted files and directories organized by transaction ID
    ├── a81f92/
    │   └── 0
    └── b6ce44/
        └── 0
```
Deleted items are stored as sequential indices under `objects/<tx_id>/` to prevent name collision or path length issues. Original paths, modes, types, and compression status are tracked in the journal log.

---

## Configuration

Settings are stored in `~/.undo/config`:
```ini
# Prompt user if deleting files/directories larger than this threshold (in bytes)
large_file_threshold = 104857600

# Compression mode: auto (compress if beneficial), on (always), off (never)
compression = auto

# Compress files larger than this threshold (in bytes)
compression_threshold = 1048576
```

---

## Limitations

- **Intercept Boundary**: Only intercepts files/folders deleted via standard `rm` calls. It does not monitor low-level filesystem drivers, system calls, or other utilities (`mv`, `rmdir`, `find -delete`).
- **Disk Allocation**: Backup copies are written to the home folder partition. Adequate free disk space is required on that partition to store deleted objects.
- **Exclusion Filters**: Currently, all files deleted via `rm` are backed up unless bypassed explicitly (e.g. running `\rm` or `/bin/rm`).

---

## Security Philosophy

1. **User Scope isolation**: Undo creates the `~/.undo` directory with permissions `0700` (restricted to the executing user). Deleted files are shielded from other unprivileged local users.
2. **Metadata Preservation**: Original file permissions (owner, group, and mode bits) are recorded in the journal and applied to restored files during recovery.
3. **No Network Access**: Undo does not transmit telemetry, statistics, or file data to any remote servers.

---

## Roadmap

- **Automatic Purging**: Implement auto-clean rules based on storage capacity quotas or transaction ages.
- **Pattern Exclusions**: Support file pattern configurations (e.g., `*.tmp`, `node_modules/`) to skip backup storage and delete permanently.
- **Shell Support**: Add shell script integrations for Fish and PowerShell.

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

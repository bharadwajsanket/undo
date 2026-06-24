# Undo

> Bring Ctrl+Z to terminal file deletions.

Undo is a lightweight, daemonless command-line utility written in C17 that intercepts the standard `rm` command, backs up deleted files locally, and enables instant restoration. It provides a simple, dependency-free recovery mechanism for Unix terminals without background services or bloated databases.

---

## Quick Start

### 1. Installation
Install Undo using `curl` directly from the repository:
```bash
curl -sSL https://raw.githubusercontent.com/bharadwajsanket/undo/main/install.sh | bash
```
*Alternatively, you can compile from source by running `make` and copying the binary to your system PATH.*

### 2. Test Deletion
Once installed, delete any test file:
```bash
rm thesis.pdf
```
*Output:*
```
Stored in Undo
ID: a81f92
```

### 3. Restore Instantly
To revert the deletion and restore the file to its original path:
```bash
undo
```
*Output:*
```
Restored thesis.pdf
```

---

## Screenshots

### Intercept Deletion
![File Deletion](assets/screenshots/delete-file.png)

### Undo Restore
![File Restoration](assets/screenshots/restore-file.png)

### Check Deletion Log
![History Output](assets/screenshots/history.png)

### Storage Efficiency
![Storage Statistics](assets/screenshots/stats.png)

---

## Features

- **Write-Ahead Logging**: Uses a transactional log system (`~/.undo/journal.log`) to guarantee reliability. Data states are fully committed to disk before original files are removed.
- **Crash Rollback**: Startup processes scan the journal to roll back any pending (partially completed) transaction resulting from shell interruptions or power failures.
- **Auto-Compression**: Regular files are automatically deflated on-the-fly using `zlib` when beneficial to optimize disk storage.
- **Overwrite Safeguard**: Prioritizes data safety. Restoration halts immediately if a file already exists at the target path.
- **Zero Daemon Overhead**: Operates purely on command execution and exits immediately. No persistent memory or background CPU footprint.
- **Symbolic Link & Recursion Support**: Supports recursive directory deletions (`rm -r`) and symbolic links.

---

## Usage Examples

### Restore a Specific Deletion
```bash
undo a81f92
```

### Display History Log
```bash
undo history
```

### Show Storage Usage
Shows compression ratios, transaction counts, and configuration settings:
```bash
undo stats
```

### Configure Interactively
```bash
undo config
```

### Purge History
```bash
undo clean
```

---

## Storage & Configuration

Undo stores transaction files and settings locally under `~/.undo/`:
- `config`: Settings file containing threshold preferences.
- `journal.log`: Log file mapping transaction lifecycle states.
- `objects/`: Houses deleted objects organized under their respective transaction IDs.

Customize settings in `~/.undo/config`:
```ini
# Prompt user before storing files larger than this threshold (in bytes)
large_file_threshold = 104857600

# Compression mode: auto, on, off
compression = auto

# Compress files larger than this threshold (in bytes)
compression_threshold = 1048576
```

---

## Limitations

- **rm Only**: Only intercepts files/folders deleted via standard `rm` calls. Low-level driver calls or deletions through other CLI tools (`find -delete`, `rmdir`) are not tracked.
- **Disk Availability**: Backup objects are written to the home folder storage partition. Deletions will fail safely if the home partition runs out of disk space.
- **Bypass**: To delete files permanently without backup, prefix the command with a backslash (`\rm file.txt`) or call the system binary directly (`/bin/rm file.txt`).

---

## Security Philosophy

1. **User Isolation**: The `~/.undo` storage directory is initialized with permission mode `0700` (readable/writable only by the owner).
2. **Permissions Preservation**: File ownership permissions (modes) are preserved in the transaction records and applied back to restored files on recovery.
3. **Local and Private**: Undo operates offline and has no networking code. No telemetry or telemetry logs are ever created or transmitted.

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

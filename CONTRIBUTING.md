# Contributing to Undo

Thank you for your interest in contributing to Undo! We prioritize keeping this tool lightweight, secure, and reliable.

## Development Setup

### Build
Undo requires standard C development tools and `zlib`. To clean and build the local binary:
```bash
make clean
make
```

### Test
Before proposing any code modifications, verify that all test suites pass without warnings:
```bash
# Run unit and integration tests
./tests/test_undo.sh

# Run stress tests
./tests/stress_test.sh
```

Ensure your contributions do not introduce compiler warnings. The project compiles with `-Wall -Wextra` enabled.

---

## Coding Style

1. **Standards**: Conforms to standard C17 (`-std=c17`). Avoid vendor-specific or compiler-specific extensions to preserve cross-platform compatibility across Linux and macOS.
2. **Filesystem Synchronization**: Any write operation updating journal logs or moving transaction objects must be flushed to disk via `fsync()`.
3. **Daemonless Design**: Keep runtime resource utilization low. Do not introduce background processes, threads, file system watchers, or persistent loops. Undo must perform its tasks and exit immediately.
4. **Memory Allocation**: Minimize heap usage. Free allocated structures immediately when no longer needed.

---

## Pull Requests

- **Single Focus**: Keep pull requests focused. Do not mix unrelated formatting, refactoring, and feature changes in the same contribution.
- **Verification**: Include test validations in your PR description. If your changes address a bug, update `tests/test_undo.sh` or `tests/stress_test.sh` to prevent regressions.
- **Safety Checks**: Do not modify transactional commit or rollback behaviors unless correcting a data integrity bug.
- **Documentation**: Adjust `README.md` or structural files if introducing new commands, configurations, or parameters.

# Release Checklist

Use this checklist to verify the repository state before publishing a public release of Undo.

## Final Release Verification Flow

Execute these steps in order to perform a clean validation and publish the release:

### 1. Clean Build
Clear previous object files and compile a clean binary:
- [ ] Run clean and build:
  ```bash
  make clean
  make
  ```

### 2. Run Test Suites
Verify all integration and boundary conditions are satisfied:
- [ ] Run standard tests:
  ```bash
  ./tests/test_undo.sh
  ```
- [ ] Run volume stress tests:
  ```bash
  chmod +x tests/stress_test.sh
  ./tests/stress_test.sh
  ```

### 3. Review Documentation & Versioning
Check version tags and verify formatting:
- [ ] Run version query:
  ```bash
  ./undo --version
  ```
- [ ] Confirm version matches `v0.1.0` in [CHANGELOG.md](CHANGELOG.md).
- [ ] Confirm [README.md](README.md) matches all current options and does not have broken links.
- [ ] Check repository status for untracked garbage files:
  ```bash
  git status
  ```

### 4. Create and Push Release Tag
Commit all finalized documentation, tag the commit, and push:
- [ ] Commit any updates:
  ```bash
  git add .
  git commit -m "Release v0.1.0"
  ```
- [ ] Create annotated git tag:
  ```bash
  git tag -a v0.1.0 -m "Release version 0.1.0"
  ```
- [ ] Push main and tags to remote:
  ```bash
  git push origin main --tags
  ```

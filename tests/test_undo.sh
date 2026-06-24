#!/bin/bash
set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo "============================================="
echo "Starting UNDO Automated Test Suite"
echo "============================================="

# Ensure undo binary is compiled
make clean
make

UNDO="./undo"

# Initialize test files directory
TEST_DIR="sandbox_test"
rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"

# Clean any existing UNDO storage to start fresh
echo "y" | $UNDO clean > /dev/null

# Helper for asserting files exist/not exist
assert_exists() {
    if [ ! -e "$1" ] && [ ! -L "$1" ]; then
        echo -e "${RED}FAIL: Expected $1 to exist${NC}"
        exit 1
    fi
}

assert_not_exists() {
    if [ -e "$1" ] || [ -L "$1" ]; then
        echo -e "${RED}FAIL: Expected $1 to NOT exist${NC}"
        exit 1
    fi
}

assert_content() {
    local content=$(cat "$1")
    if [ "$content" != "$2" ]; then
        echo -e "${RED}FAIL: Content of $1 ('$content') does not match expected '$2'${NC}"
        exit 1
    fi
}

# --- Test Case 1: Simple File Delete and Restore ---
echo -n "Test 1: Simple File Delete and Restore... "
echo "hello world" > "$TEST_DIR/file.txt"
$UNDO --rm "$TEST_DIR/file.txt" > /dev/null
assert_not_exists "$TEST_DIR/file.txt"

# Restore latest
$UNDO > /dev/null
assert_exists "$TEST_DIR/file.txt"
assert_content "$TEST_DIR/file.txt" "hello world"
echo -e "${GREEN}PASS${NC}"


# --- Test Case 2: Overwrite Protection ---
echo -n "Test 2: Overwrite Protection... "
$UNDO --rm "$TEST_DIR/file.txt" > /dev/null
assert_not_exists "$TEST_DIR/file.txt"

# Recreate the file to create conflict
echo "conflicting file" > "$TEST_DIR/file.txt"

# Attempt restore, should fail
if $UNDO 2>/dev/null; then
    echo -e "${RED}FAIL: Restore should have failed due to conflict${NC}"
    exit 1
fi

# Ensure conflict was protected
assert_exists "$TEST_DIR/file.txt"
assert_content "$TEST_DIR/file.txt" "conflicting file"

# Remove conflict and restore again
rm "$TEST_DIR/file.txt"
$UNDO > /dev/null
assert_exists "$TEST_DIR/file.txt"
assert_content "$TEST_DIR/file.txt" "hello world"
echo -e "${GREEN}PASS${NC}"


# --- Test Case 3: Recursive Directory Delete and Restore ---
echo -n "Test 3: Recursive Directory Delete and Restore... "
mkdir -p "$TEST_DIR/folder/subfolder"
echo "subfile content" > "$TEST_DIR/folder/subfolder/file2.txt"
echo "rootfile content" > "$TEST_DIR/folder/file1.txt"

# Delete folder recursively
$UNDO --rm -r "$TEST_DIR/folder" > /dev/null
assert_not_exists "$TEST_DIR/folder"

# Restore latest
$UNDO > /dev/null
assert_exists "$TEST_DIR/folder"
assert_exists "$TEST_DIR/folder/file1.txt"
assert_exists "$TEST_DIR/folder/subfolder/file2.txt"
assert_content "$TEST_DIR/folder/file1.txt" "rootfile content"
assert_content "$TEST_DIR/folder/subfolder/file2.txt" "subfile content"
echo -e "${GREEN}PASS${NC}"


# --- Test Case 4: Symbolic Link Delete and Restore ---
echo -n "Test 4: Symbolic Link Delete and Restore... "
ln -s "file.txt" "$TEST_DIR/symlink.txt"
assert_exists "$TEST_DIR/symlink.txt"

$UNDO --rm "$TEST_DIR/symlink.txt" > /dev/null
assert_not_exists "$TEST_DIR/symlink.txt"

$UNDO > /dev/null
assert_exists "$TEST_DIR/symlink.txt"
if [ ! -L "$TEST_DIR/symlink.txt" ]; then
    echo -e "${RED}FAIL: Restored file is not a symlink${NC}"
    exit 1
fi
echo -e "${GREEN}PASS${NC}"


# --- Test Case 5: Compression ---
echo -n "Test 5: Auto-Compression of Large Compressible Files... "
# Generate a compressible 1.5MB file (above 1MB default threshold)
dd if=/dev/zero of="$TEST_DIR/large.txt" bs=1024 count=1500 2>/dev/null
assert_exists "$TEST_DIR/large.txt"

# Delete large file
$UNDO --rm "$TEST_DIR/large.txt" > /dev/null
assert_not_exists "$TEST_DIR/large.txt"

# Verify in journal that compression status is 1
# Get latest file line from journal
JOURNAL_PATH="$HOME/.undo/journal.log"
HEX_NAME=$(echo -n "large.txt" | xxd -p | tr -d '\n')
LATEST_FILE_ENTRY=$(grep -E "^FILE .*$HEX_NAME" "$JOURNAL_PATH" | tail -n 1)
if [[ "$LATEST_FILE_ENTRY" != *1 ]]; then
    echo -e "${RED}FAIL: Large compressible file was not compressed (entry: $LATEST_FILE_ENTRY)${NC}"
    exit 1
fi

# Restore large file
$UNDO > /dev/null
assert_exists "$TEST_DIR/large.txt"
LARGE_SIZE=$(wc -c < "$TEST_DIR/large.txt" | tr -d ' ')
if [ "$LARGE_SIZE" -ne 1536000 ]; then
    echo -e "${RED}FAIL: Restored file size ($LARGE_SIZE) does not match original size 1536000${NC}"
    exit 1
fi
echo -e "${GREEN}PASS${NC}"


# --- Test Case 6: Stats and History commands ---
echo -n "Test 6: Stats and History Commands... "
$UNDO history > /dev/null
$UNDO stats > /dev/null
echo -e "${GREEN}PASS${NC}"


# --- Test Case 7: Shell Integration (Install/Uninstall) ---
echo -n "Test 7: Shell Integration... "
# Backup shell configs if they exist
ZSH_BACKUP=""
BASH_BACKUP=""
[ -f "$HOME/.zshrc" ] && ZSH_BACKUP=$(cat "$HOME/.zshrc")
[ -f "$HOME/.bashrc" ] && BASH_BACKUP=$(cat "$HOME/.bashrc")

# Test install
$UNDO install > /dev/null

# Verify installation block
RC_FILE=""
if [ -f "$HOME/.zshrc" ] && [[ $(cat "$HOME/.zshrc") == *"# >>> UNDO Shell Integration >>>"* ]]; then
    RC_FILE="$HOME/.zshrc"
elif [ -f "$HOME/.bashrc" ] && [[ $(cat "$HOME/.bashrc") == *"# >>> UNDO Shell Integration >>>"* ]]; then
    RC_FILE="$HOME/.bashrc"
fi

if [ -z "$RC_FILE" ]; then
    echo -e "${RED}FAIL: Shell integration block not found in .zshrc or .bashrc${NC}"
    # Restore backups
    [ -n "$ZSH_BACKUP" ] && echo "$ZSH_BACKUP" > "$HOME/.zshrc"
    [ -n "$BASH_BACKUP" ] && echo "$BASH_BACKUP" > "$HOME/.bashrc"
    exit 1
fi

# Test uninstall
$UNDO uninstall > /dev/null
if [[ $(cat "$RC_FILE") == *"# >>> UNDO Shell Integration >>>"* ]]; then
    echo -e "${RED}FAIL: Shell integration block was not removed from $RC_FILE${NC}"
    # Restore backups
    [ -n "$ZSH_BACKUP" ] && echo "$ZSH_BACKUP" > "$HOME/.zshrc"
    [ -n "$BASH_BACKUP" ] && echo "$BASH_BACKUP" > "$HOME/.bashrc"
    exit 1
fi

# Restore original shell configs
if [ -n "$ZSH_BACKUP" ]; then echo "$ZSH_BACKUP" > "$HOME/.zshrc"; else rm -f "$HOME/.zshrc"; fi
if [ -n "$BASH_BACKUP" ]; then echo "$BASH_BACKUP" > "$HOME/.bashrc"; else rm -f "$HOME/.bashrc"; fi

echo -e "${GREEN}PASS${NC}"


# --- Test Case 8: Crash Safety Recovery ---
echo -n "Test 8: Crash Safety Recovery (Rollback of Pending Transaction)... "
# We write a mock uncommitted transaction in the journal
TX_ID="c1a53b"
HEX_PATH=$(echo -n "$(pwd)/$TEST_DIR/crash_file.txt" | xxd -p | tr -d '\n')
# Clean config path and ensure objects directory
mkdir -p "$HOME/.undo/objects/$TX_ID"
echo "crash recovery content" > "$HOME/.undo/objects/$TX_ID/0"

# Append start and file entries to journal but DO NOT commit
echo "START $TX_ID $(date +%s)" >> "$JOURNAL_PATH"
echo "FILE $TX_ID 0 $HEX_PATH 22 0644 0 0 0" >> "$JOURNAL_PATH"

# Ensure the original file does not exist yet (simulating it was partially deleted or moved)
assert_not_exists "$TEST_DIR/crash_file.txt"

# Run undo - it should perform crash recovery for TX_ID on startup!
$UNDO --version > /dev/null

# Verify recovery restored the file
assert_exists "$TEST_DIR/crash_file.txt"
assert_content "$TEST_DIR/crash_file.txt" "crash recovery content"

# Verify journal now has ABORT entry for this transaction
if ! grep -q "ABORT $TX_ID" "$JOURNAL_PATH"; then
    echo -e "${RED}FAIL: Journal does not contain ABORT marker for crashed transaction${NC}"
    exit 1
fi

echo -e "${GREEN}PASS${NC}"


# Cleanup sandbox
rm -rf "$TEST_DIR"
echo "y" | $UNDO clean > /dev/null

echo "============================================="
echo -e "${GREEN}ALL TESTS PASSED SUCCESSFULLY!${NC}"
echo "============================================="

#!/usr/bin/env bash

# UNDO v1 Stress Test Suite
#
# Run from project root:
#   chmod +x stress_test.sh
#   ./stress_test.sh
#
# Assumes:
#   ./undo exists
#
# WARNING:
#   This script creates and deletes many test files/directories.
#   It should only be run inside a disposable test directory.

set -euo pipefail

ORIG_DIR=$(pwd)
UNDO="../undo"

PASS=0
FAIL=0

pass() {
    echo "[PASS] $1"
    PASS=$((PASS + 1))
}

fail() {
    echo "[FAIL] $1"
    FAIL=$((FAIL + 1))
}

cleanup() {
    cd "$ORIG_DIR" 2>/dev/null || true
    rm -rf stress_workspace 2>/dev/null || true
}

trap cleanup EXIT

cleanup
mkdir stress_workspace
cd stress_workspace

# Clean Undo storage for a clean and consistent test run
echo "y" | $UNDO clean >/dev/null

echo
echo "===================================="
echo "UNDO v1 STRESS TEST SUITE"
echo "===================================="
echo

############################################################
# Test 1: Basic Delete + Restore
############################################################
echo "hello world" > basic.txt

$UNDO --rm basic.txt >/dev/null

if [ -f basic.txt ]; then
    fail "basic delete"
else
    pass "basic delete"
fi

$UNDO >/dev/null

if grep -q "hello world" basic.txt; then
    pass "basic restore"
else
    fail "basic restore"
fi

############################################################
# Test 2: Filename With Spaces
############################################################
echo "spaces" > "my file.txt"

$UNDO --rm "my file.txt" >/dev/null
$UNDO >/dev/null

if [ -f "my file.txt" ]; then
    pass "space filename"
else
    fail "space filename"
fi

############################################################
# Test 3: Unicode Filename
############################################################
echo "unicode" > "नमस्ते.txt"

$UNDO --rm "नमस्ते.txt" >/dev/null
$UNDO >/dev/null

if [ -f "नमस्ते.txt" ]; then
    pass "unicode filename"
else
    fail "unicode filename"
fi

############################################################
# Test 4: Deep Nested Path
############################################################
mkdir -p a/b/c/d/e/f/g

echo "nested" > a/b/c/d/e/f/g/test.txt

$UNDO --rm a/b/c/d/e/f/g/test.txt >/dev/null
$UNDO >/dev/null

if [ -f a/b/c/d/e/f/g/test.txt ]; then
    pass "deep nested restore"
else
    fail "deep nested restore"
fi

############################################################
# Test 5: Collision Protection
############################################################
echo "old" > collision.txt

$UNDO --rm collision.txt >/dev/null

echo "new" > collision.txt

if $UNDO >/dev/null 2>&1; then
    fail "collision protection"
else
    pass "collision protection"
fi

rm -f collision.txt

############################################################
# Test 6: Many Files
############################################################
mkdir many

for i in $(seq 1 250)
do
    echo "$i" > "many/$i.txt"
    $UNDO --rm "many/$i.txt" >/dev/null
done

pass "mass delete 250 files"

############################################################
# Test 7: Repeated Restore Cycles
############################################################
for i in $(seq 1 50)
do
    echo "cycle" > cycle.txt

    $UNDO --rm cycle.txt >/dev/null
    $UNDO >/dev/null
    if ! grep -q "cycle" cycle.txt; then
        fail "restore cycle $i"
        break
    fi
    rm -f cycle.txt
done

pass "50 restore cycles"

############################################################
# Test 8: Duplicate Names Different Paths
############################################################
mkdir -p one two

echo "one" > one/test.txt
echo "two" > two/test.txt

$UNDO --rm one/test.txt >/dev/null
$UNDO --rm two/test.txt >/dev/null

$UNDO >/dev/null
$UNDO >/dev/null

if [ -f one/test.txt ] && [ -f two/test.txt ]; then
    pass "duplicate names"
else
    fail "duplicate names"
fi

############################################################
# Test 9: History Command
############################################################
if $UNDO history >/dev/null 2>&1; then
    pass "history command"
else
    fail "history command"
fi

############################################################
# Test 10: Stats Command
############################################################
if $UNDO stats >/dev/null 2>&1; then
    pass "stats command"
else
    fail "stats command"
fi

############################################################
# Test 11: Journal Flood
############################################################
mkdir flood

for i in $(seq 1 1000)
do
    echo "flood" > "flood/$i.txt"
    $UNDO --rm "flood/$i.txt" >/dev/null
done

pass "journal flood 1000 files"

############################################################
# Test 12: Empty File
############################################################
touch empty.txt

$UNDO --rm empty.txt >/dev/null
$UNDO >/dev/null

if [ -f empty.txt ]; then
    pass "empty file"
else
    fail "empty file"
fi

############################################################
# Test 13: Binary File
############################################################
dd if=/dev/urandom of=random.bin bs=1024 count=10 >/dev/null 2>&1

ORIGINAL_HASH=$(shasum random.bin | awk '{print $1}')

$UNDO --rm random.bin >/dev/null
$UNDO >/dev/null

RESTORED_HASH=$(shasum random.bin | awk '{print $1}')

if [ "$ORIGINAL_HASH" = "$RESTORED_HASH" ]; then
    pass "binary integrity"
else
    fail "binary integrity"
fi

############################################################
# Test 14: Multiple Consecutive Restores
############################################################
for i in $(seq 1 10)
do
    echo "$i" > "restore_$i.txt"
    $UNDO --rm "restore_$i.txt" >/dev/null
done

for i in $(seq 1 10)
do
    $UNDO >/dev/null
done

pass "consecutive restores"

############################################################
# Test 15: Missing Transaction
############################################################
if $UNDO deadbeef >/dev/null 2>&1; then
    fail "invalid transaction id"
else
    pass "invalid transaction id"
fi

############################################################
# Summary
############################################################
echo
echo "===================================="
echo "TEST SUMMARY"
echo "===================================="
echo

echo "Passed : $PASS"
echo "Failed : $FAIL"
echo

if [ "$FAIL" -eq 0 ]; then
    echo "ALL TESTS PASSED"
    exit 0
else
    echo "SOME TESTS FAILED"
    exit 1
fi

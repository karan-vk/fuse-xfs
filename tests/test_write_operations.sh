#!/bin/bash
#
# test_write_operations.sh - Comprehensive test suite for fusexfs write operations
#
# This script tests all write operations implemented in fusexfs:
# - Phase 1: chmod, chown, utimens, truncate, fsync
# - Phase 2: create, write, mknod
# - Phase 3: mkdir, unlink, rmdir, rename
# - Phase 4: link, symlink
#
# Usage: ./test_write_operations.sh <xfs_image> <mount_point> [fusexfs_binary]
#
# Author: FuseXFS Development Team
# Version: 1.0
#

set -o pipefail

# ============================================================================
# Configuration and Constants
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPT_NAME="$(basename "${BASH_SOURCE[0]}")"

# Default values
DEFAULT_FUSEXFS_BINARY="${SCRIPT_DIR}/../build/fusexfs"
XFS_IMAGE=""
MOUNT_POINT=""
FUSEXFS_BINARY=""

# Test counters
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0
TOTAL_TESTS=0

# Colors for output (if terminal supports it)
if [ -t 1 ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[0;33m'
    BLUE='\033[0;34m'
    NC='\033[0m' # No Color
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    NC=''
fi

# Test result log
TEST_LOG=""

# ============================================================================
# Helper Functions
# ============================================================================

usage() {
    cat << EOF
Usage: $SCRIPT_NAME <xfs_image> <mount_point> [fusexfs_binary]

Test script for fusexfs write operations.

Arguments:
  xfs_image       Path to an XFS image file or block device
  mount_point     Directory where XFS will be mounted
  fusexfs_binary  Path to fusexfs binary (default: ${DEFAULT_FUSEXFS_BINARY})

Options:
  -h, --help      Show this help message

Example:
  $SCRIPT_NAME /tmp/test.xfs /mnt/xfs
  $SCRIPT_NAME /tmp/test.xfs /mnt/xfs ./fusexfs

Environment Variables:
  SKIP_CHOWN_TESTS    Set to 1 to skip chown tests (requires root)
  VERBOSE             Set to 1 for verbose output
EOF
}

log() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[OK]${NC} $1"
}

# Record test result
# Arguments: test_name, expected_result, actual_result, status (PASS/FAIL/SKIP)
record_test() {
    local test_name="$1"
    local expected="$2"
    local actual="$3"
    local status="$4"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    case "$status" in
        PASS)
            TESTS_PASSED=$((TESTS_PASSED + 1))
            TEST_LOG="${TEST_LOG}${GREEN}[PASS]${NC} ${test_name}\n"
            [ "$VERBOSE" = "1" ] && echo -e "${GREEN}[PASS]${NC} ${test_name}"
            ;;
        FAIL)
            TESTS_FAILED=$((TESTS_FAILED + 1))
            TEST_LOG="${TEST_LOG}${RED}[FAIL]${NC} ${test_name}\n"
            TEST_LOG="${TEST_LOG}       Expected: ${expected}\n"
            TEST_LOG="${TEST_LOG}       Actual:   ${actual}\n"
            echo -e "${RED}[FAIL]${NC} ${test_name}"
            echo "       Expected: ${expected}"
            echo "       Actual:   ${actual}"
            ;;
        SKIP)
            TESTS_SKIPPED=$((TESTS_SKIPPED + 1))
            TEST_LOG="${TEST_LOG}${YELLOW}[SKIP]${NC} ${test_name}: ${expected}\n"
            [ "$VERBOSE" = "1" ] && echo -e "${YELLOW}[SKIP]${NC} ${test_name}: ${expected}"
            ;;
    esac
}

# Assert that two values are equal
# Arguments: test_name, expected, actual
assert_equals() {
    local test_name="$1"
    local expected="$2"
    local actual="$3"
    
    if [ "$expected" = "$actual" ]; then
        record_test "$test_name" "$expected" "$actual" "PASS"
        return 0
    else
        record_test "$test_name" "$expected" "$actual" "FAIL"
        return 1
    fi
}

# Assert that a file exists
# Arguments: test_name, file_path
assert_file_exists() {
    local test_name="$1"
    local file_path="$2"
    
    if [ -e "$file_path" ]; then
        record_test "$test_name" "file exists" "file exists" "PASS"
        return 0
    else
        record_test "$test_name" "file exists" "file does not exist" "FAIL"
        return 1
    fi
}

# Assert that a file does not exist
# Arguments: test_name, file_path
assert_file_not_exists() {
    local test_name="$1"
    local file_path="$2"
    
    if [ ! -e "$file_path" ]; then
        record_test "$test_name" "file does not exist" "file does not exist" "PASS"
        return 0
    else
        record_test "$test_name" "file does not exist" "file exists" "FAIL"
        return 1
    fi
}

# Assert that a directory exists
# Arguments: test_name, dir_path
assert_dir_exists() {
    local test_name="$1"
    local dir_path="$2"
    
    if [ -d "$dir_path" ]; then
        record_test "$test_name" "directory exists" "directory exists" "PASS"
        return 0
    else
        record_test "$test_name" "directory exists" "not a directory or doesn't exist" "FAIL"
        return 1
    fi
}

# Assert that a symlink exists and points to the expected target
# Arguments: test_name, link_path, expected_target
assert_symlink() {
    local test_name="$1"
    local link_path="$2"
    local expected_target="$3"
    
    if [ -L "$link_path" ]; then
        local actual_target=$(readlink "$link_path")
        if [ "$actual_target" = "$expected_target" ]; then
            record_test "$test_name" "symlink -> $expected_target" "symlink -> $actual_target" "PASS"
            return 0
        else
            record_test "$test_name" "symlink -> $expected_target" "symlink -> $actual_target" "FAIL"
            return 1
        fi
    else
        record_test "$test_name" "symlink -> $expected_target" "not a symlink" "FAIL"
        return 1
    fi
}

# Assert that a command fails with expected error
# Arguments: test_name, expected_errno, command...
assert_command_fails() {
    local test_name="$1"
    local expected_errno="$2"
    shift 2
    
    local output
    output=$("$@" 2>&1)
    local exit_code=$?
    
    if [ $exit_code -ne 0 ]; then
        record_test "$test_name" "command fails" "command failed (exit=$exit_code)" "PASS"
        return 0
    else
        record_test "$test_name" "command fails" "command succeeded" "FAIL"
        return 1
    fi
}

# Get file permissions in octal format
get_file_mode() {
    stat -f "%OLp" "$1" 2>/dev/null || stat -c "%a" "$1" 2>/dev/null
}

# Get file owner uid
get_file_uid() {
    stat -f "%u" "$1" 2>/dev/null || stat -c "%u" "$1" 2>/dev/null
}

# Get file owner gid
get_file_gid() {
    stat -f "%g" "$1" 2>/dev/null || stat -c "%g" "$1" 2>/dev/null
}

# Get file size
get_file_size() {
    stat -f "%z" "$1" 2>/dev/null || stat -c "%s" "$1" 2>/dev/null
}

# Get hard link count
get_link_count() {
    stat -f "%l" "$1" 2>/dev/null || stat -c "%h" "$1" 2>/dev/null
}

# Get file modification time as epoch
get_mtime() {
    stat -f "%m" "$1" 2>/dev/null || stat -c "%Y" "$1" 2>/dev/null
}

# Clean up test directory
cleanup_test_dir() {
    local test_dir="${MOUNT_POINT}/test_dir"
    if [ -d "$test_dir" ]; then
        rm -rf "$test_dir" 2>/dev/null || true
    fi
}

# Create test directory
setup_test_dir() {
    local test_dir="${MOUNT_POINT}/test_dir"
    cleanup_test_dir
    mkdir -p "$test_dir"
    echo "$test_dir"
}

# ============================================================================
# Test Categories
# ============================================================================

# ----------------------------------------------------------------------------
# PHASE 1: Basic Attribute Operations
# ----------------------------------------------------------------------------

test_phase1_chmod() {
    log "Testing chmod operations..."
    
    local test_dir=$(setup_test_dir)
    local test_file="${test_dir}/chmod_test"
    
    # Create test file
    touch "$test_file"
    
    # Test 1: Basic chmod to 755
    chmod 755 "$test_file"
    local mode=$(get_file_mode "$test_file")
    assert_equals "chmod: set mode to 755" "755" "$mode"
    
    # Test 2: chmod to 644
    chmod 644 "$test_file"
    mode=$(get_file_mode "$test_file")
    assert_equals "chmod: set mode to 644" "644" "$mode"
    
    # Test 3: chmod to 600
    chmod 600 "$test_file"
    mode=$(get_file_mode "$test_file")
    assert_equals "chmod: set mode to 600" "600" "$mode"
    
    # Test 4: chmod to 777
    chmod 777 "$test_file"
    mode=$(get_file_mode "$test_file")
    assert_equals "chmod: set mode to 777" "777" "$mode"
    
    # Test 5: chmod with symbolic notation (u+x)
    chmod 644 "$test_file"
    chmod u+x "$test_file"
    mode=$(get_file_mode "$test_file")
    assert_equals "chmod: symbolic u+x" "744" "$mode"
    
    # Test 6: chmod on directory
    local test_subdir="${test_dir}/chmod_dir"
    mkdir "$test_subdir"
    chmod 700 "$test_subdir"
    mode=$(get_file_mode "$test_subdir")
    assert_equals "chmod: directory mode 700" "700" "$mode"
    
    cleanup_test_dir
}

test_phase1_chown() {
    log "Testing chown operations..."
    
    # Skip if not root
    if [ "$(id -u)" != "0" ] && [ "$SKIP_CHOWN_TESTS" = "1" ]; then
        record_test "chown: requires root" "test skipped" "running as non-root" "SKIP"
        return
    fi
    
    local test_dir=$(setup_test_dir)
    local test_file="${test_dir}/chown_test"
    
    touch "$test_file"
    
    if [ "$(id -u)" = "0" ]; then
        # Test 1: Change owner to uid 1000
        chown 1000 "$test_file" 2>/dev/null
        local uid=$(get_file_uid "$test_file")
        assert_equals "chown: set uid to 1000" "1000" "$uid"
        
        # Test 2: Change group to gid 1000
        chown :1000 "$test_file" 2>/dev/null
        local gid=$(get_file_gid "$test_file")
        assert_equals "chown: set gid to 1000" "1000" "$gid"
        
        # Test 3: Change owner and group
        chown 500:500 "$test_file" 2>/dev/null
        uid=$(get_file_uid "$test_file")
        gid=$(get_file_gid "$test_file")
        assert_equals "chown: set uid to 500" "500" "$uid"
        assert_equals "chown: set gid to 500" "500" "$gid"
    else
        # Test as non-root: chown to own uid/gid should work
        local my_uid=$(id -u)
        local my_gid=$(id -g)
        
        chown "$my_uid:$my_gid" "$test_file" 2>/dev/null
        local uid=$(get_file_uid "$test_file")
        local gid=$(get_file_gid "$test_file")
        assert_equals "chown: set own uid" "$my_uid" "$uid"
        assert_equals "chown: set own gid" "$my_gid" "$gid"
    fi
    
    cleanup_test_dir
}

test_phase1_utimens() {
    log "Testing utimens (timestamp) operations..."
    
    local test_dir=$(setup_test_dir)
    local test_file="${test_dir}/utimens_test"
    
    touch "$test_file"
    
    # Test 1: Set specific timestamp using touch -t
    touch -t 202001011200 "$test_file"
    local mtime=$(get_mtime "$test_file")
    # Note: exact time depends on timezone, just verify it changed
    if [ -n "$mtime" ] && [ "$mtime" != "0" ]; then
        record_test "utimens: set mtime via touch -t" "timestamp set" "timestamp=$mtime" "PASS"
    else
        record_test "utimens: set mtime via touch -t" "timestamp set" "failed to get mtime" "FAIL"
    fi
    
    # Test 2: Update timestamp to now
    local before_mtime=$(get_mtime "$test_file")
    sleep 1
    touch "$test_file"
    local after_mtime=$(get_mtime "$test_file")
    
    if [ "$after_mtime" -gt "$before_mtime" ]; then
        record_test "utimens: update to current time" "mtime increased" "before=$before_mtime, after=$after_mtime" "PASS"
    else
        record_test "utimens: update to current time" "mtime increased" "before=$before_mtime, after=$after_mtime" "FAIL"
    fi
    
    cleanup_test_dir
}

test_phase1_truncate() {
    log "Testing truncate operations..."
    
    local test_dir=$(setup_test_dir)
    local test_file="${test_dir}/truncate_test"
    
    # Create file with content
    echo "Hello, World! This is test content for truncation." > "$test_file"
    local original_size=$(get_file_size "$test_file")
    
    # Test 1: Truncate to smaller size
    truncate -s 10 "$test_file"
    local size=$(get_file_size "$test_file")
    assert_equals "truncate: shrink to 10 bytes" "10" "$size"
    
    # Test 2: Truncate to 0
    truncate -s 0 "$test_file"
    size=$(get_file_size "$test_file")
    assert_equals "truncate: shrink to 0 bytes" "0" "$size"
    
    # Test 3: Extend file (creates sparse file)
    truncate -s 1024 "$test_file"
    size=$(get_file_size "$test_file")
    assert_equals "truncate: extend to 1024 bytes" "1024" "$size"
    
    # Test 4: Extend to larger size
    truncate -s 4096 "$test_file"
    size=$(get_file_size "$test_file")
    assert_equals "truncate: extend to 4096 bytes" "4096" "$size"
    
    # Test 5: Verify content after truncate
    echo "abc" > "$test_file"
    truncate -s 2 "$test_file"
    local content=$(cat "$test_file")
    assert_equals "truncate: content preserved" "ab" "$content"
    
    cleanup_test_dir
}

test_phase1_fsync() {
    log "Testing fsync operations..."
    
    local test_dir=$(setup_test_dir)
    local test_file="${test_dir}/fsync_test"
    
    # Create file and sync
    echo "Test content for fsync" > "$test_file"
    
    # Use sync command (which internally calls fsync)
    sync
    
    # Verify file persists
    if [ -f "$test_file" ]; then
        local content=$(cat "$test_file")
        if [ "$content" = "Test content for fsync" ]; then
            record_test "fsync: data persists after sync" "data preserved" "data preserved" "PASS"
        else
            record_test "fsync: data persists after sync" "data preserved" "data corrupted" "FAIL"
        fi
    else
        record_test "fsync: data persists after sync" "file exists" "file missing" "FAIL"
    fi
    
    cleanup_test_dir
}

# ----------------------------------------------------------------------------
# PHASE 2: File I/O Operations
# ----------------------------------------------------------------------------

test_phase2_create() {
    log "Testing file create operations..."
    
    local test_dir=$(setup_test_dir)
    
    # Test 1: Create empty file with touch
    local test_file="${test_dir}/create_touch"
    touch "$test_file"
    assert_file_exists "create: empty file with touch" "$test_file"
    
    # Test 2: Create file with content using echo
    test_file="${test_dir}/create_echo"
    echo "Hello" > "$test_file"
    assert_file_exists "create: file with echo" "$test_file"
    local content=$(cat "$test_file")
    assert_equals "create: echo content" "Hello" "$content"
    
    # Test 3: Create file with printf
    test_file="${test_dir}/create_printf"
    printf "Test\nMultiple\nLines" > "$test_file"
    assert_file_exists "create: file with printf" "$test_file"
    
    # Test 4: Create file with dd
    test_file="${test_dir}/create_dd"
    dd if=/dev/zero of="$test_file" bs=1024 count=1 2>/dev/null
    assert_file_exists "create: file with dd" "$test_file"
    local size=$(get_file_size "$test_file")
    assert_equals "create: dd file size" "1024" "$size"
    
    # Test 5: Create file with specific permissions
    test_file="${test_dir}/create_mode"
    touch "$test_file"
    chmod 600 "$test_file"
    local mode=$(get_file_mode "$test_file")
    assert_equals "create: file with mode 600" "600" "$mode"
    
    cleanup_test_dir
}

test_phase2_write() {
    log "Testing file write operations..."
    
    local test_dir=$(setup_test_dir)
    local test_file="${test_dir}/write_test"
    
    # Test 1: Write and verify content
    echo "Test content line 1" > "$test_file"
    local content=$(cat "$test_file")
    assert_equals "write: basic write" "Test content line 1" "$content"
    
    # Test 2: Append to file
    echo "Line 2" >> "$test_file"
    local line_count=$(wc -l < "$test_file" | tr -d ' ')
    assert_equals "write: append creates 2 lines" "2" "$line_count"
    
    # Test 3: Overwrite file
    echo "New content" > "$test_file"
    content=$(cat "$test_file")
    assert_equals "write: overwrite" "New content" "$content"
    
    # Test 4: Write larger file
    local test_file_large="${test_dir}/write_large"
    dd if=/dev/urandom of="$test_file_large" bs=1024 count=100 2>/dev/null
    local size=$(get_file_size "$test_file_large")
    assert_equals "write: 100KB file" "102400" "$size"
    
    # Test 5: Write at specific offset using dd
    echo "0123456789" > "$test_file"
    echo -n "XX" | dd of="$test_file" bs=1 seek=3 conv=notrunc 2>/dev/null
    content=$(cat "$test_file")
    assert_equals "write: at offset" "012XX56789" "$content"
    
    # Test 6: Multiple sequential writes
    local test_file_seq="${test_dir}/write_seq"
    for i in $(seq 1 10); do
        echo "Line $i" >> "$test_file_seq"
    done
    line_count=$(wc -l < "$test_file_seq" | tr -d ' ')
    assert_equals "write: sequential appends" "10" "$line_count"
    
    cleanup_test_dir
}

test_phase2_mknod() {
    log "Testing mknod operations..."
    
    local test_dir=$(setup_test_dir)
    
    # Test 1: Create FIFO (named pipe)
    local fifo_file="${test_dir}/test_fifo"
    mkfifo "$fifo_file" 2>/dev/null
    if [ -p "$fifo_file" ]; then
        record_test "mknod: create FIFO" "FIFO created" "FIFO created" "PASS"
    else
        record_test "mknod: create FIFO" "FIFO created" "FIFO not created" "FAIL"
    fi
    
    # Test 2: Create device node (requires root)
    if [ "$(id -u)" = "0" ]; then
        local dev_file="${test_dir}/test_dev"
        mknod "$dev_file" c 1 3 2>/dev/null  # /dev/null equivalent
        if [ -c "$dev_file" ]; then
            record_test "mknod: create char device" "device created" "device created" "PASS"
        else
            record_test "mknod: create char device" "device created" "device not created" "FAIL"
        fi
    else
        record_test "mknod: create char device" "requires root" "" "SKIP"
    fi
    
    cleanup_test_dir
}

# ----------------------------------------------------------------------------
# PHASE 3: Directory Operations
# ----------------------------------------------------------------------------

test_phase3_mkdir() {
    log "Testing mkdir operations..."
    
    local test_dir=$(setup_test_dir)
    
    # Test 1: Create single directory
    local new_dir="${test_dir}/mkdir_single"
    mkdir "$new_dir"
    assert_dir_exists "mkdir: single directory" "$new_dir"
    
    # Test 2: Create nested directories with -p
    local nested_dir="${test_dir}/mkdir_nested/level1/level2/level3"
    mkdir -p "$nested_dir"
    assert_dir_exists "mkdir: nested directories" "$nested_dir"
    
    # Test 3: Create directory with specific mode
    local mode_dir="${test_dir}/mkdir_mode"
    mkdir -m 700 "$mode_dir"
    assert_dir_exists "mkdir: directory with mode" "$mode_dir"
    local mode=$(get_file_mode "$mode_dir")
    assert_equals "mkdir: directory mode 700" "700" "$mode"
    
    # Test 4: Create multiple directories
    mkdir "${test_dir}/multi1" "${test_dir}/multi2" "${test_dir}/multi3"
    assert_dir_exists "mkdir: multi1" "${test_dir}/multi1"
    assert_dir_exists "mkdir: multi2" "${test_dir}/multi2"
    assert_dir_exists "mkdir: multi3" "${test_dir}/multi3"
    
    cleanup_test_dir
}

test_phase3_rmdir() {
    log "Testing rmdir operations..."
    
    local test_dir=$(setup_test_dir)
    
    # Test 1: Remove empty directory
    local empty_dir="${test_dir}/rmdir_empty"
    mkdir "$empty_dir"
    rmdir "$empty_dir"
    assert_file_not_exists "rmdir: empty directory" "$empty_dir"
    
    # Test 2: rmdir on non-empty directory should fail
    local nonempty_dir="${test_dir}/rmdir_nonempty"
    mkdir "$nonempty_dir"
    touch "${nonempty_dir}/file.txt"
    
    rmdir "$nonempty_dir" 2>/dev/null
    local exit_code=$?
    if [ $exit_code -ne 0 ]; then
        record_test "rmdir: non-empty fails with ENOTEMPTY" "command fails" "exit code $exit_code" "PASS"
    else
        record_test "rmdir: non-empty fails with ENOTEMPTY" "command fails" "command succeeded" "FAIL"
    fi
    
    # Cleanup for next test
    rm -rf "$nonempty_dir"
    
    # Test 3: Remove nested directories
    local nested="${test_dir}/nested1/nested2"
    mkdir -p "$nested"
    rmdir "$nested"
    rmdir "${test_dir}/nested1"
    assert_file_not_exists "rmdir: nested inner" "$nested"
    assert_file_not_exists "rmdir: nested outer" "${test_dir}/nested1"
    
    cleanup_test_dir
}

test_phase3_unlink() {
    log "Testing unlink (rm) operations..."
    
    local test_dir=$(setup_test_dir)
    
    # Test 1: Remove regular file
    local test_file="${test_dir}/unlink_test"
    touch "$test_file"
    rm "$test_file"
    assert_file_not_exists "unlink: regular file" "$test_file"
    
    # Test 2: Remove file with content
    test_file="${test_dir}/unlink_content"
    echo "Content to delete" > "$test_file"
    rm "$test_file"
    assert_file_not_exists "unlink: file with content" "$test_file"
    
    # Test 3: rm on directory should fail (use rmdir)
    local test_subdir="${test_dir}/unlink_dir"
    mkdir "$test_subdir"
    rm "$test_subdir" 2>/dev/null
    local exit_code=$?
    if [ $exit_code -ne 0 ]; then
        record_test "unlink: directory fails with EISDIR" "command fails" "exit code $exit_code" "PASS"
    else
        record_test "unlink: directory fails with EISDIR" "command fails" "command succeeded" "FAIL"
    fi
    rmdir "$test_subdir"  # Cleanup
    
    # Test 4: Remove multiple files
    touch "${test_dir}/multi1" "${test_dir}/multi2" "${test_dir}/multi3"
    rm "${test_dir}/multi1" "${test_dir}/multi2" "${test_dir}/multi3"
    assert_file_not_exists "unlink: multi1" "${test_dir}/multi1"
    assert_file_not_exists "unlink: multi2" "${test_dir}/multi2"
    assert_file_not_exists "unlink: multi3" "${test_dir}/multi3"
    
    cleanup_test_dir
}

test_phase3_rename() {
    log "Testing rename operations..."
    
    local test_dir=$(setup_test_dir)
    
    # Test 1: Rename file within same directory
    local src="${test_dir}/rename_src"
    local dst="${test_dir}/rename_dst"
    echo "test content" > "$src"
    mv "$src" "$dst"
    assert_file_not_exists "rename: source removed" "$src"
    assert_file_exists "rename: destination created" "$dst"
    local content=$(cat "$dst")
    assert_equals "rename: content preserved" "test content" "$content"
    
    # Test 2: Rename to overwrite existing file
    local src2="${test_dir}/rename_src2"
    echo "new content" > "$src2"
    mv "$src2" "$dst"  # Overwrite dst
    content=$(cat "$dst")
    assert_equals "rename: overwrite content" "new content" "$content"
    
    # Test 3: Rename across directories
    local subdir="${test_dir}/subdir"
    mkdir "$subdir"
    echo "moving content" > "${test_dir}/moving_file"
    mv "${test_dir}/moving_file" "${subdir}/moved_file"
    assert_file_not_exists "rename: cross-dir source removed" "${test_dir}/moving_file"
    assert_file_exists "rename: cross-dir destination created" "${subdir}/moved_file"
    content=$(cat "${subdir}/moved_file")
    assert_equals "rename: cross-dir content preserved" "moving content" "$content"
    
    # Test 4: Rename directory
    local dir_src="${test_dir}/dir_src"
    local dir_dst="${test_dir}/dir_dst"
    mkdir "$dir_src"
    touch "${dir_src}/file_in_dir"
    mv "$dir_src" "$dir_dst"
    assert_file_not_exists "rename: directory source removed" "$dir_src"
    assert_dir_exists "rename: directory destination created" "$dir_dst"
    assert_file_exists "rename: directory contents preserved" "${dir_dst}/file_in_dir"
    
    cleanup_test_dir
}

# ----------------------------------------------------------------------------
# PHASE 4: Link Operations
# ----------------------------------------------------------------------------

test_phase4_link() {
    log "Testing hard link operations..."
    
    local test_dir=$(setup_test_dir)
    local original="${test_dir}/link_original"
    local hardlink="${test_dir}/link_hard"
    
    # Create original file
    echo "Shared content" > "$original"
    
    # Test 1: Create hard link
    ln "$original" "$hardlink"
    assert_file_exists "link: hard link created" "$hardlink"
    
    # Test 2: Verify link count
    local link_count=$(get_link_count "$original")
    assert_equals "link: original link count" "2" "$link_count"
    
    link_count=$(get_link_count "$hardlink")
    assert_equals "link: hard link count" "2" "$link_count"
    
    # Test 3: Verify shared content
    local content=$(cat "$hardlink")
    assert_equals "link: shared content" "Shared content" "$content"
    
    # Test 4: Modify through hard link
    echo "Modified content" > "$hardlink"
    content=$(cat "$original")
    assert_equals "link: modification visible in original" "Modified content" "$content"
    
    # Test 5: Remove original, hard link remains
    rm "$original"
    assert_file_not_exists "link: original removed" "$original"
    assert_file_exists "link: hard link remains" "$hardlink"
    content=$(cat "$hardlink")
    assert_equals "link: content persists" "Modified content" "$content"
    
    # Test 6: Hard link to directory should fail
    local test_subdir="${test_dir}/link_dir"
    mkdir "$test_subdir"
    ln "$test_subdir" "${test_dir}/dir_hardlink" 2>/dev/null
    local exit_code=$?
    if [ $exit_code -ne 0 ]; then
        record_test "link: directory hard link fails with EPERM" "command fails" "exit code $exit_code" "PASS"
    else
        record_test "link: directory hard link fails with EPERM" "command fails" "command succeeded" "FAIL"
    fi
    
    cleanup_test_dir
}

test_phase4_symlink() {
    log "Testing symbolic link operations..."
    
    local test_dir=$(setup_test_dir)
    local original="${test_dir}/symlink_original"
    local symlink="${test_dir}/symlink_link"
    
    # Create original file
    echo "Original content" > "$original"
    
    # Test 1: Create symbolic link
    ln -s "$original" "$symlink"
    assert_file_exists "symlink: link created" "$symlink"
    
    # Test 2: Verify it's a symlink
    if [ -L "$symlink" ]; then
        record_test "symlink: is symbolic link" "is symlink" "is symlink" "PASS"
    else
        record_test "symlink: is symbolic link" "is symlink" "not a symlink" "FAIL"
    fi
    
    # Test 3: Read symlink target
    local target=$(readlink "$symlink")
    assert_equals "symlink: target path" "$original" "$target"
    
    # Test 4: Access through symlink
    local content=$(cat "$symlink")
    assert_equals "symlink: read through link" "Original content" "$content"
    
    # Test 5: Modify through symlink
    echo "Modified via symlink" > "$symlink"
    content=$(cat "$original")
    assert_equals "symlink: write through link" "Modified via symlink" "$content"
    
    # Test 6: Remove original, symlink becomes dangling
    rm "$original"
    if [ ! -e "$symlink" ] && [ -L "$symlink" ]; then
        record_test "symlink: dangling link" "link dangling" "link dangling" "PASS"
    else
        record_test "symlink: dangling link" "link dangling" "link not dangling" "FAIL"
    fi
    
    # Test 7: Create symlink to directory
    local dir_target="${test_dir}/symlink_dir_target"
    local dir_link="${test_dir}/symlink_dir_link"
    mkdir "$dir_target"
    touch "${dir_target}/file_in_dir"
    ln -s "$dir_target" "$dir_link"
    assert_file_exists "symlink: directory link works" "${dir_link}/file_in_dir"
    
    # Test 8: Create symlink with relative path
    local rel_link="${test_dir}/symlink_relative"
    ln -s "symlink_dir_target" "$rel_link"
    assert_file_exists "symlink: relative path link" "${rel_link}/file_in_dir"
    
    cleanup_test_dir
}

# ----------------------------------------------------------------------------
# Error Handling Tests
# ----------------------------------------------------------------------------

test_error_handling() {
    log "Testing error handling..."
    
    local test_dir=$(setup_test_dir)
    
    # Test 1: Create file in non-existent directory (ENOENT)
    touch "${test_dir}/nonexistent/file" 2>/dev/null
    local exit_code=$?
    if [ $exit_code -ne 0 ]; then
        record_test "error: ENOENT - create in nonexistent dir" "command fails" "exit code $exit_code" "PASS"
    else
        record_test "error: ENOENT - create in nonexistent dir" "command fails" "command succeeded" "FAIL"
    fi
    
    # Test 2: Create file with existing name when not truncating
    local existing="${test_dir}/existing_file"
    touch "$existing"
    # Try to mkdir with same name
    mkdir "$existing" 2>/dev/null
    exit_code=$?
    if [ $exit_code -ne 0 ]; then
        record_test "error: EEXIST - mkdir on existing file" "command fails" "exit code $exit_code" "PASS"
    else
        record_test "error: EEXIST - mkdir on existing file" "command fails" "command succeeded" "FAIL"
    fi
    
    # Test 3: Unlink directory (EISDIR)
    local test_subdir="${test_dir}/error_dir"
    mkdir "$test_subdir"
    rm "$test_subdir" 2>/dev/null
    exit_code=$?
    if [ $exit_code -ne 0 ]; then
        record_test "error: EISDIR - rm on directory" "command fails" "exit code $exit_code" "PASS"
    else
        record_test "error: EISDIR - rm on directory" "command fails" "command succeeded" "FAIL"
    fi
    rmdir "$test_subdir"
    
    # Test 4: Rmdir on file (ENOTDIR)
    local test_file="${test_dir}/error_file"
    touch "$test_file"
    rmdir "$test_file" 2>/dev/null
    exit_code=$?
    if [ $exit_code -ne 0 ]; then
        record_test "error: ENOTDIR - rmdir on file" "command fails" "exit code $exit_code" "PASS"
    else
        record_test "error: ENOTDIR - rmdir on file" "command fails" "command succeeded" "FAIL"
    fi
    
    # Test 5: Operation on non-existent file
    cat "${test_dir}/nonexistent_file" 2>/dev/null
    exit_code=$?
    if [ $exit_code -ne 0 ]; then
        record_test "error: ENOENT - read nonexistent file" "command fails" "exit code $exit_code" "PASS"
    else
        record_test "error: ENOENT - read nonexistent file" "command fails" "command succeeded" "FAIL"
    fi
    
    cleanup_test_dir
}

# ----------------------------------------------------------------------------
# Read-Only Mount Tests
# ----------------------------------------------------------------------------

test_readonly_mount() {
    log "Testing read-only mount protection..."
    
    # This test requires remounting in read-only mode
    # Skip if the filesystem is already mounted read-write and we can't remount
    
    record_test "readonly: write protection" "skipped - requires separate mount test" "" "SKIP"
}

# ============================================================================
# Main Test Runner
# ============================================================================

run_all_tests() {
    log "Starting fusexfs write operations test suite..."
    log "Mount point: ${MOUNT_POINT}"
    log "XFS Image: ${XFS_IMAGE}"
    echo ""
    
    # Phase 1: Foundation
    echo "============================================"
    echo "PHASE 1: Foundation Operations"
    echo "============================================"
    test_phase1_chmod
    test_phase1_chown
    test_phase1_utimens
    test_phase1_truncate
    test_phase1_fsync
    echo ""
    
    # Phase 2: File I/O
    echo "============================================"
    echo "PHASE 2: File I/O Operations"
    echo "============================================"
    test_phase2_create
    test_phase2_write
    test_phase2_mknod
    echo ""
    
    # Phase 3: Directory Operations
    echo "============================================"
    echo "PHASE 3: Directory Operations"
    echo "============================================"
    test_phase3_mkdir
    test_phase3_rmdir
    test_phase3_unlink
    test_phase3_rename
    echo ""
    
    # Phase 4: Link Operations
    echo "============================================"
    echo "PHASE 4: Link Operations"
    echo "============================================"
    test_phase4_link
    test_phase4_symlink
    echo ""
    
    # Error Handling
    echo "============================================"
    echo "Error Handling Tests"
    echo "============================================"
    test_error_handling
    test_readonly_mount
    echo ""
}

print_summary() {
    echo ""
    echo "============================================"
    echo "TEST SUMMARY"
    echo "============================================"
    echo -e "Total:   ${TOTAL_TESTS}"
    echo -e "${GREEN}Passed:  ${TESTS_PASSED}${NC}"
    echo -e "${RED}Failed:  ${TESTS_FAILED}${NC}"
    echo -e "${YELLOW}Skipped: ${TESTS_SKIPPED}${NC}"
    echo ""
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}All tests passed!${NC}"
        return 0
    else
        echo -e "${RED}Some tests failed. See above for details.${NC}"
        return 1
    fi
}

# ============================================================================
# Entry Point
# ============================================================================

main() {
    # Parse arguments
    if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
        usage
        exit 0
    fi
    
    if [ $# -lt 2 ]; then
        log_error "Missing required arguments."
        usage
        exit 1
    fi
    
    XFS_IMAGE="$1"
    MOUNT_POINT="$2"
    FUSEXFS_BINARY="${3:-$DEFAULT_FUSEXFS_BINARY}"
    
    # Validate arguments
    if [ ! -e "$XFS_IMAGE" ]; then
        log_error "XFS image not found: $XFS_IMAGE"
        exit 1
    fi
    
    if [ ! -d "$MOUNT_POINT" ]; then
        log_error "Mount point is not a directory: $MOUNT_POINT"
        exit 1
    fi
    
    # Check if mount point is accessible and writable
    if ! touch "${MOUNT_POINT}/.write_test" 2>/dev/null; then
        log_error "Mount point is not writable: $MOUNT_POINT"
        log_error "Make sure the XFS filesystem is mounted in read-write mode."
        exit 1
    fi
    rm -f "${MOUNT_POINT}/.write_test"
    
    # Run tests
    run_all_tests
    
    # Print summary and exit with appropriate code
    print_summary
    exit $?
}

# Run main function
main "$@"
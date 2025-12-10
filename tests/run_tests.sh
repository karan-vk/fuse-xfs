#!/bin/bash
#
# run_tests.sh - CI integration script for fusexfs write operations tests
#
# This script:
# 1. Creates a small XFS image for testing
# 2. Mounts it with fusexfs in read-write mode
# 3. Runs the test suite
# 4. Unmounts and cleans up
# 5. Reports results
#
# Usage: ./run_tests.sh [options]
#
# Options:
#   -s, --size SIZE      Size of test image in MB (default: 64)
#   -k, --keep           Keep test image after tests
#   -v, --verbose        Verbose output
#   -h, --help           Show help
#
# Environment Variables:
#   FUSEXFS_BINARY       Path to fusexfs binary (default: ../build/fusexfs)
#   TEST_IMAGE_PATH      Path for test image (default: /tmp/fusexfs_test.xfs)
#   MOUNT_POINT          Mount point (default: /tmp/fusexfs_test_mount)
#
# Author: FuseXFS Development Team
# Version: 1.0
#

set -e

# ============================================================================
# Configuration
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPT_NAME="$(basename "${BASH_SOURCE[0]}")"

# Default values
DEFAULT_IMAGE_SIZE=64  # MB
DEFAULT_TEST_IMAGE="/tmp/fusexfs_test_$$.xfs"
DEFAULT_MOUNT_POINT="/tmp/fusexfs_test_mount_$$"
DEFAULT_FUSEXFS_BINARY="${SCRIPT_DIR}/../build/fusexfs"

# Runtime variables
IMAGE_SIZE="$DEFAULT_IMAGE_SIZE"
TEST_IMAGE="${TEST_IMAGE_PATH:-$DEFAULT_TEST_IMAGE}"
MOUNT_POINT="${MOUNT_POINT:-$DEFAULT_MOUNT_POINT}"
FUSEXFS_BINARY="${FUSEXFS_BINARY:-$DEFAULT_FUSEXFS_BINARY}"
KEEP_IMAGE=0
VERBOSE=0

# Colors
if [ -t 1 ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[0;33m'
    BLUE='\033[0;34m'
    NC='\033[0m'
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    NC=''
fi

# ============================================================================
# Helper Functions
# ============================================================================

usage() {
    cat << EOF
Usage: $SCRIPT_NAME [options]

CI integration script for fusexfs write operations tests.

Options:
  -s, --size SIZE      Size of test image in MB (default: ${DEFAULT_IMAGE_SIZE})
  -k, --keep           Keep test image and mount point after tests
  -v, --verbose        Verbose output
  -h, --help           Show this help message

Environment Variables:
  FUSEXFS_BINARY       Path to fusexfs binary (default: ${DEFAULT_FUSEXFS_BINARY})
  TEST_IMAGE_PATH      Path for test image (default: /tmp/fusexfs_test_PID.xfs)
  MOUNT_POINT          Mount point (default: /tmp/fusexfs_test_mount_PID)

Examples:
  $SCRIPT_NAME                    # Run with defaults
  $SCRIPT_NAME -s 128             # Use 128MB test image
  $SCRIPT_NAME -k -v              # Keep image, verbose output
  FUSEXFS_BINARY=./fusexfs $SCRIPT_NAME  # Custom binary

Exit Codes:
  0    All tests passed
  1    Some tests failed
  2    Setup error (missing dependencies, etc.)
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

# Check for required commands
check_dependencies() {
    local missing=()
    
    # Check for mkfs.xfs
    if ! command -v mkfs.xfs &> /dev/null; then
        missing+=("mkfs.xfs (xfsprogs)")
    fi
    
    # Check for fusermount (FUSE utilities)
    if ! command -v fusermount &> /dev/null; then
        # On macOS, check for umount with FUSE
        if [[ "$(uname)" == "Darwin" ]]; then
            if ! command -v umount &> /dev/null; then
                missing+=("umount")
            fi
        else
            missing+=("fusermount (fuse)")
        fi
    fi
    
    # Check for dd
    if ! command -v dd &> /dev/null; then
        missing+=("dd")
    fi
    
    # Check for fusexfs binary
    if [ ! -x "$FUSEXFS_BINARY" ]; then
        missing+=("fusexfs binary at $FUSEXFS_BINARY")
    fi
    
    if [ ${#missing[@]} -gt 0 ]; then
        log_error "Missing required dependencies:"
        for dep in "${missing[@]}"; do
            echo "  - $dep"
        done
        return 1
    fi
    
    return 0
}

# Create test XFS image
create_test_image() {
    log "Creating ${IMAGE_SIZE}MB XFS test image at ${TEST_IMAGE}..."
    
    # Create sparse file
    dd if=/dev/zero of="$TEST_IMAGE" bs=1M count=0 seek="$IMAGE_SIZE" 2>/dev/null
    
    if [ ! -f "$TEST_IMAGE" ]; then
        log_error "Failed to create test image"
        return 1
    fi
    
    # Format as XFS
    log "Formatting image as XFS..."
    mkfs.xfs -f "$TEST_IMAGE" > /dev/null 2>&1
    
    if [ $? -ne 0 ]; then
        log_error "Failed to format XFS image"
        rm -f "$TEST_IMAGE"
        return 1
    fi
    
    log_success "Test image created successfully"
    return 0
}

# Create mount point
create_mount_point() {
    log "Creating mount point at ${MOUNT_POINT}..."
    
    mkdir -p "$MOUNT_POINT"
    
    if [ ! -d "$MOUNT_POINT" ]; then
        log_error "Failed to create mount point"
        return 1
    fi
    
    log_success "Mount point created"
    return 0
}

# Mount XFS image with fusexfs
mount_fusexfs() {
    log "Mounting XFS image with fusexfs in read-write mode..."
    
    # Try to mount with fusexfs
    if [ "$VERBOSE" = "1" ]; then
        "$FUSEXFS_BINARY" -rw "$TEST_IMAGE" -- "$MOUNT_POINT" -f &
    else
        "$FUSEXFS_BINARY" -rw "$TEST_IMAGE" -- "$MOUNT_POINT" &
    fi
    
    local fusexfs_pid=$!
    
    # Wait for mount to complete
    local wait_count=0
    local max_wait=30
    
    while [ $wait_count -lt $max_wait ]; do
        if mountpoint -q "$MOUNT_POINT" 2>/dev/null || [ -d "${MOUNT_POINT}/." ]; then
            # Try to access the mount point
            if ls "$MOUNT_POINT" &>/dev/null; then
                log_success "Filesystem mounted successfully (PID: $fusexfs_pid)"
                echo "$fusexfs_pid" > "${TEST_IMAGE}.pid"
                return 0
            fi
        fi
        sleep 0.5
        wait_count=$((wait_count + 1))
    done
    
    log_error "Failed to mount filesystem within ${max_wait}s"
    kill "$fusexfs_pid" 2>/dev/null || true
    return 1
}

# Unmount fusexfs
unmount_fusexfs() {
    log "Unmounting filesystem..."
    
    # Sync before unmount
    sync
    
    # Read PID if available
    local pid_file="${TEST_IMAGE}.pid"
    local fusexfs_pid=""
    if [ -f "$pid_file" ]; then
        fusexfs_pid=$(cat "$pid_file")
        rm -f "$pid_file"
    fi
    
    # Try fusermount first (Linux)
    if command -v fusermount &> /dev/null; then
        fusermount -u "$MOUNT_POINT" 2>/dev/null && {
            log_success "Filesystem unmounted"
            return 0
        }
    fi
    
    # Try umount (macOS and fallback)
    umount "$MOUNT_POINT" 2>/dev/null && {
        log_success "Filesystem unmounted"
        return 0
    }
    
    # Force kill fusexfs process if needed
    if [ -n "$fusexfs_pid" ]; then
        log_warn "Forcing fusexfs termination..."
        kill -9 "$fusexfs_pid" 2>/dev/null || true
        sleep 1
        
        # Try unmount again
        fusermount -u "$MOUNT_POINT" 2>/dev/null || umount "$MOUNT_POINT" 2>/dev/null || true
    fi
    
    # Check if still mounted
    if mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
        log_warn "Mount point may still be mounted"
        return 1
    fi
    
    return 0
}

# Cleanup
cleanup() {
    log "Cleaning up..."
    
    # Unmount if mounted
    if mountpoint -q "$MOUNT_POINT" 2>/dev/null || [ -d "${MOUNT_POINT}/." ] 2>/dev/null; then
        unmount_fusexfs
    fi
    
    # Remove mount point
    if [ -d "$MOUNT_POINT" ] && [ "$KEEP_IMAGE" != "1" ]; then
        rmdir "$MOUNT_POINT" 2>/dev/null || rm -rf "$MOUNT_POINT"
    fi
    
    # Remove test image
    if [ -f "$TEST_IMAGE" ] && [ "$KEEP_IMAGE" != "1" ]; then
        rm -f "$TEST_IMAGE"
        log_success "Test image removed"
    elif [ "$KEEP_IMAGE" = "1" ] && [ -f "$TEST_IMAGE" ]; then
        log "Test image kept at: $TEST_IMAGE"
    fi
    
    # Remove pid file
    rm -f "${TEST_IMAGE}.pid"
}

# Run the test suite
run_tests() {
    local test_script="${SCRIPT_DIR}/test_write_operations.sh"
    
    if [ ! -x "$test_script" ]; then
        # Try to make it executable
        chmod +x "$test_script" 2>/dev/null || true
    fi
    
    if [ ! -f "$test_script" ]; then
        log_error "Test script not found: $test_script"
        return 2
    fi
    
    log "Running test suite..."
    echo ""
    
    local test_args=("$TEST_IMAGE" "$MOUNT_POINT")
    if [ "$VERBOSE" = "1" ]; then
        export VERBOSE=1
    fi
    
    # Run tests
    if bash "$test_script" "${test_args[@]}"; then
        return 0
    else
        return 1
    fi
}

# ============================================================================
# Signal Handlers
# ============================================================================

handle_interrupt() {
    echo ""
    log_warn "Interrupted! Cleaning up..."
    cleanup
    exit 130
}

handle_error() {
    log_error "An error occurred. Cleaning up..."
    cleanup
    exit 2
}

# ============================================================================
# Main
# ============================================================================

main() {
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -s|--size)
                IMAGE_SIZE="$2"
                shift 2
                ;;
            -k|--keep)
                KEEP_IMAGE=1
                shift
                ;;
            -v|--verbose)
                VERBOSE=1
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                usage
                exit 2
                ;;
        esac
    done
    
    # Setup signal handlers
    trap handle_interrupt INT TERM
    trap handle_error ERR
    
    echo "============================================"
    echo "FuseXFS Write Operations Test Runner"
    echo "============================================"
    echo ""
    
    # Check dependencies
    log "Checking dependencies..."
    if ! check_dependencies; then
        exit 2
    fi
    log_success "All dependencies found"
    echo ""
    
    # Create test environment
    if ! create_test_image; then
        cleanup
        exit 2
    fi
    
    if ! create_mount_point; then
        cleanup
        exit 2
    fi
    
    if ! mount_fusexfs; then
        cleanup
        exit 2
    fi
    
    echo ""
    
    # Run tests
    local test_result=0
    run_tests || test_result=$?
    
    echo ""
    
    # Cleanup
    cleanup
    
    # Report final result
    echo ""
    echo "============================================"
    if [ $test_result -eq 0 ]; then
        echo -e "${GREEN}TEST RUN COMPLETED SUCCESSFULLY${NC}"
    else
        echo -e "${RED}TEST RUN COMPLETED WITH FAILURES${NC}"
    fi
    echo "============================================"
    
    exit $test_result
}

# Run main
main "$@"
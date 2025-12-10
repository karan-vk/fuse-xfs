# FuseXFS Write Operations Test Suite

This directory contains comprehensive tests for the fusexfs write operation implementation.

## Overview

The test suite validates all write operations implemented in fusexfs across 4 phases:

| Phase | Operations | Description |
|-------|------------|-------------|
| Phase 1 | `chmod`, `chown`, `utimens`, `truncate`, `fsync` | File attribute modifications |
| Phase 2 | `create`, `write`, `mknod` | File creation and I/O |
| Phase 3 | `mkdir`, `unlink`, `rmdir`, `rename` | Directory operations |
| Phase 4 | `link`, `symlink` | Link operations |

## Quick Start

### Run All Tests (CI Mode)

```bash
# Run the full test suite with automatic setup/teardown
./run_tests.sh

# Run with verbose output
./run_tests.sh -v

# Keep test image after tests (for debugging)
./run_tests.sh -k

# Use custom image size (128MB)
./run_tests.sh -s 128
```

### Run Tests on Existing Mount

```bash
# If you already have an XFS image mounted:
./test_write_operations.sh /path/to/xfs.img /mount/point

# Or specify a custom fusexfs binary:
./test_write_operations.sh /path/to/xfs.img /mount/point /path/to/fusexfs
```

## Prerequisites

### Required Tools

- **mkfs.xfs** - Part of xfsprogs package
- **fusermount** - Part of FUSE utilities (Linux) or use `umount` (macOS)
- **dd** - Standard Unix utility
- **stat** - For file information queries
- **fusexfs** - The compiled fusexfs binary

### Installation

**Debian/Ubuntu:**
```bash
sudo apt-get install xfsprogs fuse
```

**macOS (with Homebrew):**
```bash
brew install xfsprogs macfuse
```

**Building fusexfs:**
```bash
cd /path/to/fusexfs
mkdir build && cd build
cmake ..
make
```

## Test Files

| File | Description |
|------|-------------|
| `test_write_operations.sh` | Main test script with all test cases |
| `run_tests.sh` | CI integration script for automated testing |
| `README.md` | This documentation file |

## Test Categories

### Phase 1: Foundation Operations

#### chmod Tests
- Set mode to various permission combinations (755, 644, 600, 777)
- Symbolic notation (u+x, g+w, etc.)
- Directory permission changes

#### chown Tests
- Change owner (uid)
- Change group (gid)
- Change both owner and group
- Note: Requires root for full testing

#### utimens Tests
- Set specific timestamps
- Update to current time (touch)
- Verify mtime changes

#### truncate Tests
- Shrink file to smaller size
- Truncate to zero
- Extend file (sparse file creation)
- Content preservation verification

#### fsync Tests
- Data persistence after sync
- Verify fsync callback works

### Phase 2: File I/O Operations

#### create Tests
- Create empty file with touch
- Create file with content (echo, printf)
- Create with dd
- Create with specific permissions

#### write Tests
- Basic write and verification
- Append to file
- Overwrite existing content
- Write larger files (100KB+)
- Write at specific offset
- Sequential writes

#### mknod Tests
- Create FIFO (named pipe)
- Create device nodes (requires root)

### Phase 3: Directory Operations

#### mkdir Tests
- Create single directory
- Create nested directories (mkdir -p)
- Create with specific mode
- Create multiple directories

#### rmdir Tests
- Remove empty directory
- ENOTEMPTY error on non-empty directory
- Remove nested directories

#### unlink Tests
- Remove regular file
- EISDIR error on directory
- Remove multiple files

#### rename Tests
- Rename within same directory
- Rename to overwrite existing
- Rename across directories
- Rename directories

### Phase 4: Link Operations

#### hard link Tests
- Create hard link
- Verify link count
- Shared content verification
- Modification through link
- Original removal, link persists
- EPERM error for directory hard links

#### symlink Tests
- Create symbolic link
- Read link target
- Access through symlink
- Modify through symlink
- Dangling link after target removal
- Symlink to directory
- Relative path symlinks

### Error Handling Tests
- ENOENT: Create in non-existent directory
- EEXIST: mkdir on existing file
- EISDIR: rm on directory
- ENOTDIR: rmdir on file
- ENOENT: Read non-existent file

## Test Results Format

### Console Output

```
============================================
PHASE 1: Foundation Operations
============================================
[INFO] Testing chmod operations...
[PASS] chmod: set mode to 755
[PASS] chmod: set mode to 644
[FAIL] chmod: set mode to 600
       Expected: 600
       Actual:   644
[SKIP] chown: requires root
```

### Summary

```
============================================
TEST SUMMARY
============================================
Total:   50
Passed:  45
Failed:  3
Skipped: 2

Some tests failed. See above for details.
```

### Exit Codes

| Code | Meaning |
|------|---------|
| 0 | All tests passed |
| 1 | Some tests failed |
| 2 | Setup error (missing dependencies, mount failure, etc.) |
| 130 | Interrupted (Ctrl+C) |

## Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `FUSEXFS_BINARY` | Path to fusexfs binary | `../build/fusexfs` |
| `TEST_IMAGE_PATH` | Path for test image file | `/tmp/fusexfs_test_PID.xfs` |
| `MOUNT_POINT` | Mount point directory | `/tmp/fusexfs_test_mount_PID` |
| `SKIP_CHOWN_TESTS` | Set to 1 to skip chown tests | (unset) |
| `VERBOSE` | Set to 1 for verbose output | (unset) |

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Test Write Operations

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y xfsprogs fuse libfuse-dev
    
    - name: Build fusexfs
      run: |
        mkdir build && cd build
        cmake ..
        make
    
    - name: Run tests
      run: |
        cd tests
        chmod +x *.sh
        ./run_tests.sh -v
```

### Jenkins Pipeline Example

```groovy
pipeline {
    agent any
    
    stages {
        stage('Build') {
            steps {
                sh 'mkdir -p build && cd build && cmake .. && make'
            }
        }
        
        stage('Test') {
            steps {
                sh 'cd tests && chmod +x *.sh && ./run_tests.sh'
            }
        }
    }
    
    post {
        always {
            archiveArtifacts artifacts: 'tests/*.log', allowEmptyArchive: true
        }
    }
}
```

## Troubleshooting

### "Mount point is not writable"

Make sure fusexfs is mounted with the `-rw` flag:
```bash
fusexfs -rw /path/to/xfs.img -- /mount/point
```

### "fusexfs binary not found"

Either build fusexfs first or set the `FUSEXFS_BINARY` environment variable:
```bash
export FUSEXFS_BINARY=/path/to/fusexfs
./run_tests.sh
```

### "mkfs.xfs not found"

Install xfsprogs:
```bash
# Debian/Ubuntu
sudo apt-get install xfsprogs

# macOS
brew install xfsprogs
```

### Tests fail with EROFS

The filesystem may be mounted read-only. Check the mount options:
```bash
mount | grep fusexfs
```

Ensure you're using the `-rw` flag when mounting.

### "Operation not permitted" errors

Some operations (chown, mknod device nodes) require root privileges. Run as root or skip those tests:
```bash
sudo ./run_tests.sh
# Or
SKIP_CHOWN_TESTS=1 ./run_tests.sh
```

## Adding New Tests

To add a new test case:

1. Open `test_write_operations.sh`
2. Add your test function in the appropriate phase section
3. Use the helper functions for assertions:
   - `assert_equals "test name" "expected" "actual"`
   - `assert_file_exists "test name" "/path/to/file"`
   - `assert_file_not_exists "test name" "/path/to/file"`
   - `assert_dir_exists "test name" "/path/to/dir"`
   - `assert_symlink "test name" "/path/to/link" "expected_target"`
   - `record_test "test name" "expected" "actual" "PASS|FAIL|SKIP"`
4. Call your test function from `run_all_tests()`

Example:
```bash
test_my_new_feature() {
    log "Testing my new feature..."
    
    local test_dir=$(setup_test_dir)
    local test_file="${test_dir}/my_test"
    
    # Your test code here
    touch "$test_file"
    
    # Assertions
    assert_file_exists "my_feature: file created" "$test_file"
    
    cleanup_test_dir
}
```

## Success Criteria

The test suite is considered successful when:

1. **All critical tests pass** - File operations (create, write, read, delete)
2. **Directory operations work** - mkdir, rmdir, rename
3. **Link operations work** - symlink, hard link
4. **Attribute changes persist** - chmod, truncate
5. **Error handling is correct** - Proper errno values returned
6. **No data corruption** - Written content matches read content
7. **Filesystem remains consistent** - Can be unmounted and remounted

## License

This test suite is part of the FuseXFS project and is distributed under the same license.

## Contributing

When adding new write operations to fusexfs, please also add corresponding test cases to this test suite. Follow the existing patterns for consistency.
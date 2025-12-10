# Changelog

All notable changes to the fusexfs project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

#### Full Read-Write Support
- **Command-line option `-rw`** for read-write mounting
- **File creation and writing capabilities**:
  - `create` - Create new files
  - `write` - Write data to files
  - `mknod` - Create device nodes, FIFOs, and sockets
- **Directory management operations**:
  - `mkdir` - Create directories
  - `rmdir` - Remove empty directories
  - `unlink` - Remove files
  - `rename` - Rename files and directories
- **File attribute operations**:
  - `chmod` - Change file permissions
  - `chown` - Change file ownership
  - `utimens` - Update file timestamps
  - `truncate` - Change file size
- **Link operations**:
  - `link` - Create hard links
  - `symlink` - Create symbolic links
- **Synchronization**:
  - `fsync` - Synchronize file data to disk

#### New API Functions in xfsutil
- `mount_xfs_ex()` - Mount with explicit read-only flag
- `unmount_xfs()` - Proper unmount with buffer flushing
- `xfs_is_readonly()` - Check mount mode
- `xfs_setattr_mode()` - Change file permissions
- `xfs_setattr_owner()` - Change file ownership
- `xfs_setattr_time()` - Update timestamps
- `xfs_truncate_file()` - Change file size
- `xfs_create_file()` - Create files and device nodes
- `xfs_write_file()` - Write data to files
- `xfs_create_dir()` - Create directories
- `xfs_remove_file()` - Remove files
- `xfs_remove_dir()` - Remove directories
- `xfs_rename_entry()` - Rename files/directories
- `xfs_create_link()` - Create hard links
- `xfs_create_symlink()` - Create symbolic links
- `xfs_sync_file()` - Sync file to disk
- `xfs_sync_fs()` - Sync filesystem
- `xfs_path_split()` - Split path utility
- `xfs_lookup_parent()` - Look up parent directory

#### FUSE Handlers
- `fuse_xfs_chmod()` - Handle chmod requests
- `fuse_xfs_chown()` - Handle chown requests
- `fuse_xfs_utimens()` - Handle timestamp updates
- `fuse_xfs_truncate()` - Handle truncate requests
- `fuse_xfs_create()` - Handle file creation
- `fuse_xfs_write()` - Handle write requests
- `fuse_xfs_mknod()` - Handle special file creation
- `fuse_xfs_mkdir()` - Handle directory creation
- `fuse_xfs_unlink()` - Handle file removal
- `fuse_xfs_rmdir()` - Handle directory removal
- `fuse_xfs_rename()` - Handle rename operations
- `fuse_xfs_link()` - Handle hard link creation
- `fuse_xfs_symlink()` - Handle symbolic link creation
- `fuse_xfs_fsync()` - Handle sync requests

#### Testing Infrastructure
- Comprehensive test suite in `tests/` directory
- `test_write_operations.sh` - Main test script covering all operations
- `run_tests.sh` - CI integration script
- Test documentation in `tests/README.md`

#### Documentation
- `WRITE_SUPPORT.md` - User guide for write operations
- `API.md` - Complete API reference
- Updated `README.md` with write support information
- Updated `WRITE_OPERATIONS_DESIGN.md` with implementation status

### Changed

- **`mount_xfs()`** now calls `mount_xfs_ex()` internally with read-only default
- **FUSE operations structure** updated with all write operation callbacks
- **README.md** updated to reflect full read-write support:
  - Changed description from "Read-only" to "Full read-write"
  - Added usage examples for `-rw` flag
  - Updated supported features table with write operations
  - Updated limitations section

### Fixed

- Proper inode release in all FUSE handlers
- Correct error code propagation from xfsutil to FUSE layer
- Transaction cleanup on operation failures

### Technical Details

#### Transaction Safety
All write operations use XFS's transaction system:
1. Transaction allocation with appropriate reservation
2. Inode locking and joining to transaction
3. Operation execution
4. Change logging
5. Atomic commit

#### Supported File Types
- Regular files (S_IFREG)
- Directories (S_IFDIR)
- Symbolic links (S_IFLNK)
- Block devices (S_IFBLK)
- Character devices (S_IFCHR)
- FIFOs/Named pipes (S_IFIFO)
- Sockets (S_IFSOCK)

### Known Issues

- **Extended attributes write** - Not supported (read-only access)
- **ACLs** - Not implemented
- **Quotas** - Not implemented
- **Reflinks** - Not implemented
- **External log devices** - Not supported
- **Real-time section** - Not supported

### Dependencies

No new dependencies. Uses existing:
- macFUSE 4.x
- libxfs (bundled xfsprogs)

---

## [0.2.1] - Previous Version

### Features
- Apple Silicon (ARM64) and Intel (x86_64) support
- XFS V5 superblock format with CRC checksums
- FTYPE directory entries support
- macFUSE 4.x compatibility
- Read-only access to XFS filesystems

### Supported XFS Features
- V4 and V5 superblocks
- Dir2 format directories
- Shortform, block, and leaf directories
- Regular files
- Symbolic links (read)
- Extended attributes (read)

---

## [0.2.0] - macFUSE Migration

### Changed
- Migrated from deprecated osxfuse to macFUSE
- Added ARM64 (Apple Silicon) support

---

## [0.1.x] - Original Implementation

### Features
- Initial osxfuse-based implementation
- Read-only XFS filesystem access
- Basic directory and file reading

---

## Version Comparison

| Feature | 0.1.x | 0.2.0 | 0.2.1 | Write Support |
|---------|-------|-------|-------|---------------|
| Read files | ✅ | ✅ | ✅ | ✅ |
| List directories | ✅ | ✅ | ✅ | ✅ |
| Read symlinks | ✅ | ✅ | ✅ | ✅ |
| XFS V5 | ❌ | ❌ | ✅ | ✅ |
| Apple Silicon | ❌ | ✅ | ✅ | ✅ |
| macFUSE | ❌ | ✅ | ✅ | ✅ |
| **Create files** | ❌ | ❌ | ❌ | ✅ |
| **Write files** | ❌ | ❌ | ❌ | ✅ |
| **Delete files** | ❌ | ❌ | ❌ | ✅ |
| **Create directories** | ❌ | ❌ | ❌ | ✅ |
| **Remove directories** | ❌ | ❌ | ❌ | ✅ |
| **Rename** | ❌ | ❌ | ❌ | ✅ |
| **Hard links** | ❌ | ❌ | ❌ | ✅ |
| **Create symlinks** | ❌ | ❌ | ❌ | ✅ |
| **chmod/chown** | ❌ | ❌ | ❌ | ✅ |
| **truncate** | ❌ | ❌ | ❌ | ✅ |

---

## Migration Guide

### From Read-Only to Read-Write

If you were previously using fusexfs in read-only mode and want to enable write support:

1. **Ensure filesystem is unmounted cleanly**
   ```bash
   umount /mount/point
   ```

2. **Mount with read-write flag**
   ```bash
   fusexfs -rw /path/to/xfs.img /mount/point
   ```

3. **Verify write access**
   ```bash
   touch /mount/point/test_write
   rm /mount/point/test_write
   ```

### Backward Compatibility

- Default mount mode remains **read-only** for safety
- All existing read operations work unchanged
- No changes required for read-only usage

---

## Contributors

- Original fuse-xfs authors
- @karan-vk - Modernization and write support implementation

## License

GPL License - See [COPYING](COPYING) file for details.
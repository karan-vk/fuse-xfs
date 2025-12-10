# FuseXFS Write Support Documentation

This document provides comprehensive documentation for the write operation support in fusexfs.

## Overview

FuseXFS now supports full read-write access to XFS filesystems. This includes creating, modifying, and deleting files and directories, as well as managing file attributes, timestamps, and links.

All write operations are implemented using XFS transaction-based architecture, ensuring filesystem consistency even in the event of unexpected interruptions.

## Enabling Read-Write Mode

By default, fusexfs mounts filesystems in **read-only mode** for safety. To enable write operations, use the `-rw` flag when mounting:

```bash
# Mount with read-write support
fusexfs -rw /path/to/xfs.img /mount/point

# Or with the fuse-xfs binary
./build/bin/fuse-xfs -rw /path/to/xfs.img /mount/point
```

### Mount Options

| Option | Description |
|--------|-------------|
| `-rw` | Enable read-write mode |
| `-d` | Enable debug output |
| (default) | Read-only mode |

### Verifying Mount Mode

You can verify the mount mode using the `mount` command:

```bash
mount | grep fusexfs
```

## Supported Write Operations

### Phase 1: Foundation Operations

These operations modify file metadata without changing file content or structure.

#### chmod - Change File Permissions

Changes the permission bits of a file or directory.

```bash
# Change to read-write-execute for owner, read-execute for group and others
chmod 755 /mount/point/file.txt

# Remove write permission from group
chmod g-w /mount/point/file.txt
```

**Implementation Notes:**
- Preserves file type bits (S_IFMT)
- Updates ctime (change time) automatically
- Transaction-protected

#### chown - Change File Ownership

Changes the owner and/or group of a file or directory.

```bash
# Change owner
chown user /mount/point/file.txt

# Change owner and group
chown user:group /mount/point/file.txt

# Change group only
chown :group /mount/point/file.txt
```

**Implementation Notes:**
- Either uid or gid can be -1 to leave unchanged
- Clears setuid/setgid bits when changing owner (security)
- Updates ctime automatically
- Requires appropriate privileges

#### utimens - Update Timestamps

Updates the access time (atime) and modification time (mtime) of a file.

```bash
# Update to current time
touch /mount/point/file.txt

# Set specific time
touch -t 202312251200 /mount/point/file.txt
```

**Implementation Notes:**
- Supports UTIME_NOW for current time
- Supports UTIME_OMIT to leave unchanged
- Updates ctime automatically
- Nanosecond precision supported

#### truncate - Change File Size

Changes the size of a file.

```bash
# Truncate to zero bytes
truncate -s 0 /mount/point/file.txt

# Set to specific size
truncate -s 1024 /mount/point/file.txt

# Extend file (creates sparse region)
truncate -s 1G /mount/point/file.txt
```

**Implementation Notes:**
- Shrinking frees disk blocks beyond new size
- Extending creates a sparse region (hole)
- Only works on regular files
- Updates mtime and ctime

#### fsync - Synchronize File Data

Flushes file data and metadata to disk.

```bash
# Typically called programmatically
sync /mount/point/file.txt
```

**Implementation Notes:**
- Ensures data durability
- Works with file handles
- Can sync entire filesystem

### Phase 2: File I/O Operations

These operations create, write, and delete files.

#### create - Create New Files

Creates a new file and opens it for writing.

```bash
# Create empty file
touch /mount/point/newfile.txt

# Create and write content
echo "Hello, XFS!" > /mount/point/newfile.txt

# Create with specific permissions
install -m 644 /dev/null /mount/point/newfile.txt
```

**Implementation Notes:**
- Allocates new inode
- Creates directory entry in parent
- Sets initial permissions (modified by umask)
- Returns file handle for subsequent operations

#### write - Write Data to Files

Writes data to an open file.

```bash
# Write content
echo "Data to write" > /mount/point/file.txt

# Append content
echo "More data" >> /mount/point/file.txt

# Write using dd
dd if=/dev/urandom of=/mount/point/file.txt bs=1M count=10
```

**Implementation Notes:**
- Allocates blocks as needed
- Supports writing at any offset
- Extends file size automatically
- Updates mtime and ctime
- Multiple small writes are efficient

#### mknod - Create Special Files

Creates device nodes, FIFOs, and sockets.

```bash
# Create a FIFO (named pipe)
mkfifo /mount/point/myfifo

# Create a block device (requires root)
sudo mknod /mount/point/mydev b 8 0

# Create a character device (requires root)
sudo mknod /mount/point/mychar c 1 3
```

**Implementation Notes:**
- Supports S_IFBLK, S_IFCHR, S_IFIFO, S_IFSOCK
- Device number (rdev) is stored for block/char devices
- Requires appropriate privileges for device nodes

### Phase 3: Directory Operations

These operations manage the directory structure.

#### mkdir - Create Directories

Creates a new directory.

```bash
# Create a directory
mkdir /mount/point/newdir

# Create with specific permissions
mkdir -m 755 /mount/point/newdir

# Create nested directories
mkdir -p /mount/point/path/to/nested/dir
```

**Implementation Notes:**
- Initializes with . and .. entries
- Sets link count to 2 (. entry)
- Increments parent link count (for .. entry)
- Permissions are modified by umask

#### rmdir - Remove Empty Directories

Removes an empty directory.

```bash
# Remove a directory
rmdir /mount/point/emptydir

# Remove nested directories
rmdir -p /mount/point/path/to/nested/dir
```

**Implementation Notes:**
- Fails with ENOTEMPTY if directory is not empty
- Decrements parent link count
- Frees directory inode

#### unlink - Remove Files

Removes a file (decrements link count).

```bash
# Remove a file
rm /mount/point/file.txt

# Remove without confirmation
rm -f /mount/point/file.txt
```

**Implementation Notes:**
- Decrements link count
- File data is freed when link count reaches 0
- Cannot unlink directories (use rmdir)
- Updates parent directory mtime and ctime

#### rename - Rename/Move Files and Directories

Renames or moves files and directories.

```bash
# Rename a file
mv /mount/point/oldname.txt /mount/point/newname.txt

# Move to different directory
mv /mount/point/file.txt /mount/point/subdir/

# Rename a directory
mv /mount/point/olddir /mount/point/newdir
```

**Implementation Notes:**
- Works within same filesystem only
- Can overwrite existing target (with type compatibility)
- Updates parent link counts for directory moves
- Updates .. entry when moving directories
- Atomic operation

### Phase 4: Link Operations

These operations manage filesystem links.

#### link - Create Hard Links

Creates a hard link to an existing file.

```bash
# Create a hard link
ln /mount/point/original.txt /mount/point/hardlink.txt
```

**Implementation Notes:**
- Both names point to same inode
- Increments link count on target
- Cannot create hard links to directories
- Maximum link count enforced (XFS_MAXLINK)
- Both names must be on same filesystem

#### symlink - Create Symbolic Links

Creates a symbolic link pointing to a target path.

```bash
# Create a symbolic link
ln -s /mount/point/target.txt /mount/point/symlink.txt

# Create relative symlink
ln -s ../target.txt /mount/point/subdir/symlink.txt

# Create symlink to directory
ln -s /mount/point/targetdir /mount/point/symlinkdir
```

**Implementation Notes:**
- Target path is stored in symlink inode
- Short targets stored inline (local format)
- Long targets stored in data blocks (extent format)
- Can link to non-existent targets (dangling)
- Can link to directories
- Relative or absolute paths allowed

## Transaction Safety

All write operations in fusexfs use XFS's transaction system to ensure filesystem consistency.

### How Transactions Work

1. **Allocate Transaction** - Reserve resources for the operation
2. **Join Inodes** - Lock affected inodes to the transaction
3. **Perform Operation** - Make the actual changes
4. **Log Changes** - Record changes in the journal
5. **Commit** - Atomically commit all changes

### Crash Recovery

If a crash occurs:
- Uncommitted transactions are rolled back
- Committed transactions are replayed from the journal
- Filesystem remains consistent

### Example Transaction Flow

```
create("/mnt/xfs/newfile.txt")
  → xfs_trans_alloc()       # Allocate transaction
  → xfs_inode_alloc()       # Allocate new inode
  → xfs_dir_createname()    # Add directory entry
  → xfs_trans_log_inode()   # Log changes
  → xfs_trans_commit()      # Commit atomically
```

## Error Handling

Write operations return standard POSIX error codes:

| Error | Code | Description |
|-------|------|-------------|
| `EROFS` | 30 | Filesystem is read-only |
| `ENOENT` | 2 | File or directory not found |
| `EEXIST` | 17 | File already exists |
| `ENOTDIR` | 20 | Not a directory |
| `EISDIR` | 21 | Is a directory |
| `ENOTEMPTY` | 66 | Directory not empty |
| `ENOSPC` | 28 | No space left on device |
| `ENOMEM` | 12 | Out of memory |
| `EIO` | 5 | I/O error |
| `EINVAL` | 22 | Invalid argument |
| `EMLINK` | 31 | Too many links |
| `ENAMETOOLONG` | 63 | File name too long |
| `EPERM` | 1 | Operation not permitted |

### Common Error Scenarios

**EROFS - Read-Only Filesystem**
```bash
$ touch /mount/point/newfile
touch: cannot touch '/mount/point/newfile': Read-only file system
```
Solution: Mount with `-rw` flag.

**ENOTEMPTY - Directory Not Empty**
```bash
$ rmdir /mount/point/dir
rmdir: failed to remove '/mount/point/dir': Directory not empty
```
Solution: Remove contents first or use `rm -r`.

**ENOSPC - No Space Left**
```bash
$ dd if=/dev/zero of=/mount/point/file bs=1G count=100
dd: error writing '/mount/point/file': No space left on device
```
Solution: Free up space or use a larger filesystem.

## Performance Considerations

### Write Performance Tips

1. **Batch Operations** - Group multiple small writes together when possible
2. **Sequential Writes** - Sequential writes are more efficient than random
3. **Proper fsync Usage** - Don't fsync after every small write
4. **Appropriate Block Size** - Use block-aligned I/O when possible

### Block Allocation

- XFS pre-allocates extents for better performance
- Large sequential writes are more efficient
- Sparse files are supported (holes don't consume disk space)

### Buffer Cache

- Write data is buffered before disk write
- fsync forces buffer flush to disk
- Unmount flushes all pending data

## Known Limitations

### Unsupported Features

| Feature | Status | Notes |
|---------|--------|-------|
| Extended Attributes (write) | ❌ Not Supported | Read-only xattr access |
| Access Control Lists (ACLs) | ❌ Not Supported | POSIX permissions only |
| Quotas | ❌ Not Supported | No quota enforcement |
| Real-time Devices | ❌ Not Supported | Cannot mount RT filesystems |
| External Logs | ❌ Not Supported | Internal log required |
| Reflinks | ❌ Not Supported | No copy-on-write |

### Implementation Limits

- Maximum filename length: 255 bytes
- Maximum path length: 1024 bytes
- Maximum file size: XFS limits (16 EiB theoretical)
- Maximum hard links per file: 65,000

### Known Issues

1. **Extended attributes** - Write operations for xattrs are not implemented
2. **ACLs** - Access control lists beyond standard permissions not supported
3. **Special mounts** - XFS filesystems with external logs or RT sections cannot be mounted

## Testing Write Operations

A comprehensive test suite is available in the `tests/` directory:

```bash
# Run all tests
cd tests
./run_tests.sh

# Run with verbose output
./run_tests.sh -v

# Test specific operations
./test_write_operations.sh /path/to/xfs.img /mount/point
```

See [tests/README.md](tests/README.md) for detailed testing documentation.

## Troubleshooting

### "Read-only file system" Error

```bash
$ touch /mount/point/file
touch: cannot touch '/mount/point/file': Read-only file system
```

**Solution:** Mount with the `-rw` flag:
```bash
fusexfs -rw /path/to/xfs.img /mount/point
```

### "Operation not permitted" Error

```bash
$ mknod /mount/point/dev c 1 3
mknod: cannot create '/mount/point/dev': Operation not permitted
```

**Solution:** Run as root for device node creation:
```bash
sudo mknod /mount/point/dev c 1 3
```

### Changes Not Persisted

**Solution:** Ensure proper unmount:
```bash
# macOS
diskutil unmount /mount/point

# Linux
fusermount -u /mount/point
```

### Filesystem Corruption After Crash

**Solution:** Run XFS repair on Linux:
```bash
# Check only
xfs_repair -n /path/to/xfs.img

# Repair
xfs_repair /path/to/xfs.img
```

## API Reference

For developers integrating with the xfsutil library, see [API.md](API.md) for the complete API reference.

## Design Documentation

For implementation details and architectural decisions, see [WRITE_OPERATIONS_DESIGN.md](WRITE_OPERATIONS_DESIGN.md).
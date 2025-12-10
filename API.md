# XFSUtil API Reference

This document provides comprehensive documentation for the xfsutil API functions used in fusexfs.

## Overview

The xfsutil library provides a high-level interface for XFS filesystem operations, sitting between the FUSE layer and the low-level libxfs library. All functions are declared in [`src/xfsutil/xfsutil.h`](src/xfsutil/xfsutil.h).

## Table of Contents

- [Mount Operations](#mount-operations)
- [Attribute Operations](#attribute-operations)
- [File Operations](#file-operations)
- [Directory Operations](#directory-operations)
- [Link Operations](#link-operations)
- [Utility Functions](#utility-functions)

---

## Mount Operations

### mount_xfs()

Mount an XFS filesystem in read-only mode.

```c
xfs_mount_t *mount_xfs(char *progname, char *source_name);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `progname` | `char *` | Program name for error messages |
| `source_name` | `char *` | Path to XFS image file or block device |

**Returns:**
- `xfs_mount_t *` - Pointer to mount structure on success
- `NULL` - On failure

**Description:**

Mounts an XFS filesystem in read-only mode. This is a compatibility wrapper that calls `mount_xfs_ex()` with `readonly=1`.

**Example:**
```c
xfs_mount_t *mp = mount_xfs("fusexfs", "/path/to/xfs.img");
if (mp == NULL) {
    fprintf(stderr, "Failed to mount filesystem\n");
    exit(1);
}
```

**Errors:**
- Filesystem has external log
- Filesystem has real-time section
- Filesystem is in progress (mkfs incomplete)
- I/O error reading superblock

---

### mount_xfs_ex()

Mount an XFS filesystem with explicit read-only flag.

```c
xfs_mount_t *mount_xfs_ex(char *progname, char *source_name, int readonly);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `progname` | `char *` | Program name for error messages |
| `source_name` | `char *` | Path to XFS image file or block device |
| `readonly` | `int` | Non-zero for read-only mount, 0 for read-write |

**Returns:**
- `xfs_mount_t *` - Pointer to mount structure on success
- `NULL` - On failure

**Description:**

Mounts an XFS filesystem with the specified access mode. When `readonly` is 0, write operations are enabled.

**Example:**
```c
// Mount read-write
xfs_mount_t *mp = mount_xfs_ex("fusexfs", "/path/to/xfs.img", 0);
if (mp == NULL) {
    fprintf(stderr, "Failed to mount filesystem\n");
    exit(1);
}
```

**Errors:**
- Same as `mount_xfs()`

---

### unmount_xfs()

Unmount an XFS filesystem.

```c
int unmount_xfs(xfs_mount_t *mp);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `mp` | `xfs_mount_t *` | Mount structure to unmount |

**Returns:**
- `0` - Success
- Negative errno - On failure

**Description:**

Flushes all pending data and unmounts the filesystem. Should be called during cleanup.

**Example:**
```c
int error = unmount_xfs(mp);
if (error) {
    fprintf(stderr, "Unmount failed: %d\n", error);
}
```

---

### xfs_is_readonly()

Check if filesystem is mounted read-only.

```c
int xfs_is_readonly(xfs_mount_t *mp);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `mp` | `xfs_mount_t *` | Mount structure |

**Returns:**
- Non-zero - Filesystem is read-only
- `0` - Filesystem is read-write

**Description:**

Checks the mount flags to determine if the filesystem is mounted read-only.

**Example:**
```c
if (xfs_is_readonly(mp)) {
    return -EROFS;
}
```

---

## Attribute Operations

### xfs_setattr_mode()

Change file permissions.

```c
int xfs_setattr_mode(xfs_inode_t *ip, mode_t mode);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `ip` | `xfs_inode_t *` | Inode to modify |
| `mode` | `mode_t` | New permission bits |

**Returns:**
- `0` - Success
- `-ENOMEM` - Transaction allocation failed
- Other negative errno - On failure

**Description:**

Changes the permission bits of a file or directory. Preserves the file type bits (S_IFMT). Automatically updates the ctime.

**Example:**
```c
xfs_inode_t *ip;
int error = find_path(mp, "/path/to/file", &ip);
if (error == 0) {
    error = xfs_setattr_mode(ip, 0755);
    libxfs_iput(ip, 0);
}
```

**Errors:**
- `-ENOMEM` - Out of memory
- `-EIO` - I/O error during commit

---

### xfs_setattr_owner()

Change file ownership.

```c
int xfs_setattr_owner(xfs_inode_t *ip, uid_t uid, gid_t gid);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `ip` | `xfs_inode_t *` | Inode to modify |
| `uid` | `uid_t` | New owner ID (-1 to leave unchanged) |
| `gid` | `gid_t` | New group ID (-1 to leave unchanged) |

**Returns:**
- `0` - Success
- Negative errno - On failure

**Description:**

Changes the owner and/or group of a file. Either `uid` or `gid` can be -1 to leave that value unchanged. Clears setuid/setgid bits when ownership changes.

**Example:**
```c
// Change owner only
error = xfs_setattr_owner(ip, 1000, (gid_t)-1);

// Change both owner and group
error = xfs_setattr_owner(ip, 1000, 1000);
```

---

### xfs_setattr_time()

Update file timestamps.

```c
int xfs_setattr_time(xfs_inode_t *ip,
                     const struct timespec *atime,
                     const struct timespec *mtime);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `ip` | `xfs_inode_t *` | Inode to modify |
| `atime` | `const struct timespec *` | New access time (NULL to skip) |
| `mtime` | `const struct timespec *` | New modification time (NULL to skip) |

**Returns:**
- `0` - Success
- Negative errno - On failure

**Description:**

Updates the access time and/or modification time of a file. Supports special values:
- `UTIME_NOW` - Set to current time
- `UTIME_OMIT` - Leave unchanged

Ctime is always updated.

**Example:**
```c
struct timespec times[2];
times[0].tv_nsec = UTIME_NOW;  // atime = now
times[1].tv_sec = 1234567890;  // mtime = specific time
times[1].tv_nsec = 0;

error = xfs_setattr_time(ip, &times[0], &times[1]);
```

---

### xfs_truncate_file()

Change file size.

```c
int xfs_truncate_file(xfs_inode_t *ip, off_t size);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `ip` | `xfs_inode_t *` | File inode |
| `size` | `off_t` | New file size |

**Returns:**
- `0` - Success
- `-EINVAL` - Not a regular file
- `-ENOMEM` - Out of memory
- Other negative errno - On failure

**Description:**

Truncates a file to the specified size. If size is less than current size, blocks beyond the new size are freed. If size is greater, the file is extended (creating a sparse region).

**Example:**
```c
// Truncate to zero
error = xfs_truncate_file(ip, 0);

// Extend file
error = xfs_truncate_file(ip, 1024 * 1024);
```

---

## File Operations

### xfs_create_file()

Create a new file.

```c
int xfs_create_file(xfs_mount_t *mp, xfs_inode_t *dp, const char *name,
                    mode_t mode, dev_t rdev, xfs_inode_t **ipp);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `mp` | `xfs_mount_t *` | Mount structure |
| `dp` | `xfs_inode_t *` | Parent directory inode |
| `name` | `const char *` | Name of new file |
| `mode` | `mode_t` | File mode (permissions + type) |
| `rdev` | `dev_t` | Device number (for block/char devices) |
| `ipp` | `xfs_inode_t **` | Output: newly created inode |

**Returns:**
- `0` - Success
- `-ENOTDIR` - Parent is not a directory
- `-ENOMEM` - Out of memory
- `-ENOSPC` - No space left
- Other negative errno - On failure

**Description:**

Creates a new file in the specified directory. The `mode` parameter includes both permission bits and file type (S_IFREG, S_IFBLK, S_IFCHR, S_IFIFO, or S_IFSOCK).

The caller is responsible for releasing the returned inode with `libxfs_iput()`.

**Example:**
```c
xfs_inode_t *dp, *ip;

// Look up parent directory
error = find_path(mp, "/parent", &dp);
if (error) return error;

// Create regular file
error = xfs_create_file(mp, dp, "newfile.txt", 
                        S_IFREG | 0644, 0, &ip);

libxfs_iput(dp, 0);

if (error == 0) {
    // Use the new file...
    libxfs_iput(ip, 0);
}
```

---

### xfs_write_file()

Write data to a file.

```c
ssize_t xfs_write_file(xfs_inode_t *ip, const char *buf, 
                       off_t offset, size_t size);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `ip` | `xfs_inode_t *` | File inode |
| `buf` | `const char *` | Data buffer to write |
| `offset` | `off_t` | Offset in file |
| `size` | `size_t` | Number of bytes to write |

**Returns:**
- Positive value - Number of bytes written
- Negative errno - On failure

**Description:**

Writes data to a file at the specified offset. Allocates blocks as needed and extends the file size if writing beyond the current end.

**Example:**
```c
const char *data = "Hello, XFS!";
ssize_t written = xfs_write_file(ip, data, 0, strlen(data));
if (written < 0) {
    fprintf(stderr, "Write failed: %zd\n", written);
}
```

---

### xfs_remove_file()

Remove a file (unlink).

```c
int xfs_remove_file(xfs_mount_t *mp, xfs_inode_t *dp, const char *name,
                    xfs_inode_t *ip);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `mp` | `xfs_mount_t *` | Mount structure |
| `dp` | `xfs_inode_t *` | Parent directory inode |
| `name` | `const char *` | Name of file to remove |
| `ip` | `xfs_inode_t *` | File inode (optional, NULL to look up) |

**Returns:**
- `0` - Success
- `-ENOTDIR` - Parent is not a directory
- `-ENOENT` - File not found
- `-EISDIR` - Target is a directory (use rmdir)
- Other negative errno - On failure

**Description:**

Removes a file from a directory. Decrements the link count and frees the inode if it reaches zero. Cannot be used on directories.

**Example:**
```c
error = xfs_remove_file(mp, parent_dp, "file.txt", NULL);
```

---

### xfs_sync_file()

Synchronize file data to disk.

```c
int xfs_sync_file(xfs_inode_t *ip);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `ip` | `xfs_inode_t *` | File inode |

**Returns:**
- `0` - Success
- Negative errno - On failure

**Description:**

Flushes all pending data and metadata for the file to disk.

**Example:**
```c
error = xfs_sync_file(ip);
```

---

### xfs_sync_fs()

Synchronize entire filesystem.

```c
int xfs_sync_fs(xfs_mount_t *mp);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `mp` | `xfs_mount_t *` | Mount structure |

**Returns:**
- `0` - Success
- Negative errno - On failure

**Description:**

Flushes all pending data and metadata for the entire filesystem to disk.

---

## Directory Operations

### xfs_create_dir()

Create a new directory.

```c
int xfs_create_dir(xfs_mount_t *mp, xfs_inode_t *dp, const char *name,
                   mode_t mode, xfs_inode_t **ipp);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `mp` | `xfs_mount_t *` | Mount structure |
| `dp` | `xfs_inode_t *` | Parent directory inode |
| `name` | `const char *` | Name of new directory |
| `mode` | `mode_t` | Directory permissions |
| `ipp` | `xfs_inode_t **` | Output: newly created directory inode |

**Returns:**
- `0` - Success
- `-ENOTDIR` - Parent is not a directory
- `-ENOMEM` - Out of memory
- `-ENOSPC` - No space left
- Other negative errno - On failure

**Description:**

Creates a new directory with . and .. entries. The new directory has a link count of 2 (self-reference via .), and the parent's link count is incremented.

**Example:**
```c
xfs_inode_t *newdir;
error = xfs_create_dir(mp, parent, "newdir", 0755, &newdir);
if (error == 0) {
    libxfs_iput(newdir, 0);
}
```

---

### xfs_remove_dir()

Remove an empty directory.

```c
int xfs_remove_dir(xfs_mount_t *mp, xfs_inode_t *dp, const char *name,
                   xfs_inode_t *ip);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `mp` | `xfs_mount_t *` | Mount structure |
| `dp` | `xfs_inode_t *` | Parent directory inode |
| `name` | `const char *` | Name of directory to remove |
| `ip` | `xfs_inode_t *` | Directory inode (optional, NULL to look up) |

**Returns:**
- `0` - Success
- `-ENOTDIR` - Target is not a directory
- `-ENOTEMPTY` - Directory is not empty
- Other negative errno - On failure

**Description:**

Removes an empty directory. The directory must contain only . and .. entries. Decrements the parent's link count.

**Example:**
```c
error = xfs_remove_dir(mp, parent, "emptydir", NULL);
if (error == -ENOTEMPTY) {
    fprintf(stderr, "Directory not empty\n");
}
```

---

### xfs_rename_entry()

Rename a file or directory.

```c
int xfs_rename_entry(xfs_mount_t *mp, xfs_inode_t *src_dp,
                     const char *src_name, xfs_inode_t *dst_dp,
                     const char *dst_name);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `mp` | `xfs_mount_t *` | Mount structure |
| `src_dp` | `xfs_inode_t *` | Source parent directory |
| `src_name` | `const char *` | Source name |
| `dst_dp` | `xfs_inode_t *` | Destination parent directory |
| `dst_name` | `const char *` | Destination name |

**Returns:**
- `0` - Success
- `-ENOENT` - Source not found
- `-EISDIR` / `-ENOTDIR` - Type mismatch with existing destination
- `-ENOTEMPTY` - Destination directory not empty
- Other negative errno - On failure

**Description:**

Renames or moves a file or directory. Can overwrite an existing destination if types are compatible. When renaming directories, updates the .. entry and link counts appropriately.

**Example:**
```c
// Same directory rename
error = xfs_rename_entry(mp, dir, "oldname", dir, "newname");

// Move to different directory
error = xfs_rename_entry(mp, src_dir, "file", dst_dir, "file");
```

---

## Link Operations

### xfs_create_link()

Create a hard link.

```c
int xfs_create_link(xfs_mount_t *mp, xfs_inode_t *ip, xfs_inode_t *newparent,
                    const char *newname);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `mp` | `xfs_mount_t *` | Mount structure |
| `ip` | `xfs_inode_t *` | Existing inode to link to |
| `newparent` | `xfs_inode_t *` | Parent directory for new link |
| `newname` | `const char *` | Name of new link |

**Returns:**
- `0` - Success
- `-ENOTDIR` - Parent is not a directory
- `-EPERM` - Cannot hard link directories
- `-EMLINK` - Maximum link count exceeded
- Other negative errno - On failure

**Description:**

Creates a hard link to an existing file. Both the original name and the new link point to the same inode. Cannot create hard links to directories.

**Example:**
```c
xfs_inode_t *ip;
error = find_path(mp, "/original", &ip);
if (error == 0) {
    error = xfs_create_link(mp, ip, parent_dir, "hardlink");
    libxfs_iput(ip, 0);
}
```

---

### xfs_create_symlink()

Create a symbolic link.

```c
int xfs_create_symlink(xfs_mount_t *mp, xfs_inode_t *parent, const char *name,
                       const char *target, xfs_inode_t **ipp);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `mp` | `xfs_mount_t *` | Mount structure |
| `parent` | `xfs_inode_t *` | Parent directory inode |
| `name` | `const char *` | Name of the symlink |
| `target` | `const char *` | Target path |
| `ipp` | `xfs_inode_t **` | Output: newly created symlink inode |

**Returns:**
- `0` - Success
- `-ENOTDIR` - Parent is not a directory
- `-ENAMETOOLONG` - Target path too long
- Other negative errno - On failure

**Description:**

Creates a symbolic link pointing to the target path. The target can be a relative or absolute path and can point to non-existent files. Short targets are stored inline; long targets are stored in data blocks.

**Example:**
```c
xfs_inode_t *link_ip;
error = xfs_create_symlink(mp, parent, "mylink", "../target.txt", &link_ip);
if (error == 0) {
    libxfs_iput(link_ip, 0);
}
```

---

## Utility Functions

### find_path()

Look up an inode by path.

```c
int find_path(xfs_mount_t *mp, const char *path, xfs_inode_t **result);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `mp` | `xfs_mount_t *` | Mount structure |
| `path` | `const char *` | Path to look up |
| `result` | `xfs_inode_t **` | Output: found inode |

**Returns:**
- `0` - Success
- Non-zero - Path not found

**Description:**

Traverses the path and returns the inode. Caller must release the inode with `libxfs_iput()`.

**Example:**
```c
xfs_inode_t *ip;
int error = find_path(mp, "/some/path/file.txt", &ip);
if (error == 0) {
    // Use inode...
    libxfs_iput(ip, 0);
}
```

---

### xfs_path_split()

Split a path into parent and filename.

```c
int xfs_path_split(const char *path, char **parent, char **name);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `const char *` | Full path |
| `parent` | `char **` | Output: parent directory path |
| `name` | `char **` | Output: filename |

**Returns:**
- `0` - Success
- Negative errno - On failure

**Description:**

Splits a path into parent directory and filename components. Caller must free both output strings.

**Example:**
```c
char *parent, *name;
error = xfs_path_split("/path/to/file.txt", &parent, &name);
// parent = "/path/to"
// name = "file.txt"
free(parent);
free(name);
```

---

### xfs_lookup_parent()

Look up parent directory by path.

```c
int xfs_lookup_parent(xfs_mount_t *mp, const char *path,
                      xfs_inode_t **parent_ip, char *name, size_t name_size);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `mp` | `xfs_mount_t *` | Mount structure |
| `path` | `const char *` | Full path |
| `parent_ip` | `xfs_inode_t **` | Output: parent directory inode |
| `name` | `char *` | Buffer for filename |
| `name_size` | `size_t` | Size of name buffer |

**Returns:**
- `0` - Success
- `-ENOENT` - Parent directory not found
- `-ENAMETOOLONG` - Filename too long
- Other negative errno - On failure

**Description:**

Combined operation that splits the path and looks up the parent directory inode. The caller must release the parent inode with `libxfs_iput()`.

**Example:**
```c
xfs_inode_t *parent;
char name[MAXNAMELEN + 1];

error = xfs_lookup_parent(mp, "/path/to/file.txt", &parent, name, sizeof(name));
if (error == 0) {
    // name = "file.txt", parent = inode of /path/to
    // ... perform operation ...
    libxfs_iput(parent, 0);
}
```

---

### xfs_stat()

Get file statistics.

```c
int xfs_stat(xfs_inode_t *inode, struct stat *stats);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `inode` | `xfs_inode_t *` | Inode to stat |
| `stats` | `struct stat *` | Output: file statistics |

**Returns:**
- `0` - Success
- Non-zero - On failure

**Description:**

Fills in the stat structure with file information (size, mode, uid, gid, timestamps, etc.).

---

### xfs_is_dir() / xfs_is_link() / xfs_is_regular()

Check file type.

```c
int xfs_is_dir(xfs_inode_t *inode);
int xfs_is_link(xfs_inode_t *inode);
int xfs_is_regular(xfs_inode_t *inode);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `inode` | `xfs_inode_t *` | Inode to check |

**Returns:**
- Non-zero - True (is the specified type)
- `0` - False

---

### xfs_readfile()

Read data from a file.

```c
int xfs_readfile(xfs_inode_t *ip, void *buffer, off_t offset, 
                 size_t len, int *last_extent);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `ip` | `xfs_inode_t *` | File inode |
| `buffer` | `void *` | Buffer to read into |
| `offset` | `off_t` | Offset in file |
| `len` | `size_t` | Number of bytes to read |
| `last_extent` | `int *` | Output: last extent hint (can be NULL) |

**Returns:**
- Positive value - Number of bytes read
- `0` - End of file
- Negative value - Error

---

### xfs_readdir()

Read directory entries.

```c
int xfs_readdir(xfs_inode_t *dp, void *dirent, size_t bufsize,
                xfs_off_t *offset, filldir_t filldir);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `dp` | `xfs_inode_t *` | Directory inode |
| `dirent` | `void *` | User data for callback |
| `bufsize` | `size_t` | Buffer size hint |
| `offset` | `xfs_off_t *` | Starting offset |
| `filldir` | `filldir_t` | Callback function |

**Returns:**
- `0` - Success
- Non-zero - Error or callback requested stop

---

### xfs_readlink()

Read symbolic link target.

```c
int xfs_readlink(xfs_inode_t *ip, void *buffer, off_t offset, 
                 size_t len, int *last_extent);
```

**Parameters:**
| Parameter | Type | Description |
|-----------|------|-------------|
| `ip` | `xfs_inode_t *` | Symlink inode |
| `buffer` | `void *` | Buffer for target path |
| `offset` | `off_t` | Starting offset |
| `len` | `size_t` | Buffer size |
| `last_extent` | `int *` | Output: extent hint (can be NULL) |

**Returns:**
- Positive value - Length of target path
- Negative value - Error

---

## Error Handling

All functions return negative errno values on failure. Common errors:

| Error | Value | Description |
|-------|-------|-------------|
| `-ENOENT` | -2 | File or directory not found |
| `-EIO` | -5 | I/O error |
| `-ENOMEM` | -12 | Out of memory |
| `-EEXIST` | -17 | File exists |
| `-ENOTDIR` | -20 | Not a directory |
| `-EISDIR` | -21 | Is a directory |
| `-EINVAL` | -22 | Invalid argument |
| `-ENOSPC` | -28 | No space left |
| `-EROFS` | -30 | Read-only filesystem |
| `-EMLINK` | -31 | Too many links |
| `-ENAMETOOLONG` | -63 | Filename too long |
| `-ENOTEMPTY` | -66 | Directory not empty |

## Thread Safety

The xfsutil functions are **not** thread-safe. External synchronization is required when using from multiple threads. The FUSE layer handles this by serializing operations.

## Memory Management

- **Inodes**: Functions that return inodes require the caller to release them with `libxfs_iput(inode, 0)`.
- **Strings**: Functions that allocate strings (like `xfs_path_split()`) require the caller to `free()` them.
- **Mount structures**: Release with `unmount_xfs()` or `libxfs_umount()`.

## See Also

- [WRITE_SUPPORT.md](WRITE_SUPPORT.md) - User documentation for write operations
- [WRITE_OPERATIONS_DESIGN.md](WRITE_OPERATIONS_DESIGN.md) - Design documentation
- [src/xfsutil/xfsutil.h](src/xfsutil/xfsutil.h) - Header file with declarations
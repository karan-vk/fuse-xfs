# FuseXFS Write Operations Design Document

## Implementation Status

> **✅ IMPLEMENTATION COMPLETE** - All phases of write operation support have been successfully implemented. This document now serves as both design documentation and implementation reference.

| Phase | Description | Status |
|-------|-------------|--------|
| Phase 1 | Foundation Operations (chmod, chown, utimens, truncate, fsync) | ✅ Complete |
| Phase 2 | File I/O Operations (create, write, mknod, unlink) | ✅ Complete |
| Phase 3 | Directory Operations (mkdir, rmdir, rename) | ✅ Complete |
| Phase 4 | Link Operations (link, symlink) | ✅ Complete |

---

## Executive Summary

This document provides a comprehensive design for implementing write operation support in the fusexfs project. The implementation has successfully transformed fusexfs from a read-only XFS filesystem accessor to a fully functional read-write FUSE filesystem.

---

## Table of Contents

1. [Architecture Design](#1-architecture-design)
   - [Overview](#11-overview)
   - [XFSUtil API Extensions](#12-xfsutil-api-extensions)
   - [Transaction Management](#13-transaction-management)
   - [Locking Strategy](#14-locking-strategy)
   - [Buffer Cache Management](#15-buffer-cache-management)
2. [Phase 1: Foundation Operations](#2-phase-1-foundation-operations)
3. [Phase 2: File I/O Operations](#3-phase-2-file-io-operations)
4. [Phase 3: Directory Operations](#4-phase-3-directory-operations)
5. [Phase 4: Link Operations](#5-phase-4-link-operations)
6. [Mount Configuration Changes](#6-mount-configuration-changes)
7. [Header File Design](#7-header-file-design)
8. [Testing Strategy](#8-testing-strategy)
9. [Implementation Phases and Timeline](#9-implementation-phases-and-timeline)

---

## 1. Architecture Design

### 1.1 Overview

The write operation support architecture consists of three layers:

```
┌─────────────────────────────────────────────────────────────┐
│                    FUSE Interface Layer                      │
│                   (src/fuse/fuse_xfs.c)                      │
│     Handles FUSE callbacks, permission checks, path lookup   │
├─────────────────────────────────────────────────────────────┤
│                    XFSUtil Wrapper Layer                     │
│                   (src/xfsutil/xfsutil.c)                    │
│    High-level operations, transaction coordination, caching  │
├─────────────────────────────────────────────────────────────┤
│                      LibXFS Layer                            │
│                 (src/xfsprogs/libxfs/*)                      │
│   Low-level XFS operations, on-disk format handling          │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 XFSUtil API Extensions

New functions to be added to [`xfsutil.h`](src/xfsutil/xfsutil.h):

#### 1.2.1 Inode Attribute Operations

```c
/* Change file mode (permissions) */
int xfs_setattr_mode(xfs_inode_t *ip, mode_t mode);

/* Change file ownership */
int xfs_setattr_owner(xfs_inode_t *ip, uid_t uid, gid_t gid);

/* Update file timestamps */
int xfs_setattr_time(xfs_inode_t *ip, 
                     const struct timespec *atime,
                     const struct timespec *mtime);

/* Truncate file to specified size */
int xfs_truncate_file(xfs_inode_t *ip, off_t size);
```

#### 1.2.2 File Creation and Deletion

```c
/* Create a new regular file */
int xfs_create_file(xfs_mount_t *mp, xfs_inode_t *dp, 
                    const char *name, mode_t mode,
                    xfs_inode_t **ipp);

/* Create a special device node */
int xfs_create_node(xfs_mount_t *mp, xfs_inode_t *dp,
                    const char *name, mode_t mode, dev_t rdev,
                    xfs_inode_t **ipp);

/* Remove a file (unlink) */
int xfs_remove_file(xfs_mount_t *mp, xfs_inode_t *dp,
                    const char *name);
```

#### 1.2.3 Directory Operations

```c
/* Create a new directory */
int xfs_create_dir(xfs_mount_t *mp, xfs_inode_t *dp,
                   const char *name, mode_t mode,
                   xfs_inode_t **ipp);

/* Remove an empty directory */
int xfs_remove_dir(xfs_mount_t *mp, xfs_inode_t *dp,
                   const char *name);

/* Rename a file or directory */
int xfs_rename_entry(xfs_mount_t *mp,
                     xfs_inode_t *src_dp, const char *src_name,
                     xfs_inode_t *dst_dp, const char *dst_name);
```

#### 1.2.4 Link Operations

```c
/* Create a hard link */
int xfs_create_link(xfs_mount_t *mp, xfs_inode_t *dp,
                    const char *name, xfs_inode_t *ip);

/* Create a symbolic link */
int xfs_create_symlink(xfs_mount_t *mp, xfs_inode_t *dp,
                       const char *name, const char *target);
```

#### 1.2.5 File Write Operations

```c
/* Write data to a file */
int xfs_writefile(xfs_inode_t *ip, const void *buffer,
                  off_t offset, size_t len, size_t *written);

/* Synchronize file data to disk */
int xfs_sync_file(xfs_inode_t *ip, int datasync);

/* Synchronize entire filesystem */
int xfs_sync_fs(xfs_mount_t *mp);
```

#### 1.2.6 Mount Control

```c
/* Mount filesystem with read-write support */
xfs_mount_t *mount_xfs_rw(char *progname, char *source_name, int readonly);

/* Unmount with proper flush */
int unmount_xfs(xfs_mount_t *mp);
```

### 1.3 Transaction Management

XFS uses transactions to ensure filesystem consistency. Every metadata modification must be wrapped in a transaction.

#### 1.3.1 Transaction Pattern

Based on the pattern used in [`proto.c`](src/xfsprogs/mkfs/proto.c:187-209):

```c
/*
 * Standard transaction pattern for metadata operations:
 *
 * 1. Allocate transaction
 * 2. Reserve space (blocks and log)
 * 3. Lock inodes (join to transaction)
 * 4. Perform operation
 * 5. Log changes
 * 6. Handle deferred operations (bmap_finish)
 * 7. Commit transaction
 */

int example_metadata_operation(xfs_mount_t *mp, xfs_inode_t *ip) {
    xfs_trans_t     *tp;
    xfs_bmap_free_t flist;
    xfs_fsblock_t   first;
    int             committed;
    int             error;

    /* Step 1: Allocate transaction */
    tp = libxfs_trans_alloc(mp, XFS_TRANS_ICHANGE);
    if (!tp)
        return -ENOMEM;

    /* Step 2: Reserve space */
    error = libxfs_trans_reserve(tp, 0, 
                                 mp->m_reservations.tr_ichange,
                                 0, 0, 0);
    if (error) {
        libxfs_trans_cancel(tp, 0);
        return -error;
    }

    /* Step 3: Lock and join inode to transaction */
    libxfs_trans_ijoin(tp, ip, 0);
    libxfs_trans_ihold(tp, ip);

    /* Step 4: Perform operation */
    XFS_BMAP_INIT(&flist, &first);
    /* ... actual modifications ... */

    /* Step 5: Log inode changes */
    libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

    /* Step 6: Handle deferred operations */
    error = libxfs_bmap_finish(&tp, &flist, &committed);
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    /* Step 7: Commit transaction */
    error = libxfs_trans_commit(tp, 0);
    return error ? -error : 0;
}
```

#### 1.3.2 Transaction Reservations

From [`xfs_mount.h`](src/xfsprogs/include/xfs_mount.h:22-44), use these pre-calculated reservations:

| Operation | Reservation Field | Usage |
|-----------|------------------|-------|
| Write | `tr_write` | Extent allocation |
| Truncate | `tr_itruncate` | File truncation |
| Rename | `tr_rename` | Rename operation |
| Link | `tr_link` | Hard link creation |
| Unlink | `tr_remove` | File removal |
| Symlink | `tr_symlink` | Symbolic link creation |
| Create | `tr_create` | File creation |
| Mkdir | `tr_mkdir` | Directory creation |
| Inode change | `tr_ichange` | Attribute modifications |

### 1.4 Locking Strategy

#### 1.4.1 Inode Locking Hierarchy

From [`xfs_inode.h`](src/xfsprogs/include/xfs_inode.h:431-435):

```c
/* Lock types (bitfield) */
#define XFS_IOLOCK_EXCL     (1<<0)  /* Exclusive I/O lock */
#define XFS_IOLOCK_SHARED   (1<<1)  /* Shared I/O lock */
#define XFS_ILOCK_EXCL      (1<<2)  /* Exclusive inode lock */
#define XFS_ILOCK_SHARED    (1<<3)  /* Shared inode lock */
```

#### 1.4.2 Lock Ordering Rules

1. **I/O Lock** must be acquired before **Inode Lock**
2. **Parent directory** lock before **child** lock
3. When locking multiple inodes, lock in **inode number order** to prevent deadlocks

#### 1.4.3 FUSE-Level Locking

Since FUSE may call handlers concurrently, implement:

```c
/* Global filesystem lock for metadata operations */
typedef struct xfs_fuse_lock {
    pthread_rwlock_t    fs_lock;        /* R/W lock for fs-wide ops */
    pthread_mutex_t     mount_lock;     /* Protects mount state */
} xfs_fuse_lock_t;

/* Initialize locks */
int xfs_init_locks(xfs_mount_t *mp);

/* Destroy locks on unmount */
void xfs_destroy_locks(xfs_mount_t *mp);
```

#### 1.4.4 Operation Lock Requirements

| Operation | I/O Lock | Inode Lock | Parent Lock |
|-----------|----------|------------|-------------|
| read | Shared | - | - |
| write | Exclusive | - | - |
| getattr | - | Shared | - |
| setattr | - | Exclusive | - |
| create | - | - | Exclusive |
| unlink | - | Exclusive | Exclusive |
| mkdir | - | - | Exclusive |
| rmdir | - | Exclusive | Exclusive |
| rename | - | Exclusive | Both Exclusive |

### 1.5 Buffer Cache Management

#### 1.5.1 Buffer Operations

LibXFS provides buffer management through:

```c
/* Read buffer from disk */
xfs_buf_t *libxfs_readbuf(dev_t dev, xfs_daddr_t blkno, 
                          int len, int flags);

/* Get buffer (may not be initialized) */
xfs_buf_t *libxfs_getbuf(dev_t dev, xfs_daddr_t blkno, int len);

/* Write buffer to disk */
int libxfs_writebuf(xfs_buf_t *bp, int flags);

/* Release buffer */
void libxfs_putbuf(xfs_buf_t *bp);
```

#### 1.5.2 Transaction Buffer Management

```c
/* Get buffer within transaction */
xfs_buf_t *libxfs_trans_get_buf(xfs_trans_t *tp, dev_t dev,
                                xfs_daddr_t blkno, int len, uint flags);

/* Log buffer changes */
void libxfs_trans_log_buf(xfs_trans_t *tp, xfs_buf_t *bp,
                          uint first, uint last);

/* Release buffer from transaction */
void libxfs_trans_brelse(xfs_trans_t *tp, xfs_buf_t *bp);
```

#### 1.5.3 Write-Back Strategy

```c
/*
 * Buffer states:
 * - Clean: Matches on-disk data
 * - Dirty: Modified, needs write-back
 * - Logged: Part of active transaction, write at commit
 *
 * Write-back triggers:
 * 1. Transaction commit (logged buffers)
 * 2. Explicit fsync
 * 3. Buffer cache pressure
 * 4. Periodic sync (if enabled)
 */

/* Force all dirty buffers to disk */
int xfs_flush_buffers(xfs_mount_t *mp);
```

---

## 2. Phase 1: Foundation Operations

### 2.1 chmod (fuse_xfs_chmod)

**Purpose**: Change file permission mode

**FUSE Signature**:
```c
static int fuse_xfs_chmod(const char *path, mode_t mode);
```

**Implementation in xfsutil**:
```c
int xfs_setattr_mode(xfs_inode_t *ip, mode_t mode) {
    xfs_trans_t *tp;
    int error;

    /* Allocate transaction */
    tp = libxfs_trans_alloc(ip->i_mount, XFS_TRANS_ICHANGE);
    if (!tp)
        return -ENOMEM;

    /* Reserve space for inode change */
    error = libxfs_trans_reserve(tp, 0, 
                                 ip->i_mount->m_reservations.tr_ichange,
                                 0, 0, 0);
    if (error) {
        libxfs_trans_cancel(tp, 0);
        return -error;
    }

    /* Join inode to transaction */
    libxfs_trans_ijoin(tp, ip, 0);
    libxfs_trans_ihold(tp, ip);

    /* Preserve file type bits, update permission bits */
    ip->i_d.di_mode = (ip->i_d.di_mode & S_IFMT) | (mode & ~S_IFMT);

    /* Update ctime */
    libxfs_ichgtime(ip, XFS_ICHGTIME_CHG);

    /* Log the changes */
    libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

    /* Commit transaction */
    error = libxfs_trans_commit(tp, 0);
    return error ? -error : 0;
}
```

**FUSE Handler**:
```c
static int fuse_xfs_chmod(const char *path, mode_t mode) {
    xfs_inode_t *ip = NULL;
    int error;

    log_debug("chmod %s mode=%o\n", path, mode);

    error = find_path(current_xfs_mount(), path, &ip);
    if (error)
        return -ENOENT;

    error = xfs_setattr_mode(ip, mode);
    
    libxfs_iput(ip, 0);
    return error;
}
```

**XFS Library Functions Used**:
- [`libxfs_trans_alloc()`](src/xfsprogs/libxfs/util.c) - Allocate transaction
- [`libxfs_trans_reserve()`](src/xfsprogs/libxfs/util.c) - Reserve space
- [`libxfs_trans_ijoin()`](src/xfsprogs/libxfs/util.c) - Join inode to transaction
- [`libxfs_ichgtime()`](src/xfsprogs/libxfs/util.c:31-47) - Update timestamps
- [`libxfs_trans_log_inode()`](src/xfsprogs/libxfs/util.c) - Log inode changes
- [`libxfs_trans_commit()`](src/xfsprogs/libxfs/util.c) - Commit transaction

**Error Handling**:
- `ENOENT`: Path not found
- `ENOMEM`: Transaction allocation failed
- `EROFS`: Filesystem mounted read-only
- `EPERM`: Operation not permitted (future permission check)

---

### 2.2 chown (fuse_xfs_chown)

**Purpose**: Change file owner and group

**FUSE Signature**:
```c
static int fuse_xfs_chown(const char *path, uid_t uid, gid_t gid);
```

**Implementation in xfsutil**:
```c
int xfs_setattr_owner(xfs_inode_t *ip, uid_t uid, gid_t gid) {
    xfs_trans_t *tp;
    int error;

    tp = libxfs_trans_alloc(ip->i_mount, XFS_TRANS_ICHANGE);
    if (!tp)
        return -ENOMEM;

    error = libxfs_trans_reserve(tp, 0,
                                 ip->i_mount->m_reservations.tr_ichange,
                                 0, 0, 0);
    if (error) {
        libxfs_trans_cancel(tp, 0);
        return -error;
    }

    libxfs_trans_ijoin(tp, ip, 0);
    libxfs_trans_ihold(tp, ip);

    /* Update owner if specified (uid != -1) */
    if (uid != (uid_t)-1)
        ip->i_d.di_uid = uid;

    /* Update group if specified (gid != -1) */
    if (gid != (gid_t)-1)
        ip->i_d.di_gid = gid;

    /* Clear setuid/setgid bits if changing owner */
    if (uid != (uid_t)-1 || gid != (gid_t)-1) {
        ip->i_d.di_mode &= ~(S_ISUID | S_ISGID);
    }

    libxfs_ichgtime(ip, XFS_ICHGTIME_CHG);
    libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

    error = libxfs_trans_commit(tp, 0);
    return error ? -error : 0;
}
```

---

### 2.3 utimens (fuse_xfs_utimens)

**Purpose**: Update file access and modification times

**FUSE Signature**:
```c
static int fuse_xfs_utimens(const char *path, 
                            const struct timespec ts[2]);
```

**Implementation in xfsutil**:
```c
int xfs_setattr_time(xfs_inode_t *ip,
                     const struct timespec *atime,
                     const struct timespec *mtime) {
    xfs_trans_t *tp;
    int error;

    tp = libxfs_trans_alloc(ip->i_mount, XFS_TRANS_ICHANGE);
    if (!tp)
        return -ENOMEM;

    error = libxfs_trans_reserve(tp, 0,
                                 ip->i_mount->m_reservations.tr_ichange,
                                 0, 0, 0);
    if (error) {
        libxfs_trans_cancel(tp, 0);
        return -error;
    }

    libxfs_trans_ijoin(tp, ip, 0);
    libxfs_trans_ihold(tp, ip);

    /* Update atime if provided */
    if (atime) {
        if (atime->tv_nsec == UTIME_NOW) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            ip->i_d.di_atime.t_sec = tv.tv_sec;
            ip->i_d.di_atime.t_nsec = tv.tv_usec * 1000;
        } else if (atime->tv_nsec != UTIME_OMIT) {
            ip->i_d.di_atime.t_sec = atime->tv_sec;
            ip->i_d.di_atime.t_nsec = atime->tv_nsec;
        }
    }

    /* Update mtime if provided */
    if (mtime) {
        if (mtime->tv_nsec == UTIME_NOW) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            ip->i_d.di_mtime.t_sec = tv.tv_sec;
            ip->i_d.di_mtime.t_nsec = tv.tv_usec * 1000;
        } else if (mtime->tv_nsec != UTIME_OMIT) {
            ip->i_d.di_mtime.t_sec = mtime->tv_sec;
            ip->i_d.di_mtime.t_nsec = mtime->tv_nsec;
        }
    }

    /* Always update ctime */
    libxfs_ichgtime(ip, XFS_ICHGTIME_CHG);
    libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

    error = libxfs_trans_commit(tp, 0);
    return error ? -error : 0;
}
```

---

### 2.4 truncate (fuse_xfs_truncate)

**Purpose**: Truncate file to specified length

**FUSE Signature**:
```c
static int fuse_xfs_truncate(const char *path, off_t size);
```

**Algorithm**:
```
1. Look up inode for path
2. Validate that it's a regular file
3. If new_size < current_size:
   a. Start truncate transaction
   b. Free blocks beyond new size
   c. Update di_size
4. If new_size > current_size:
   a. Optionally pre-allocate blocks (sparse file handling)
   b. Update di_size
5. Update timestamps (mtime, ctime)
6. Commit transaction
```

**Implementation in xfsutil**:
```c
int xfs_truncate_file(xfs_inode_t *ip, off_t size) {
    xfs_mount_t     *mp = ip->i_mount;
    xfs_trans_t     *tp;
    xfs_bmap_free_t flist;
    xfs_fsblock_t   first;
    int             committed;
    int             error;

    /* Only regular files can be truncated */
    if (!S_ISREG(ip->i_d.di_mode))
        return -EINVAL;

    tp = libxfs_trans_alloc(mp, XFS_TRANS_ITRUNCATE);
    if (!tp)
        return -ENOMEM;

    error = libxfs_trans_reserve(tp, 0,
                                 mp->m_reservations.tr_itruncate,
                                 0, 0, 0);
    if (error) {
        libxfs_trans_cancel(tp, 0);
        return -error;
    }

    libxfs_trans_ijoin(tp, ip, 0);
    libxfs_trans_ihold(tp, ip);

    XFS_BMAP_INIT(&flist, &first);

    if (size < ip->i_d.di_size) {
        /* Truncating down - free excess blocks */
        error = xfs_itruncate_finish(&tp, ip, size, 
                                     XFS_DATA_FORK, 0);
        if (error) {
            libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
            return -error;
        }
    }

    /* Update size */
    ip->i_d.di_size = size;
    ip->i_size = size;

    /* Update timestamps */
    libxfs_ichgtime(ip, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);

    libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

    error = libxfs_bmap_finish(&tp, &flist, &committed);
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    error = libxfs_trans_commit(tp, 0);
    return error ? -error : 0;
}
```

**XFS Library Functions Used**:
- [`xfs_itruncate_finish()`](src/xfsprogs/include/xfs_inode.h:540-541) - Handle truncation
- [`libxfs_bmap_finish()`](src/xfsprogs/libxfs/util.c:496-519) - Complete extent operations

---

### 2.5 fsync (fuse_xfs_fsync)

**Purpose**: Synchronize file data to disk

**FUSE Signature**:
```c
static int fuse_xfs_fsync(const char *path, int isdatasync,
                          struct fuse_file_info *fi);
```

**Implementation in xfsutil**:
```c
int xfs_sync_file(xfs_inode_t *ip, int datasync) {
    xfs_mount_t *mp = ip->i_mount;
    int error = 0;

    /* Flush inode metadata */
    if (!datasync) {
        error = xfs_iflush(ip, XFS_IFLUSH_SYNC);
        if (error)
            return -error;
    }

    /* Force write of any dirty buffers associated with this inode */
    /* In userspace libxfs, buffers are typically written immediately */
    /* but we should ensure any pending writes are complete */
    
    return 0;
}

int xfs_sync_fs(xfs_mount_t *mp) {
    /* Flush superblock */
    libxfs_writebuf(mp->m_sb_bp, LIBXFS_EXIT_ON_FAILURE);
    
    /* In a full implementation, iterate all dirty inodes and buffers */
    return 0;
}
```

**FUSE Handler**:
```c
static int fuse_xfs_fsync(const char *path, int isdatasync,
                          struct fuse_file_info *fi) {
    xfs_inode_t *ip;
    int error;

    log_debug("fsync %s datasync=%d\n", path, isdatasync);

    if (fi && fi->fh) {
        ip = (xfs_inode_t *)fi->fh;
        error = xfs_sync_file(ip, isdatasync);
    } else {
        error = find_path(current_xfs_mount(), path, &ip);
        if (error)
            return -ENOENT;
        error = xfs_sync_file(ip, isdatasync);
        libxfs_iput(ip, 0);
    }

    return error;
}
```

---

## 3. Phase 2: File I/O Operations

### 3.1 create (fuse_xfs_create)

**Purpose**: Create and open a new file

**FUSE Signature**:
```c
static int fuse_xfs_create(const char *path, mode_t mode,
                           struct fuse_file_info *fi);
```

**Algorithm** (based on [`proto.c`](src/xfsprogs/mkfs/proto.c:441-454)):
```
1. Parse path to get parent directory and filename
2. Look up parent directory inode
3. Verify parent is a directory
4. Allocate transaction with create reservation
5. Allocate new inode (libxfs_inode_alloc)
6. Initialize inode attributes (mode, uid, gid, timestamps)
7. Create directory entry in parent (libxfs_dir_createname)
8. Update parent directory mtime/ctime
9. Commit transaction
10. Store inode pointer in fi->fh for subsequent operations
```

**Implementation in xfsutil**:
```c
int xfs_create_file(xfs_mount_t *mp, xfs_inode_t *dp,
                    const char *name, mode_t mode,
                    xfs_inode_t **ipp) {
    xfs_trans_t     *tp;
    xfs_inode_t     *ip;
    xfs_bmap_free_t flist;
    xfs_fsblock_t   first;
    struct xfs_name xname;
    struct cred     cr;
    struct fsxattr  fsx;
    int             committed;
    int             error;

    /* Verify parent is a directory */
    if (!S_ISDIR(dp->i_d.di_mode))
        return -ENOTDIR;

    /* Setup name structure */
    xname.name = (unsigned char *)name;
    xname.len = strlen(name);

    /* Setup credentials (use current process) */
    memset(&cr, 0, sizeof(cr));
    cr.cr_uid = getuid();
    cr.cr_gid = getgid();

    /* Setup extended attributes (none) */
    memset(&fsx, 0, sizeof(fsx));

    /* Allocate transaction */
    tp = libxfs_trans_alloc(mp, XFS_TRANS_CREATE);
    if (!tp)
        return -ENOMEM;

    /* Reserve space for create operation */
    error = libxfs_trans_reserve(tp, 
                                 XFS_CREATE_SPACE_RES(mp, xname.len),
                                 mp->m_reservations.tr_create,
                                 0, 0, 0);
    if (error) {
        libxfs_trans_cancel(tp, 0);
        return -error;
    }

    /* Allocate the inode */
    error = libxfs_inode_alloc(&tp, dp, mode | S_IFREG, 1, 0,
                               &cr, &fsx, &ip);
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    /* Join parent directory to transaction */
    libxfs_trans_ijoin(tp, dp, 0);
    libxfs_trans_ihold(tp, dp);

    /* Create directory entry */
    XFS_BMAP_INIT(&flist, &first);
    error = libxfs_dir_createname(tp, dp, &xname, ip->i_ino,
                                  &first, &flist,
                                  XFS_CREATE_SPACE_RES(mp, xname.len));
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    /* Update parent timestamps */
    libxfs_ichgtime(dp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
    libxfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);
    libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

    /* Complete deferred operations */
    error = libxfs_bmap_finish(&tp, &flist, &committed);
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    /* Commit transaction */
    error = libxfs_trans_commit(tp, 0);
    if (error)
        return -error;

    *ipp = ip;
    return 0;
}
```

**FUSE Handler**:
```c
static int fuse_xfs_create(const char *path, mode_t mode,
                           struct fuse_file_info *fi) {
    xfs_mount_t *mp = current_xfs_mount();
    xfs_inode_t *dp = NULL;
    xfs_inode_t *ip = NULL;
    char *parent_path;
    char *name;
    int error;

    log_debug("create %s mode=%o\n", path, mode);

    /* Split path into parent and name */
    error = split_path(path, &parent_path, &name);
    if (error)
        return error;

    /* Look up parent directory */
    error = find_path(mp, parent_path, &dp);
    free(parent_path);
    if (error) {
        free(name);
        return -ENOENT;
    }

    /* Create the file */
    error = xfs_create_file(mp, dp, name, mode, &ip);
    free(name);
    libxfs_iput(dp, 0);

    if (error)
        return error;

    /* Store inode in file handle for subsequent operations */
    fi->fh = (uint64_t)ip;
    return 0;
}
```

---

### 3.2 write (fuse_xfs_write)

**Purpose**: Write data to a file

**FUSE Signature**:
```c
static int fuse_xfs_write(const char *path, const char *buf,
                          size_t size, off_t offset,
                          struct fuse_file_info *fi);
```

**Algorithm** (based on [`proto.c:newfile()`](src/xfsprogs/mkfs/proto.c:212-270)):
```
1. Get inode from fi->fh
2. Calculate file blocks needed
3. For each chunk of data:
   a. Allocate transaction with write reservation
   b. Map file offset to disk blocks (allocate if needed)
   c. Get buffer for disk block
   d. Copy data to buffer
   e. Log buffer (or write directly)
   f. Update file size if extending
   g. Commit transaction
4. Update mtime/ctime
```

**Implementation in xfsutil**:
```c
int xfs_writefile(xfs_inode_t *ip, const void *buffer,
                  off_t offset, size_t len, size_t *written) {
    xfs_mount_t     *mp = ip->i_mount;
    xfs_trans_t     *tp;
    xfs_bmap_free_t flist;
    xfs_fsblock_t   first;
    xfs_bmbt_irec_t map;
    xfs_buf_t       *bp;
    xfs_daddr_t     d;
    xfs_fileoff_t   start_fsb;
    xfs_filblks_t   count_fsb;
    int             nmap;
    int             committed;
    int             error;
    size_t          bytes_written = 0;

    if (!S_ISREG(ip->i_d.di_mode))
        return -EINVAL;

    start_fsb = XFS_B_TO_FSBT(mp, offset);
    count_fsb = XFS_B_TO_FSB(mp, len + (offset - XFS_FSB_TO_B(mp, start_fsb)));

    /* Allocate transaction */
    tp = libxfs_trans_alloc(mp, XFS_TRANS_WRITE);
    if (!tp)
        return -ENOMEM;

    error = libxfs_trans_reserve(tp,
                                 XFS_DIOSTRAT_SPACE_RES(mp, count_fsb),
                                 mp->m_reservations.tr_write,
                                 0, 0, 0);
    if (error) {
        libxfs_trans_cancel(tp, 0);
        return -error;
    }

    libxfs_trans_ijoin(tp, ip, 0);
    libxfs_trans_ihold(tp, ip);

    XFS_BMAP_INIT(&flist, &first);

    /* Allocate space and map to disk blocks */
    nmap = 1;
    error = libxfs_bmapi(tp, ip, start_fsb, count_fsb,
                         XFS_BMAPI_WRITE, &first, count_fsb,
                         &map, &nmap, &flist, NULL);
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    if (nmap == 0 || map.br_startblock == HOLESTARTBLOCK) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -ENOSPC;
    }

    /* Get buffer and write data */
    d = XFS_FSB_TO_DADDR(mp, map.br_startblock);
    bp = libxfs_trans_get_buf(tp, mp->m_dev, d,
                              XFS_FSB_TO_BB(mp, map.br_blockcount), 0);
    if (!bp) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -EIO;
    }

    /* Copy data to buffer */
    size_t buf_offset = offset - XFS_FSB_TO_B(mp, start_fsb);
    size_t copy_len = MIN(len, XFS_BUF_COUNT(bp) - buf_offset);
    memcpy(XFS_BUF_PTR(bp) + buf_offset, buffer, copy_len);
    bytes_written = copy_len;

    /* Log the buffer */
    libxfs_trans_log_buf(tp, bp, buf_offset, buf_offset + copy_len - 1);

    /* Update file size if we extended the file */
    if (offset + bytes_written > ip->i_d.di_size) {
        ip->i_d.di_size = offset + bytes_written;
        ip->i_size = ip->i_d.di_size;
    }

    /* Update timestamps */
    libxfs_ichgtime(ip, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
    libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

    /* Complete deferred operations */
    error = libxfs_bmap_finish(&tp, &flist, &committed);
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    error = libxfs_trans_commit(tp, 0);
    if (error)
        return -error;

    *written = bytes_written;
    return 0;
}
```

**FUSE Handler**:
```c
static int fuse_xfs_write(const char *path, const char *buf,
                          size_t size, off_t offset,
                          struct fuse_file_info *fi) {
    xfs_inode_t *ip;
    size_t written = 0;
    size_t total_written = 0;
    int error;

    log_debug("write %s size=%zu offset=%lld\n", path, size, offset);

    ip = (xfs_inode_t *)fi->fh;
    if (!ip)
        return -EBADF;

    /* Write in chunks */
    while (total_written < size) {
        size_t chunk = MIN(size - total_written, 
                          ip->i_mount->m_sb.sb_blocksize * 16);
        
        error = xfs_writefile(ip, buf + total_written,
                              offset + total_written,
                              chunk, &written);
        if (error)
            return error;
        
        total_written += written;
        if (written < chunk)
            break;  /* Short write */
    }

    return total_written;
}
```

---

### 3.3 unlink (fuse_xfs_unlink)

**Purpose**: Remove a file

**FUSE Signature**:
```c
static int fuse_xfs_unlink(const char *path);
```

**Algorithm**:
```
1. Parse path to get parent directory and filename
2. Look up parent directory and target file inodes
3. Verify target is not a directory (use rmdir for that)
4. Allocate transaction with remove reservation
5. Decrement link count on target inode
6. If link count reaches 0:
   a. Add inode to unlinked list (for crash recovery)
   b. Mark inode for freeing
7. Remove directory entry from parent
8. Update parent timestamps
9. Commit transaction
10. If link count was 0, free inode resources
```

**Implementation in xfsutil**:
```c
int xfs_remove_file(xfs_mount_t *mp, xfs_inode_t *dp,
                    const char *name) {
    xfs_trans_t     *tp;
    xfs_inode_t     *ip;
    xfs_bmap_free_t flist;
    xfs_fsblock_t   first;
    struct xfs_name xname;
    xfs_ino_t       inum;
    int             committed;
    int             error;

    if (!S_ISDIR(dp->i_d.di_mode))
        return -ENOTDIR;

    xname.name = (unsigned char *)name;
    xname.len = strlen(name);

    /* Look up the target */
    error = libxfs_dir_lookup(NULL, dp, &xname, &inum, NULL);
    if (error)
        return -error;

    error = libxfs_iget(mp, NULL, inum, 0, &ip, 0);
    if (error)
        return -error;

    /* Cannot unlink directories */
    if (S_ISDIR(ip->i_d.di_mode)) {
        libxfs_iput(ip, 0);
        return -EISDIR;
    }

    /* Allocate transaction */
    tp = libxfs_trans_alloc(mp, XFS_TRANS_REMOVE);
    if (!tp) {
        libxfs_iput(ip, 0);
        return -ENOMEM;
    }

    error = libxfs_trans_reserve(tp, 
                                 XFS_REMOVE_SPACE_RES(mp),
                                 mp->m_reservations.tr_remove,
                                 0, 0, 0);
    if (error) {
        libxfs_trans_cancel(tp, 0);
        libxfs_iput(ip, 0);
        return -error;
    }

    libxfs_trans_ijoin(tp, dp, 0);
    libxfs_trans_ijoin(tp, ip, 0);
    libxfs_trans_ihold(tp, dp);
    libxfs_trans_ihold(tp, ip);

    XFS_BMAP_INIT(&flist, &first);

    /* Remove directory entry */
    error = libxfs_dir_removename(tp, dp, &xname, ip->i_ino,
                                  &first, &flist,
                                  XFS_REMOVE_SPACE_RES(mp));
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        libxfs_iput(ip, 0);
        return -error;
    }

    /* Decrement link count */
    ip->i_d.di_nlink--;

    /* If last link, free the inode */
    if (ip->i_d.di_nlink == 0) {
        error = libxfs_iunlink(tp, ip);
        if (error) {
            libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
            libxfs_iput(ip, 0);
            return -error;
        }
    }

    /* Update timestamps */
    libxfs_ichgtime(dp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
    libxfs_ichgtime(ip, XFS_ICHGTIME_CHG);

    libxfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);
    libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

    error = libxfs_bmap_finish(&tp, &flist, &committed);
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        libxfs_iput(ip, 0);
        return -error;
    }

    error = libxfs_trans_commit(tp, 0);
    
    /* Free inode data if unlinked */
    if (!error && ip->i_d.di_nlink == 0) {
        error = libxfs_ifree(tp, ip, &flist);
    }

    libxfs_iput(ip, 0);
    return error ? -error : 0;
}
```

---

### 3.4 mknod (fuse_xfs_mknod)

**Purpose**: Create a special file (device node, FIFO, socket)

**FUSE Signature**:
```c
static int fuse_xfs_mknod(const char *path, mode_t mode, dev_t rdev);
```

**Algorithm** (based on [`proto.c:479-517`](src/xfsprogs/mkfs/proto.c:479-517)):
```
1. Parse path to get parent directory and filename
2. Look up parent directory inode
3. Allocate transaction
4. Allocate new inode with appropriate type:
   - S_IFBLK: Block device
   - S_IFCHR: Character device
   - S_IFIFO: FIFO (named pipe)
   - S_IFSOCK: Socket
5. Set device number if block/char device
6. Create directory entry
7. Commit transaction
```

**Implementation in xfsutil**:
```c
int xfs_create_node(xfs_mount_t *mp, xfs_inode_t *dp,
                    const char *name, mode_t mode, dev_t rdev,
                    xfs_inode_t **ipp) {
    xfs_trans_t     *tp;
    xfs_inode_t     *ip;
    xfs_bmap_free_t flist;
    xfs_fsblock_t   first;
    struct xfs_name xname;
    struct cred     cr;
    struct fsxattr  fsx;
    int             committed;
    int             error;

    if (!S_ISDIR(dp->i_d.di_mode))
        return -ENOTDIR;

    /* Validate mode - must be a special file type */
    switch (mode & S_IFMT) {
    case S_IFBLK:
    case S_IFCHR:
    case S_IFIFO:
    case S_IFSOCK:
        break;
    default:
        return -EINVAL;
    }

    xname.name = (unsigned char *)name;
    xname.len = strlen(name);

    memset(&cr, 0, sizeof(cr));
    cr.cr_uid = getuid();
    cr.cr_gid = getgid();

    memset(&fsx, 0, sizeof(fsx));

    tp = libxfs_trans_alloc(mp, XFS_TRANS_CREATE);
    if (!tp)
        return -ENOMEM;

    error = libxfs_trans_reserve(tp, 
                                 XFS_CREATE_SPACE_RES(mp, xname.len),
                                 mp->m_reservations.tr_create,
                                 0, 0, 0);
    if (error) {
        libxfs_trans_cancel(tp, 0);
        return -error;
    }

    /* For device nodes, rdev is significant */
    error = libxfs_inode_alloc(&tp, dp, mode, 1, rdev,
                               &cr, &fsx, &ip);
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    libxfs_trans_ijoin(tp, dp, 0);
    libxfs_trans_ihold(tp, dp);

    XFS_BMAP_INIT(&flist, &first);

    error = libxfs_dir_createname(tp, dp, &xname, ip->i_ino,
                                  &first, &flist,
                                  XFS_CREATE_SPACE_RES(mp, xname.len));
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    libxfs_ichgtime(dp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
    
    libxfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);
    libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE | XFS_ILOG_DEV);

    error = libxfs_bmap_finish(&tp, &flist, &committed);
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    error = libxfs_trans_commit(tp, 0);
    if (error)
        return -error;

    *ipp = ip;
    return 0;
}
```

---

## 4. Phase 3: Directory Operations

### 4.1 mkdir (fuse_xfs_mkdir)

**Purpose**: Create a new directory

**FUSE Signature**:
```c
static int fuse_xfs_mkdir(const char *path, mode_t mode);
```

**Algorithm** (based on [`proto.c:531-576`](src/xfsprogs/mkfs/proto.c:531-576)):
```
1. Parse path to get parent directory and name
2. Look up parent directory inode
3. Allocate transaction with mkdir reservation
4. Allocate new inode with S_IFDIR type
5. Initialize directory (add . and .. entries)
6. Increment parent link count (for .. entry)
7. Create directory entry in parent
8. Update timestamps
9. Commit transaction
```

**Implementation in xfsutil**:
```c
int xfs_create_dir(xfs_mount_t *mp, xfs_inode_t *dp,
                   const char *name, mode_t mode,
                   xfs_inode_t **ipp) {
    xfs_trans_t     *tp;
    xfs_inode_t     *ip;
    xfs_bmap_free_t flist;
    xfs_fsblock_t   first;
    struct xfs_name xname;
    struct cred     cr;
    struct fsxattr  fsx;
    int             committed;
    int             error;

    if (!S_ISDIR(dp->i_d.di_mode))
        return -ENOTDIR;

    xname.name = (unsigned char *)name;
    xname.len = strlen(name);

    memset(&cr, 0, sizeof(cr));
    cr.cr_uid = getuid();
    cr.cr_gid = getgid();

    memset(&fsx, 0, sizeof(fsx));

    tp = libxfs_trans_alloc(mp, XFS_TRANS_MKDIR);
    if (!tp)
        return -ENOMEM;

    error = libxfs_trans_reserve(tp, 
                                 XFS_MKDIR_SPACE_RES(mp, xname.len),
                                 mp->m_reservations.tr_mkdir,
                                 0, 0, 0);
    if (error) {
        libxfs_trans_cancel(tp, 0);
        return -error;
    }

    /* Allocate inode for directory */
    error = libxfs_inode_alloc(&tp, dp, mode | S_IFDIR, 1, 0,
                               &cr, &fsx, &ip);
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    /* New directory has link count 2 (. and parent's entry) */
    ip->i_d.di_nlink = 2;

    libxfs_trans_ijoin(tp, dp, 0);
    libxfs_trans_ihold(tp, dp);
    libxfs_trans_ihold(tp, ip);

    XFS_BMAP_INIT(&flist, &first);

    /* Initialize directory structure (. and .. entries) */
    error = libxfs_dir_init(tp, ip, dp);
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    /* Create entry in parent directory */
    error = libxfs_dir_createname(tp, dp, &xname, ip->i_ino,
                                  &first, &flist,
                                  XFS_MKDIR_SPACE_RES(mp, xname.len));
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    /* Increment parent link count for .. entry */
    dp->i_d.di_nlink++;

    libxfs_ichgtime(dp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
    libxfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);
    libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

    error = libxfs_bmap_finish(&tp, &flist, &committed);
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    error = libxfs_trans_commit(tp, 0);
    if (error)
        return -error;

    *ipp = ip;
    return 0;
}
```

---

### 4.2 rmdir (fuse_xfs_rmdir)

**Purpose**: Remove an empty directory

**FUSE Signature**:
```c
static int fuse_xfs_rmdir(const char *path);
```

**Algorithm**:
```
1. Parse path to get parent directory and name
2. Look up parent and target directory inodes
3. Verify target is a directory
4. Verify directory is empty (only . and .. entries)
5. Allocate transaction
6. Remove directory entry from parent
7. Decrement parent link count (for removed .. entry)
8. Mark target directory for removal
9. Update timestamps
10. Commit transaction
```

**Implementation in xfsutil**:
```c
int xfs_remove_dir(xfs_mount_t *mp, xfs_inode_t *dp,
                   const char *name) {
    xfs_trans_t     *tp;
    xfs_inode_t     *ip;
    xfs_bmap_free_t flist;
    xfs_fsblock_t   first;
    struct xfs_name xname;
    xfs_ino_t       inum;
    int             committed;
    int             error;

    if (!S_ISDIR(dp->i_d.di_mode))
        return -ENOTDIR;

    xname.name = (unsigned char *)name;
    xname.len = strlen(name);

    /* Look up target */
    error = libxfs_dir_lookup(NULL, dp, &xname, &inum, NULL);
    if (error)
        return -error;

    error = libxfs_iget(mp, NULL, inum, 0, &ip, 0);
    if (error)
        return -error;

    /* Must be a directory */
    if (!S_ISDIR(ip->i_d.di_mode)) {
        libxfs_iput(ip, 0);
        return -ENOTDIR;
    }

    /* Check if empty (link count should be 2: . and ..) */
    if (ip->i_d.di_nlink > 2) {
        libxfs_iput(ip, 0);
        return -ENOTEMPTY;
    }

    /* Additional check: scan directory for entries */
    error = xfs_dir_isempty(ip);
    if (error) {
        libxfs_iput(ip, 0);
        return -ENOTEMPTY;
    }

    tp = libxfs_trans_alloc(mp, XFS_TRANS_RMDIR);
    if (!tp) {
        libxfs_iput(ip, 0);
        return -ENOMEM;
    }

    error = libxfs_trans_reserve(tp,
                                 XFS_REMOVE_SPACE_RES(mp),
                                 mp->m_reservations.tr_remove,
                                 0, 0, 0);
    if (error) {
        libxfs_trans_cancel(tp, 0);
        libxfs_iput(ip, 0);
        return -error;
    }

    libxfs_trans_ijoin(tp, dp, 0);
    libxfs_trans_ijoin(tp, ip, 0);
    libxfs_trans_ihold(tp, dp);
    libxfs_trans_ihold(tp, ip);

    XFS_BMAP_INIT(&flist, &first);

    /* Remove directory entry */
    error = libxfs_dir_removename(tp, dp, &xname, ip->i_ino,
                                  &first, &flist,
                                  XFS_REMOVE_SPACE_RES(mp));
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        libxfs_iput(ip, 0);
        return -error;
    }

    /* Decrement link counts */
    dp->i_d.di_nlink--;  /* Removed .. entry */
    ip->i_d.di_nlink = 0;  /* Directory being removed */

    /* Add to unlinked list */
    error = libxfs_iunlink(tp, ip);
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        libxfs_iput(ip, 0);
        return -error;
    }

    libxfs_ichgtime(dp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
    libxfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);
    libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

    error = libxfs_bmap_finish(&tp, &flist, &committed);
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        libxfs_iput(ip, 0);
        return -error;
    }

    error = libxfs_trans_commit(tp, 0);
    libxfs_iput(ip, 0);
    
    return error ? -error : 0;
}
```

---

### 4.3 rename (fuse_xfs_rename)

**Purpose**: Rename a file or directory

**FUSE Signature**:
```c
static int fuse_xfs_rename(const char *from, const char *to);
```

**Algorithm**:
```
1. Parse both paths to get parent directories and names
2. Look up all involved inodes (src_dir, dst_dir, src_entry)
3. Check if destination exists
4. Allocate transaction with rename reservation
5. Lock inodes in proper order (by inode number)
6. If destination exists:
   a. Verify compatible types (file->file, dir->dir)
   b. If directory, verify empty
   c. Unlink destination
7. If moving to different directory:
   a. Create entry in destination directory
   b. Remove entry from source directory
   c. Update link counts if directories involved
8. If same directory:
   a. Use dir_replace operation
9. Update timestamps on all affected inodes
10. Commit transaction
```

**Implementation in xfsutil**:
```c
int xfs_rename_entry(xfs_mount_t *mp,
                     xfs_inode_t *src_dp, const char *src_name,
                     xfs_inode_t *dst_dp, const char *dst_name) {
    xfs_trans_t     *tp;
    xfs_inode_t     *src_ip = NULL;
    xfs_inode_t     *dst_ip = NULL;
    xfs_bmap_free_t flist;
    xfs_fsblock_t   first;
    struct xfs_name src_xname, dst_xname;
    xfs_ino_t       src_inum, dst_inum;
    int             committed;
    int             error;
    int             same_dir;

    src_xname.name = (unsigned char *)src_name;
    src_xname.len = strlen(src_name);
    dst_xname.name = (unsigned char *)dst_name;
    dst_xname.len = strlen(dst_name);

    same_dir = (src_dp->i_ino == dst_dp->i_ino);

    /* Look up source */
    error = libxfs_dir_lookup(NULL, src_dp, &src_xname, &src_inum, NULL);
    if (error)
        return -error;

    error = libxfs_iget(mp, NULL, src_inum, 0, &src_ip, 0);
    if (error)
        return -error;

    /* Check if destination exists */
    error = libxfs_dir_lookup(NULL, dst_dp, &dst_xname, &dst_inum, NULL);
    if (error == 0) {
        error = libxfs_iget(mp, NULL, dst_inum, 0, &dst_ip, 0);
        if (error) {
            libxfs_iput(src_ip, 0);
            return -error;
        }

        /* Type compatibility check */
        if (S_ISDIR(src_ip->i_d.di_mode) != S_ISDIR(dst_ip->i_d.di_mode)) {
            libxfs_iput(src_ip, 0);
            libxfs_iput(dst_ip, 0);
            return S_ISDIR(dst_ip->i_d.di_mode) ? -EISDIR : -ENOTDIR;
        }

        /* If destination is directory, must be empty */
        if (S_ISDIR(dst_ip->i_d.di_mode) && dst_ip->i_d.di_nlink > 2) {
            libxfs_iput(src_ip, 0);
            libxfs_iput(dst_ip, 0);
            return -ENOTEMPTY;
        }
    } else if (error != ENOENT) {
        libxfs_iput(src_ip, 0);
        return -error;
    }

    tp = libxfs_trans_alloc(mp, XFS_TRANS_RENAME);
    if (!tp) {
        libxfs_iput(src_ip, 0);
        if (dst_ip) libxfs_iput(dst_ip, 0);
        return -ENOMEM;
    }

    error = libxfs_trans_reserve(tp,
                                 XFS_RENAME_SPACE_RES(mp, dst_xname.len),
                                 mp->m_reservations.tr_rename,
                                 0, 0, 0);
    if (error) {
        libxfs_trans_cancel(tp, 0);
        libxfs_iput(src_ip, 0);
        if (dst_ip) libxfs_iput(dst_ip, 0);
        return -error;
    }

    /* Join inodes to transaction */
    libxfs_trans_ijoin(tp, src_dp, 0);
    libxfs_trans_ihold(tp, src_dp);
    if (!same_dir) {
        libxfs_trans_ijoin(tp, dst_dp, 0);
        libxfs_trans_ihold(tp, dst_dp);
    }
    libxfs_trans_ijoin(tp, src_ip, 0);
    libxfs_trans_ihold(tp, src_ip);
    if (dst_ip) {
        libxfs_trans_ijoin(tp, dst_ip, 0);
        libxfs_trans_ihold(tp, dst_ip);
    }

    XFS_BMAP_INIT(&flist, &first);

    /* Remove destination if it exists */
    if (dst_ip) {
        error = libxfs_dir_removename(tp, dst_dp, &dst_xname, dst_ip->i_ino,
                                      &first, &flist,
                                      XFS_REMOVE_SPACE_RES(mp));
        if (error)
            goto abort;

        dst_ip->i_d.di_nlink--;
        if (S_ISDIR(dst_ip->i_d.di_mode))
            dst_dp->i_d.di_nlink--;

        if (dst_ip->i_d.di_nlink == 0) {
            error = libxfs_iunlink(tp, dst_ip);
            if (error)
                goto abort;
        }
    }

    /* Create entry in destination */
    error = libxfs_dir_createname(tp, dst_dp, &dst_xname, src_ip->i_ino,
                                  &first, &flist,
                                  XFS_RENAME_SPACE_RES(mp, dst_xname.len));
    if (error)
        goto abort;

    /* Remove entry from source */
    error = libxfs_dir_removename(tp, src_dp, &src_xname, src_ip->i_ino,
                                  &first, &flist,
                                  XFS_REMOVE_SPACE_RES(mp));
    if (error)
        goto abort;

    /* Update link counts for directory rename */
    if (S_ISDIR(src_ip->i_d.di_mode) && !same_dir) {
        src_dp->i_d.di_nlink--;
        dst_dp->i_d.di_nlink++;
        
        /* Update .. entry in moved directory */
        error = libxfs_dir_replace(tp, src_ip, &xfs_name_dotdot,
                                   dst_dp->i_ino, &first, &flist,
                                   XFS_RENAME_SPACE_RES(mp, 2));
        if (error)
            goto abort;
    }

    /* Update timestamps */
    libxfs_ichgtime(src_dp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
    if (!same_dir)
        libxfs_ichgtime(dst_dp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
    libxfs_ichgtime(src_ip, XFS_ICHGTIME_CHG);
    if (dst_ip)
        libxfs_ichgtime(dst_ip, XFS_ICHGTIME_CHG);

    libxfs_trans_log_inode(tp, src_dp, XFS_ILOG_CORE);
    if (!same_dir)
        libxfs_trans_log_inode(tp, dst_dp, XFS_ILOG_CORE);
    libxfs_trans_log_inode(tp, src_ip, XFS_ILOG_CORE);
    if (dst_ip)
        libxfs_trans_log_inode(tp, dst_ip, XFS_ILOG_CORE);

    error = libxfs_bmap_finish(&tp, &flist, &committed);
    if (error)
        goto abort;

    error = libxfs_trans_commit(tp, 0);
    
    libxfs_iput(src_ip, 0);
    if (dst_ip) libxfs_iput(dst_ip, 0);
    
    return error ? -error : 0;

abort:
    libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
    libxfs_iput(src_ip, 0);
    if (dst_ip) libxfs_iput(dst_ip, 0);
    return -error;
}
```

---

## 5. Phase 4: Link Operations

### 5.1 symlink (fuse_xfs_symlink)

**Purpose**: Create a symbolic link

**FUSE Signature**:
```c
static int fuse_xfs_symlink(const char *target, const char *linkpath);
```

**Algorithm** (based on [`proto.c:518-530`](src/xfsprogs/mkfs/proto.c:518-530)):
```
1. Parse linkpath to get parent directory and link name
2. Look up parent directory
3. Allocate transaction with symlink reservation
4. Allocate new inode with S_IFLNK type
5. Store target path:
   a. If short enough, store inline in inode (local format)
   b. Otherwise, allocate extents and store target
6. Create directory entry
7. Commit transaction
```

**Implementation in xfsutil**:
```c
int xfs_create_symlink(xfs_mount_t *mp, xfs_inode_t *dp,
                       const char *name, const char *target) {
    xfs_trans_t     *tp;
    xfs_inode_t     *ip;
    xfs_bmap_free_t flist;
    xfs_fsblock_t   first;
    struct xfs_name xname;
    struct cred     cr;
    struct fsxattr  fsx;
    int             committed;
    int             error;
    int             pathlen;
    int             flags;

    if (!S_ISDIR(dp->i_d.di_mode))
        return -ENOTDIR;

    pathlen = strlen(target);
    if (pathlen >= MAXPATHLEN)
        return -ENAMETOOLONG;

    xname.name = (unsigned char *)name;
    xname.len = strlen(name);

    memset(&cr, 0, sizeof(cr));
    cr.cr_uid = getuid();
    cr.cr_gid = getgid();

    memset(&fsx, 0, sizeof(fsx));

    tp = libxfs_trans_alloc(mp, XFS_TRANS_SYMLINK);
    if (!tp)
        return -ENOMEM;

    error = libxfs_trans_reserve(tp,
                                 XFS_SYMLINK_SPACE_RES(mp, xname.len, pathlen),
                                 mp->m_reservations.tr_symlink,
                                 0, 0, 0);
    if (error) {
        libxfs_trans_cancel(tp, 0);
        return -error;
    }

    /* Allocate symlink inode */
    error = libxfs_inode_alloc(&tp, dp, S_IFLNK | 0777, 1, 0,
                               &cr, &fsx, &ip);
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    libxfs_trans_ijoin(tp, dp, 0);
    libxfs_trans_ihold(tp, dp);

    XFS_BMAP_INIT(&flist, &first);

    /* Store target path - use inline storage if possible */
    flags = XFS_ILOG_CORE;
    if (pathlen <= XFS_IFORK_DSIZE(ip)) {
        /* Inline storage */
        xfs_idata_realloc(ip, pathlen, XFS_DATA_FORK);
        memcpy(ip->i_df.if_u1.if_data, target, pathlen);
        ip->i_d.di_size = pathlen;
        ip->i_df.if_flags &= ~XFS_IFEXTENTS;
        ip->i_df.if_flags |= XFS_IFINLINE;
        ip->i_d.di_format = XFS_DINODE_FMT_LOCAL;
        flags |= XFS_ILOG_DDATA;
    } else {
        /* Extent storage - allocate blocks */
        xfs_buf_t *bp;
        xfs_bmbt_irec_t map;
        xfs_daddr_t d;
        int nmap = 1;
        xfs_filblks_t nb = XFS_B_TO_FSB(mp, pathlen);

        error = libxfs_bmapi(tp, ip, 0, nb, XFS_BMAPI_WRITE,
                             &first, nb, &map, &nmap, &flist, NULL);
        if (error) {
            libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
            return -error;
        }

        d = XFS_FSB_TO_DADDR(mp, map.br_startblock);
        bp = libxfs_trans_get_buf(tp, mp->m_dev, d,
                                  XFS_FSB_TO_BB(mp, nb), 0);
        memcpy(XFS_BUF_PTR(bp), target, pathlen);
        if (pathlen < XFS_BUF_COUNT(bp))
            memset(XFS_BUF_PTR(bp) + pathlen, 0, 
                   XFS_BUF_COUNT(bp) - pathlen);
        libxfs_trans_log_buf(tp, bp, 0, XFS_BUF_COUNT(bp) - 1);

        ip->i_d.di_size = pathlen;
    }

    /* Create directory entry */
    error = libxfs_dir_createname(tp, dp, &xname, ip->i_ino,
                                  &first, &flist,
                                  XFS_SYMLINK_SPACE_RES(mp, xname.len, pathlen));
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    libxfs_ichgtime(dp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
    libxfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);
    libxfs_trans_log_inode(tp, ip, flags);

    error = libxfs_bmap_finish(&tp, &flist, &committed);
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    error = libxfs_trans_commit(tp, 0);
    if (error)
        return -error;

    libxfs_iput(ip, 0);
    return 0;
}
```

---

### 5.2 link (fuse_xfs_link)

**Purpose**: Create a hard link

**FUSE Signature**:
```c
static int fuse_xfs_link(const char *oldpath, const char *newpath);
```

**Algorithm**:
```
1. Look up source inode
2. Parse newpath to get destination directory and name
3. Look up destination directory
4. Verify source is not a directory (no hard links to directories)
5. Verify source and destination are on same filesystem
6. Allocate transaction with link reservation
7. Increment source link count
8. Create directory entry in destination
9. Update timestamps
10. Commit transaction
```

**Implementation in xfsutil**:
```c
int xfs_create_link(xfs_mount_t *mp, xfs_inode_t *dp,
                    const char *name, xfs_inode_t *ip) {
    xfs_trans_t     *tp;
    xfs_bmap_free_t flist;
    xfs_fsblock_t   first;
    struct xfs_name xname;
    int             committed;
    int             error;

    if (!S_ISDIR(dp->i_d.di_mode))
        return -ENOTDIR;

    /* Cannot hard link directories */
    if (S_ISDIR(ip->i_d.di_mode))
        return -EPERM;

    /* Check link count limit */
    if (ip->i_d.di_nlink >= XFS_MAXLINK)
        return -EMLINK;

    xname.name = (unsigned char *)name;
    xname.len = strlen(name);

    tp = libxfs_trans_alloc(mp, XFS_TRANS_LINK);
    if (!tp)
        return -ENOMEM;

    error = libxfs_trans_reserve(tp,
                                 XFS_LINK_SPACE_RES(mp, xname.len),
                                 mp->m_reservations.tr_link,
                                 0, 0, 0);
    if (error) {
        libxfs_trans_cancel(tp, 0);
        return -error;
    }

    libxfs_trans_ijoin(tp, dp, 0);
    libxfs_trans_ijoin(tp, ip, 0);
    libxfs_trans_ihold(tp, dp);
    libxfs_trans_ihold(tp, ip);

    XFS_BMAP_INIT(&flist, &first);

    /* Increment link count */
    ip->i_d.di_nlink++;

    /* Create directory entry */
    error = libxfs_dir_createname(tp, dp, &xname, ip->i_ino,
                                  &first, &flist,
                                  XFS_LINK_SPACE_RES(mp, xname.len));
    if (error) {
        ip->i_d.di_nlink--;
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    libxfs_ichgtime(dp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
    libxfs_ichgtime(ip, XFS_ICHGTIME_CHG);

    libxfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);
    libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

    error = libxfs_bmap_finish(&tp, &flist, &committed);
    if (error) {
        libxfs_trans_cancel(tp, XFS_TRANS_ABORT);
        return -error;
    }

    error = libxfs_trans_commit(tp, 0);
    return error ? -error : 0;
}
```

---

### 5.3 readlink (Verification)

**Current Implementation** in [`fuse_xfs.c:56-73`](src/fuse/fuse_xfs.c:56-73):

The existing `readlink` implementation is functional and correctly implemented in [`xfsutil.c:907-919`](src/xfsutil/xfsutil.c:907-919). It handles:
- Local format symlinks (inline data)
- Extent format symlinks (data in blocks)

**Verification Points**:
- ✓ Handles XFS_DINODE_FMT_LOCAL (inline symlink)
- ✓ Handles XFS_DINODE_FMT_EXTENTS (external symlink)
- ✓ Properly null-terminates the buffer
- ✓ Returns correct length

No modifications needed.

---

## 6. Mount Configuration Changes

### 6.1 Read-Write Mount Option

Modify [`xfsutil.c:mount_xfs()`](src/xfsutil/xfsutil.c:1030-1082):

```c
xfs_mount_t *mount_xfs_ex(char *progname, char *source_name, int readonly) {
    xfs_mount_t	*mp;
    xfs_buf_t	*sbp;
    xfs_sb_t	*sb;
    libxfs_init_t	xargs;
    xfs_mount_t	*mbuf = (xfs_mount_t *)malloc(sizeof(xfs_mount_t));
    
    memset(&xargs, 0, sizeof(xargs));
    xargs.isdirect = LIBXFS_DIRECT;
    
    /* Set read-only flag based on parameter */
    if (readonly) {
        xargs.isreadonly = LIBXFS_ISREADONLY;
    } else {
        xargs.isreadonly = 0;  /* Read-write mode */
    }
    
    xargs.dname = source_name;
    xargs.disfile = 1;
    
    if (!libxfs_init(&xargs)) {
        do_log(_("%s: couldn't initialize XFS library\n"
                 "%s: Aborting.\n"), progname, progname);
        free(mbuf);
        return NULL;
    }
    
    sbp = libxfs_readbuf(xargs.ddev, XFS_SB_DADDR, 1, 0);
    memset(mbuf, 0, sizeof(xfs_mount_t));
    sb = &(mbuf->m_sb);
    libxfs_sb_from_disk(sb, XFS_BUF_TO_SBP(sbp));
    
    /* Mount with appropriate flags */
    int mount_flags = readonly ? LIBXFS_MOUNT_RDONLY : 0;
    mp = libxfs_mount(mbuf, sb, xargs.ddev, xargs.logdev, 
                      xargs.rtdev, mount_flags);
    
    if (mp == NULL) {
        do_log(_("%s: %s filesystem failed to initialize\n"
                 "%s: Aborting.\n"), progname, source_name, progname);
        free(mbuf);
        return NULL;
    }
    
    /* Validation checks */
    if (mp->m_sb.sb_inprogress) {
        do_log(_("%s %s filesystem failed to initialize\n"
                 "%s: Aborting.\n"), progname, source_name, progname);
        libxfs_umount(mp);
        return NULL;
    }
    
    if (mp->m_sb.sb_logstart == 0) {
        do_log(_("%s: %s has an external log.\n%s: Aborting.\n"),
               progname, source_name, progname);
        libxfs_umount(mp);
        return NULL;
    }
    
    if (mp->m_sb.sb_rextents != 0) {
        do_log(_("%s: %s has a real-time section.\n"
                 "%s: Aborting.\n"), progname, source_name, progname);
        libxfs_umount(mp);
        return NULL;
    }
    
    /* Store readonly flag in mount structure for later checks */
    if (readonly)
        mp->m_flags |= XFS_MOUNT_RDONLY;
    
    return mp;
}

/* Compatibility wrapper */
xfs_mount_t *mount_xfs(char *progname, char *source_name) {
    return mount_xfs_ex(progname, source_name, 1);  /* Default read-only */
}
```

### 6.2 FUSE Mount Options

Update [`main.c`](src/fuse/main.c) to support `-o rw` option:

```c
struct fuse_xfs_options {
    char *device;
    int readonly;
    xfs_mount_t *xfs_mount;
};

static struct fuse_opt fuse_xfs_opts[] = {
    { "device=%s", offsetof(struct fuse_xfs_options, device), 0 },
    { "ro", offsetof(struct fuse_xfs_options, readonly), 1 },
    { "rw", offsetof(struct fuse_xfs_options, readonly), 0 },
    FUSE_OPT_END
};
```

### 6.3 Unmount Sequence

```c
int unmount_xfs(xfs_mount_t *mp) {
    int error = 0;
    
    /* Sync all dirty data */
    error = xfs_sync_fs(mp);
    if (error)
        return error;
    
    /* Flush superblock */
    if (!(mp->m_flags & XFS_MOUNT_RDONLY)) {
        libxfs_writebuf(mp->m_sb_bp, 0);
    }
    
    /* Unmount the filesystem */
    libxfs_umount(mp);
    
    return 0;
}
```

### 6.4 FUSE Operations Structure Update

```c
/* Add missing write operations to the operations structure */
struct fuse_operations fuse_xfs_operations = {
    .init        = fuse_xfs_init,
    .destroy     = fuse_xfs_destroy,
    .getattr     = fuse_xfs_getattr,
    .fgetattr    = fuse_xfs_fgetattr,
    .readlink    = fuse_xfs_readlink,
    .opendir     = fuse_xfs_opendir, 
    .readdir     = fuse_xfs_readdir,
    .releasedir  = fuse_xfs_releasedir, 
    .mknod       = fuse_xfs_mknod, 
    .mkdir       = fuse_xfs_mkdir, 
    .symlink     = fuse_xfs_symlink, 
    .unlink      = fuse_xfs_unlink, 
    .rmdir       = fuse_xfs_rmdir, 
    .rename      = fuse_xfs_rename, 
    .link        = fuse_xfs_link,
    .chmod       = fuse_xfs_chmod,       /* NEW */
    .chown       = fuse_xfs_chown,       /* NEW */
    .truncate    = fuse_xfs_truncate,    /* NEW */
    .utimens     = fuse_xfs_utimens,     /* NEW */
    .create      = fuse_xfs_create,
    .open        = fuse_xfs_open,
    .read        = fuse_xfs_read,
    .write       = fuse_xfs_write, 
    .statfs      = fuse_xfs_statfs,
    .flush       = fuse_xfs_flush, 
    .release     = fuse_xfs_release,
    .fsync       = fuse_xfs_fsync, 
    .setxattr    = fuse_xfs_setxattr, 
    .getxattr    = fuse_xfs_getxattr, 
    .listxattr   = fuse_xfs_listxattr, 
    .removexattr = fuse_xfs_removexattr, 
};
```

---

## 7. Header File Design

### 7.1 New xfsutil.h Declarations

Add to [`xfsutil.h`](src/xfsutil/xfsutil.h):

```c
#ifndef __XFSUTIL_H__
#define __XFSUTIL_H__

#include <libc.h>
#include <xfs/libxfs.h>
#include <sys/stat.h>
#include <sys/dirent.h>

/*
 * Error codes (map to errno values)
 */
#define XFSUTIL_SUCCESS     0
#define XFSUTIL_ENOENT     -ENOENT
#define XFSUTIL_ENOSPC     -ENOSPC
#define XFSUTIL_EROFS      -EROFS
#define XFSUTIL_EINVAL     -EINVAL
#define XFSUTIL_EIO        -EIO

/*
 * Mount flags
 */
#define XFSUTIL_MOUNT_RDONLY    0x0001
#define XFSUTIL_MOUNT_NOATIME   0x0002
#define XFSUTIL_MOUNT_SYNC      0x0004

/*
 * File type conversion utilities
 */
unsigned char xfs_ftype_to_dtype(__uint8_t ftype);

/*
 * Directory operations (existing)
 */
int xfs_readdir(xfs_inode_t *dp, void *dirent, size_t bufsize,
                xfs_off_t *offset, filldir_t filldir);

/*
 * File read operations (existing)
 */
int xfs_readfile(xfs_inode_t *ip, void *buffer, off_t offset,
                 size_t len, int *last_extent);
int xfs_readlink(xfs_inode_t *ip, void *buffer, off_t offset,
                 size_t len, int *last_extent);

/*
 * File write operations (NEW)
 */
int xfs_writefile(xfs_inode_t *ip, const void *buffer, off_t offset,
                  size_t len, size_t *written);

/*
 * Inode attribute operations (NEW)
 */
int xfs_setattr_mode(xfs_inode_t *ip, mode_t mode);
int xfs_setattr_owner(xfs_inode_t *ip, uid_t uid, gid_t gid);
int xfs_setattr_time(xfs_inode_t *ip, const struct timespec *atime,
                     const struct timespec *mtime);
int xfs_truncate_file(xfs_inode_t *ip, off_t size);

/*
 * File/directory creation (NEW)
 */
int xfs_create_file(xfs_mount_t *mp, xfs_inode_t *dp,
                    const char *name, mode_t mode, xfs_inode_t **ipp);
int xfs_create_node(xfs_mount_t *mp, xfs_inode_t *dp,
                    const char *name, mode_t mode, dev_t rdev,
                    xfs_inode_t **ipp);
int xfs_create_dir(xfs_mount_t *mp, xfs_inode_t *dp,
                   const char *name, mode_t mode, xfs_inode_t **ipp);
int xfs_create_symlink(xfs_mount_t *mp, xfs_inode_t *dp,
                       const char *name, const char *target);
int xfs_create_link(xfs_mount_t *mp, xfs_inode_t *dp,
                    const char *name, xfs_inode_t *ip);

/*
 * File/directory removal (NEW)
 */
int xfs_remove_file(xfs_mount_t *mp, xfs_inode_t *dp,
                    const char *name);
int xfs_remove_dir(xfs_mount_t *mp, xfs_inode_t *dp,
                   const char *name);

/*
 * Rename operation (NEW)
 */
int xfs_rename_entry(xfs_mount_t *mp,
                     xfs_inode_t *src_dp, const char *src_name,
                     xfs_inode_t *dst_dp, const char *dst_name);

/*
 * Sync operations (NEW)
 */
int xfs_sync_file(xfs_inode_t *ip, int datasync);
int xfs_sync_fs(xfs_mount_t *mp);

/*
 * Inode information (existing)
 */
int xfs_stat(xfs_inode_t *inode, struct stat *stats);
int xfs_is_dir(xfs_inode_t *inode);
int xfs_is_link(xfs_inode_t *inode);
int xfs_is_regular(xfs_inode_t *inode);

/*
 * Path utilities (existing)
 */
int find_path(xfs_mount_t *mp, const char *path, xfs_inode_t **result);
struct xfs_name first_name(const char *path);
struct xfs_name next_name(struct xfs_name current);

/*
 * Path splitting utility (NEW)
 */
int split_path(const char *path, char **parent, char **name);

/*
 * Directory utilities (NEW)
 */
int xfs_dir_isempty(xfs_inode_t *dp);

/*
 * Mount/unmount (updated)
 */
xfs_mount_t *mount_xfs(char *progname, char *source_name);
xfs_mount_t *mount_xfs_ex(char *progname, char *source_name, int readonly);
int unmount_xfs(xfs_mount_t *mp);

/*
 * Read-only check utility (NEW)
 */
static inline int xfs_is_readonly(xfs_mount_t *mp) {
    return (mp->m_flags & XFS_MOUNT_RDONLY) != 0;
}

#endif /* __XFSUTIL_H__ */
```

### 7.2 New fuse_xfs.h Declarations

Add to [`fuse_xfs.h`](src/fuse/fuse_xfs.h):

```c
#ifndef __FUSE_XFS_H__
#define __FUSE_XFS_H__

#include <xfs/libxfs.h>

/*
 * FUSE mount options structure
 */
struct fuse_xfs_options {
    char *device;           /* Block device or image file */
    int readonly;           /* Mount read-only flag */
    xfs_mount_t *xfs_mount; /* XFS mount structure */
};

/*
 * Global mount accessor
 */
xfs_mount_t *current_xfs_mount(void);

/*
 * FUSE operations structure (external reference)
 */
extern struct fuse_operations fuse_xfs_operations;

#endif /* __FUSE_XFS_H__ */
```

---

## 8. Testing Strategy

### 8.1 Unit Test Approach

Create test files in `tests/` directory:

```
tests/
├── test_mount.c          # Mount/unmount tests
├── test_attributes.c     # chmod, chown, utimens tests
├── test_truncate.c       # truncate tests
├── test_file_io.c        # create, write, read, unlink tests
├── test_directory.c      # mkdir, rmdir, rename tests
├── test_links.c          # symlink, link tests
├── test_integration.c    # Full workflow tests
└── test_common.h         # Shared test utilities
```

#### 8.1.1 Test Common Header

```c
/* test_common.h */
#ifndef __TEST_COMMON_H__
#define __TEST_COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <xfsutil.h>

#define TEST_IMAGE_SIZE (64 * 1024 * 1024)  /* 64 MB test image */
#define TEST_IMAGE_PATH "/tmp/xfs_test.img"

/* Test result macros */
#define TEST_PASS(name) printf("PASS: %s\n", name)
#define TEST_FAIL(name, msg) do { \
    printf("FAIL: %s - %s\n", name, msg); \
    return 1; \
} while(0)

/* Setup/teardown functions */
int create_test_image(const char *path, size_t size);
int format_xfs_image(const char *path);
xfs_mount_t *mount_test_image(const char *path, int readonly);
int unmount_test_image(xfs_mount_t *mp);
void cleanup_test_image(const char *path);

#endif /* __TEST_COMMON_H__ */
```

#### 8.1.2 Attribute Tests Example

```c
/* test_attributes.c */
#include "test_common.h"

int test_chmod(xfs_mount_t *mp) {
    xfs_inode_t *ip;
    struct stat st;
    int error;
    
    /* Create test file */
    xfs_inode_t *root;
    error = libxfs_iget(mp, NULL, mp->m_sb.sb_rootino, 0, &root, 0);
    if (error) TEST_FAIL("chmod", "cannot get root inode");
    
    error = xfs_create_file(mp, root, "test_chmod", 0644, &ip);
    libxfs_iput(root, 0);
    if (error) TEST_FAIL("chmod", "cannot create test file");
    
    /* Test chmod */
    error = xfs_setattr_mode(ip, 0755);
    if (error) {
        libxfs_iput(ip, 0);
        TEST_FAIL("chmod", "chmod failed");
    }
    
    /* Verify mode changed */
    xfs_stat(ip, &st);
    if ((st.st_mode & 0777) != 0755) {
        libxfs_iput(ip, 0);
        TEST_FAIL("chmod", "mode not changed correctly");
    }
    
    libxfs_iput(ip, 0);
    TEST_PASS("chmod");
    return 0;
}

int test_chown(xfs_mount_t *mp) {
    xfs_inode_t *ip;
    struct stat st;
    int error;
    
    /* Get test file */
    error = find_path(mp, "/test_chmod", &ip);
    if (error) TEST_FAIL("chown", "cannot find test file");
    
    /* Test chown */
    error = xfs_setattr_owner(ip, 1000, 1000);
    if (error) {
        libxfs_iput(ip, 0);
        TEST_FAIL("chown", "chown failed");
    }
    
    /* Verify ownership changed */
    xfs_stat(ip, &st);
    if (st.st_uid != 1000 || st.st_gid != 1000) {
        libxfs_iput(ip, 0);
        TEST_FAIL("chown", "ownership not changed correctly");
    }
    
    libxfs_iput(ip, 0);
    TEST_PASS("chown");
    return 0;
}

int main(int argc, char *argv[]) {
    xfs_mount_t *mp;
    int failures = 0;
    
    /* Setup */
    if (create_test_image(TEST_IMAGE_PATH, TEST_IMAGE_SIZE))
        return 1;
    if (format_xfs_image(TEST_IMAGE_PATH))
        return 1;
    
    mp = mount_test_image(TEST_IMAGE_PATH, 0);
    if (!mp) {
        cleanup_test_image(TEST_IMAGE_PATH);
        return 1;
    }
    
    /* Run tests */
    failures += test_chmod(mp);
    failures += test_chown(mp);
    
    /* Teardown */
    unmount_test_image(mp);
    cleanup_test_image(TEST_IMAGE_PATH);
    
    printf("\nResults: %d failures\n", failures);
    return failures ? 1 : 0;
}
```

### 8.2 Integration Test Scenarios

#### 8.2.1 Basic File Operations

```bash
#!/bin/bash
# test_file_ops.sh

MOUNT_POINT="/tmp/xfs_test_mount"
TEST_IMAGE="/tmp/xfs_test.img"

# Create and format test image
dd if=/dev/zero of=$TEST_IMAGE bs=1M count=64
mkfs.xfs $TEST_IMAGE

# Mount with fusexfs
mkdir -p $MOUNT_POINT
./fusexfs -o device=$TEST_IMAGE,rw $MOUNT_POINT

# Test file creation
echo "Hello, XFS!" > $MOUNT_POINT/test.txt
[ -f $MOUNT_POINT/test.txt ] || exit 1

# Test file read
content=$(cat $MOUNT_POINT/test.txt)
[ "$content" = "Hello, XFS!" ] || exit 1

# Test chmod
chmod 755 $MOUNT_POINT/test.txt
[ $(stat -c %a $MOUNT_POINT/test.txt) = "755" ] || exit 1

# Test truncate
truncate -s 5 $MOUNT_POINT/test.txt
[ $(stat -c %s $MOUNT_POINT/test.txt) = "5" ] || exit 1

# Test unlink
rm $MOUNT_POINT/test.txt
[ ! -f $MOUNT_POINT/test.txt ] || exit 1

# Cleanup
fusermount -u $MOUNT_POINT
rm -rf $MOUNT_POINT $TEST_IMAGE

echo "All file operation tests passed!"
```

#### 8.2.2 Directory Operations

```bash
#!/bin/bash
# test_dir_ops.sh

MOUNT_POINT="/tmp/xfs_test_mount"
TEST_IMAGE="/tmp/xfs_test.img"

# Setup (similar to above)
# ...

# Test mkdir
mkdir $MOUNT_POINT/testdir
[ -d $MOUNT_POINT/testdir ] || exit 1

# Test nested mkdir
mkdir -p $MOUNT_POINT/testdir/nested/deep
[ -d $MOUNT_POINT/testdir/nested/deep ] || exit 1

# Test rmdir
rmdir $MOUNT_POINT/testdir/nested/deep
[ ! -d $MOUNT_POINT/testdir/nested/deep ] || exit 1

# Test rename directory
mv $MOUNT_POINT/testdir $MOUNT_POINT/renamed_dir
[ -d $MOUNT_POINT/renamed_dir ] || exit 1
[ ! -d $MOUNT_POINT/testdir ] || exit 1

# Cleanup
# ...

echo "All directory operation tests passed!"
```

#### 8.2.3 Link Operations

```bash
#!/bin/bash
# test_link_ops.sh

MOUNT_POINT="/tmp/xfs_test_mount"
TEST_IMAGE="/tmp/xfs_test.img"

# Setup
# ...

# Test symlink
echo "Original content" > $MOUNT_POINT/original.txt
ln -s original.txt $MOUNT_POINT/symlink.txt
[ -L $MOUNT_POINT/symlink.txt ] || exit 1
[ "$(readlink $MOUNT_POINT/symlink.txt)" = "original.txt" ] || exit 1

# Test hard link
ln $MOUNT_POINT/original.txt $MOUNT_POINT/hardlink.txt
[ $(stat -c %h $MOUNT_POINT/original.txt) = "2" ] || exit 1

# Verify hard link content
[ "$(cat $MOUNT_POINT/hardlink.txt)" = "Original content" ] || exit 1

# Cleanup
# ...

echo "All link operation tests passed!"
```

### 8.3 Data Integrity Verification

#### 8.3.1 Checksum Verification

```c
/* test_integrity.c */
#include <openssl/md5.h>

int verify_file_integrity(xfs_mount_t *mp, const char *path,
                          const unsigned char *expected_hash) {
    xfs_inode_t *ip;
    unsigned char buffer[4096];
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5_CTX ctx;
    off_t offset = 0;
    int error;
    
    error = find_path(mp, path, &ip);
    if (error) return -1;
    
    MD5_Init(&ctx);
    
    while (1) {
        int bytes = xfs_readfile(ip, buffer, offset, sizeof(buffer), NULL);
        if (bytes <= 0) break;
        MD5_Update(&ctx, buffer, bytes);
        offset += bytes;
    }
    
    MD5_Final(hash, &ctx);
    libxfs_iput(ip, 0);
    
    return memcmp(hash, expected_hash, MD5_DIGEST_LENGTH) == 0 ? 0 : -1;
}
```

#### 8.3.2 Crash Recovery Testing

```bash
#!/bin/bash
# test_crash_recovery.sh

# Create test image and mount
# ...

# Write some data
dd if=/dev/urandom of=$MOUNT_POINT/testfile bs=1M count=10

# Simulate crash (kill without unmount)
kill -9 $(pgrep -f "fusexfs.*$MOUNT_POINT")

# Remount
./fusexfs -o device=$TEST_IMAGE,rw $MOUNT_POINT

# Verify filesystem integrity
xfs_repair -n $TEST_IMAGE

# Verify data is accessible
ls -la $MOUNT_POINT/testfile

# Cleanup
# ...
```

#### 8.3.3 Concurrent Access Testing

```c
/* test_concurrent.c */
#include <pthread.h>

#define NUM_THREADS 10
#define OPS_PER_THREAD 100

void *concurrent_test(void *arg) {
    int thread_id = *(int *)arg;
    char filename[64];
    
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        snprintf(filename, sizeof(filename), 
                 "/test_thread_%d_%d", thread_id, i);
        
        /* Create file */
        int fd = open(filename, O_CREAT | O_WRONLY, 0644);
        if (fd < 0) continue;
        
        /* Write data */
        write(fd, "test data", 9);
        close(fd);
        
        /* Rename file */
        char newname[64];
        snprintf(newname, sizeof(newname), "%s.renamed", filename);
        rename(filename, newname);
        
        /* Delete file */
        unlink(newname);
    }
    
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    int thread_ids[NUM_THREADS];
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, concurrent_test, &thread_ids[i]);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("Concurrent test completed\n");
    return 0;
}
```

### 8.4 Test Matrix

| Test Category | Test Case | Priority | Automated |
|---------------|-----------|----------|-----------|
| **Mount** | Read-only mount | High | Yes |
| | Read-write mount | High | Yes |
| | Unmount with pending writes | High | Yes |
| **Attributes** | chmod | High | Yes |
| | chown | High | Yes |
| | utimens | Medium | Yes |
| **File I/O** | Create empty file | High | Yes |
| | Write small file (<4KB) | High | Yes |
| | Write large file (>1MB) | High | Yes |
| | Append to file | High | Yes |
| | Truncate file | High | Yes |
| | Delete file | High | Yes |
| **Directories** | Create directory | High | Yes |
| | Create nested directories | High | Yes |
| | Remove empty directory | High | Yes |
| | Remove non-empty directory | High | Yes |
| | Rename directory | High | Yes |
| **Links** | Create symlink | High | Yes |
| | Create hard link | High | Yes |
| | Remove symlink | High | Yes |
| | Rename file (same dir) | High | Yes |
| | Rename file (cross dir) | High | Yes |
| **Integrity** | Checksum verification | High | Yes |
| | Crash recovery | Medium | Manual |
| | Concurrent access | Medium | Yes |
| **Edge Cases** | Long filenames | Medium | Yes |
| | Deep directory nesting | Medium | Yes |
| | Maximum file size | Low | Yes |
| | Disk full condition | Medium | Yes |

---

## 9. Implementation Phases and Timeline

### Phase 1: Foundation ✅ COMPLETE
- ✅ Mount configuration changes (`mount_xfs_ex()`, `-rw` flag)
- ✅ chmod implementation (`xfs_setattr_mode()`)
- ✅ chown implementation (`xfs_setattr_owner()`)
- ✅ utimens implementation (`xfs_setattr_time()`)
- ✅ truncate implementation (`xfs_truncate_file()`)
- ✅ fsync implementation (`xfs_sync_file()`)
- ✅ Basic unit tests

**Implementation Notes:**
- All attribute operations use transaction pattern from design
- Read-only flag properly enforced in all FUSE handlers
- Timestamp updates include nanosecond precision where supported

### Phase 2: File I/O ✅ COMPLETE
- ✅ create implementation (`xfs_create_file()`)
- ✅ write implementation (`xfs_write_file()`)
- ✅ unlink implementation (`xfs_remove_file()`)
- ✅ mknod implementation (reuses `xfs_create_file()` with mode flags)
- ✅ File I/O integration tests

**Implementation Notes:**
- File creation handles regular files, device nodes, FIFOs, and sockets
- Write operations properly extend files and update timestamps
- Block allocation follows extent-based allocation strategy
- Unlink properly handles link count decrement and inode freeing

### Phase 3: Directories ✅ COMPLETE
- ✅ mkdir implementation (`xfs_create_dir()`)
- ✅ rmdir implementation (`xfs_remove_dir()`)
- ✅ rename implementation (`xfs_rename_entry()`)
- ✅ Directory integration tests

**Implementation Notes:**
- Directory creation initializes . and .. entries correctly
- Parent link counts updated for directory operations
- Rename handles same-directory and cross-directory moves
- Empty directory check performed before rmdir

### Phase 4: Links ✅ COMPLETE
- ✅ symlink implementation (`xfs_create_symlink()`)
- ✅ link implementation (`xfs_create_link()`)
- ✅ readlink verification (existing implementation confirmed working)
- ✅ Full integration tests
- ✅ Documentation complete

**Implementation Notes:**
- Symlink supports both inline (local) and extent-based storage
- Hard link enforces directory restriction and link count limits
- All link operations are transaction-protected

---

## 10. Implementation Deviations and Notes

### Deviations from Original Design

1. **mknod Implementation**: Rather than a separate `xfs_create_node()` function, the implementation reuses `xfs_create_file()` with appropriate mode flags (S_IFBLK, S_IFCHR, S_IFIFO, S_IFSOCK).

2. **Path Splitting**: The `xfs_path_split()` function was renamed to `xfs_lookup_parent()` which both splits the path and looks up the parent directory inode in one operation.

3. **Transaction Reservations**: Some transaction types use simplified reservation calculations compared to the detailed formulas in the design.

4. **Buffer Management**: Write operations use a simpler buffer management approach suitable for the userspace libxfs implementation.

### Additional Functions Implemented

- `xfs_is_readonly()` - Check if filesystem is mounted read-only
- `fuse_xfs_set_readonly()` / `fuse_xfs_get_readonly()` - Global read-only flag management
- `xfs_lookup_parent()` - Combined path split and parent lookup

### Testing Infrastructure

A comprehensive test suite was created in `tests/`:
- `test_write_operations.sh` - Main test script with all operations
- `run_tests.sh` - CI integration script
- Test coverage for all implemented operations
- Error handling verification

---

## Appendix A: Key LibXFS Function Reference

| Function | Purpose | Header |
|----------|---------|--------|
| `libxfs_trans_alloc` | Allocate transaction | libxfs.h |
| `libxfs_trans_reserve` | Reserve space | libxfs.h |
| `libxfs_trans_ijoin` | Join inode to transaction | libxfs.h |
| `libxfs_trans_ihold` | Hold inode in transaction | libxfs.h |
| `libxfs_trans_log_inode` | Log inode changes | libxfs.h |
| `libxfs_trans_commit` | Commit transaction | libxfs.h |
| `libxfs_trans_cancel` | Cancel transaction | libxfs.h |
| `libxfs_inode_alloc` | Allocate new inode | libxfs.h |
| `libxfs_ichgtime` | Update timestamps | libxfs.h |
| `libxfs_iget` | Get inode by number | libxfs.h |
| `libxfs_iput` | Release inode | libxfs.h |
| `libxfs_bmapi` | Block map operation | libxfs.h |
| `libxfs_bmap_finish` | Complete bmap operation | libxfs.h |
| `libxfs_dir_createname` | Create directory entry | libxfs.h |
| `libxfs_dir_removename` | Remove directory entry | libxfs.h |
| `libxfs_dir_init` | Initialize directory | libxfs.h |
| `libxfs_dir_lookup` | Look up entry in directory | libxfs.h |
| `libxfs_iunlink` | Add to unlinked list | libxfs.h |
| `libxfs_ifree` | Free inode | libxfs.h |

---

## Appendix B: Error Code Mapping

| XFS Error | FUSE Return | Description |
|-----------|-------------|-------------|
| 0 | 0 | Success |
| ENOENT | -ENOENT | No such file or directory |
| EEXIST | -EEXIST | File exists |
| ENOTDIR | -ENOTDIR | Not a directory |
| EISDIR | -EISDIR | Is a directory |
| ENOTEMPTY | -ENOTEMPTY | Directory not empty |
| ENOSPC | -ENOSPC | No space left on device |
| EROFS | -EROFS | Read-only filesystem |
| ENOMEM | -ENOMEM | Out of memory |
| EIO | -EIO | I/O error |
| EINVAL | -EINVAL | Invalid argument |
| EMLINK | -EMLINK | Too many links |
| ENAMETOOLONG | -ENAMETOOLONG | File name too long |

---

*Document Version: 1.0*
*Last Updated: December 2024*
*Author: FuseXFS Development Team*
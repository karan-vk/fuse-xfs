#ifndef __XFSUTIL_H__
#define __XFSUTIL_H__

#include <libc.h>
#include <xfs/libxfs.h>
#include <sys/stat.h>
#include <sys/dirent.h>

/*
 * Convert XFS directory file type (XFS_DIR3_FT_*) to POSIX DT_* type.
 * Used for V5/FTYPE-enabled filesystems.
 */
unsigned char xfs_ftype_to_dtype(__uint8_t ftype);

int
xfs_readdir(
            xfs_inode_t	*dp,
            void		*dirent,
            size_t		bufsize,
            xfs_off_t	*offset,
            filldir_t	filldir);

int xfs_readfile(xfs_inode_t *ip, void *buffer, off_t offset, size_t len, int *last_extent);
int xfs_readlink(xfs_inode_t *ip, void *buffer, off_t offset, size_t len, int *last_extent);
int xfs_stat(xfs_inode_t *inode, struct stat *stats);
int xfs_is_dir(xfs_inode_t *inode);
int xfs_is_link(xfs_inode_t *inode);
int xfs_is_regular(xfs_inode_t *inode);

int find_path(xfs_mount_t *mp, const char *path, xfs_inode_t **result);

/*
 * Mount/unmount operations
 */
/* Mount filesystem (default read-only for backward compatibility) */
xfs_mount_t *mount_xfs(char *progname, char *source_name);

/* Mount filesystem with explicit read-only flag */
xfs_mount_t *mount_xfs_ex(char *progname, char *source_name, int readonly);

/* Unmount filesystem with proper buffer flushing */
int unmount_xfs(xfs_mount_t *mp);

/* Check if filesystem is mounted read-only */
int xfs_is_readonly(xfs_mount_t *mp);

/*
 * Inode attribute operations (Phase 1)
 */
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

/*
 * Sync operations (Phase 1)
 */
/* Synchronize file data to disk */
int xfs_sync_file(xfs_inode_t *ip);

/* Synchronize entire filesystem */
int xfs_sync_fs(xfs_mount_t *mp);

/*
 * Path utility operations (Phase 2)
 */
/* Split a path into parent directory path and filename
 * Caller must free *parent and *name */
int xfs_path_split(const char *path, char **parent, char **name);

/* Look up parent directory by path and extract filename
 * On success, *parent_ip is the parent directory inode (caller must release) */
int xfs_lookup_parent(xfs_mount_t *mp, const char *path,
                      xfs_inode_t **parent_ip, char *name, size_t name_size);

/*
 * File creation operations (Phase 2)
 */
/* Create a new file (regular file, device node, FIFO, socket)
 * @param mp      - Mount point
 * @param dp      - Parent directory inode
 * @param name    - Name of new file
 * @param mode    - File mode (permissions + type: S_IFREG, S_IFBLK, S_IFCHR, S_IFIFO, S_IFSOCK)
 * @param rdev    - Device number (for block/char devices, 0 for others)
 * @param ipp     - Output: newly created inode (caller must release)
 * Returns 0 on success, negative errno on failure */
int xfs_create_file(xfs_mount_t *mp, xfs_inode_t *dp, const char *name,
                    mode_t mode, dev_t rdev, xfs_inode_t **ipp);

/*
 * File write operations (Phase 2)
 */
/* Write data to a file
 * @param ip      - File inode
 * @param buf     - Data buffer to write
 * @param offset  - Offset in file
 * @param size    - Number of bytes to write
 * Returns bytes written on success, negative errno on failure */
ssize_t xfs_write_file(xfs_inode_t *ip, const char *buf, off_t offset, size_t size);

/*
 * Directory creation operations (Phase 3)
 */
/* Create a new directory
 * @param mp      - Mount point
 * @param dp      - Parent directory inode
 * @param name    - Name of new directory
 * @param mode    - Directory mode (permissions)
 * @param ipp     - Output: newly created directory inode (caller must release)
 * Returns 0 on success, negative errno on failure */
int xfs_create_dir(xfs_mount_t *mp, xfs_inode_t *dp, const char *name,
                   mode_t mode, xfs_inode_t **ipp);

/*
 * File/directory removal operations (Phase 3)
 */
/* Remove a file (unlink)
 * @param mp      - Mount point
 * @param dp      - Parent directory inode
 * @param name    - Name of file to remove
 * @param ip      - Inode of file to remove (optional, will lookup if NULL)
 * Returns 0 on success, negative errno on failure */
int xfs_remove_file(xfs_mount_t *mp, xfs_inode_t *dp, const char *name,
                    xfs_inode_t *ip);

/* Remove an empty directory
 * @param mp      - Mount point
 * @param dp      - Parent directory inode
 * @param name    - Name of directory to remove
 * @param ip      - Directory inode to remove (optional, will lookup if NULL)
 * Returns 0 on success, negative errno on failure */
int xfs_remove_dir(xfs_mount_t *mp, xfs_inode_t *dp, const char *name,
                   xfs_inode_t *ip);

/*
 * Rename operations (Phase 3)
 */
/* Rename a file or directory
 * @param mp          - Mount point
 * @param src_dp      - Source parent directory inode
 * @param src_name    - Source name
 * @param dst_dp      - Destination parent directory inode
 * @param dst_name    - Destination name
 * Returns 0 on success, negative errno on failure */
int xfs_rename_entry(xfs_mount_t *mp, xfs_inode_t *src_dp,
                     const char *src_name, xfs_inode_t *dst_dp,
                     const char *dst_name);

/*
 * Link operations (Phase 4)
 */
/* Create a hard link to an existing file
 * @param mp        - Mount point
 * @param ip        - Existing inode to link to
 * @param newparent - Parent directory for the new link
 * @param newname   - Name of the new link
 * Returns 0 on success, negative errno on failure */
int xfs_create_link(xfs_mount_t *mp, xfs_inode_t *ip, xfs_inode_t *newparent,
                    const char *newname);

/* Create a symbolic link
 * @param mp      - Mount point
 * @param parent  - Parent directory inode
 * @param name    - Name of the symlink
 * @param target  - Target path (what the symlink points to)
 * @param ipp     - Output: newly created symlink inode
 * Returns 0 on success, negative errno on failure */
int xfs_create_symlink(xfs_mount_t *mp, xfs_inode_t *parent, const char *name,
                       const char *target, xfs_inode_t **ipp);

struct xfs_name first_name(const char *path);
struct xfs_name next_name(struct xfs_name current);

#endif /* __XFSUTIL_H__ */

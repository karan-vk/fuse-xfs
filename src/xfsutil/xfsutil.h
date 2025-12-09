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
xfs_mount_t *mount_xfs(char *progname, char *source_name);

struct xfs_name first_name(const char *path);
struct xfs_name next_name(struct xfs_name current);

#endif /* __XFSUTIL_H__ */

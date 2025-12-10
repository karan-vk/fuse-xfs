/*
 *  fuse_xfs.h
 *  fuse-xfs
 *
 *  Created by Alexandre Hardy on 4/16/11.
 *
 */

#ifndef __FUSE_XFS_H__
#define __FUSE_XFS_H__

#define XATTR_LIST_MAX  16

#include <xfsutil.h>

/*
 * FUSE mount options structure
 */
struct fuse_xfs_options {
    char *device;           /* Block device or image file */
    xfs_mount_t *xfs_mount; /* XFS mount structure */
    unsigned char readonly;  /* Mount read-only flag */
    unsigned char probeonly;
    unsigned char printlabel;
    unsigned char printuuid;
};

/*
 * Global mount accessor
 */
xfs_mount_t *current_xfs_mount(void);

/*
 * Read-only flag control
 */
void fuse_xfs_set_readonly(int readonly);
int fuse_xfs_get_readonly(void);

/*
 * FUSE operations structure (external reference)
 */
extern struct fuse_operations fuse_xfs_operations;

#endif /* __FUSE_XFS_H__ */

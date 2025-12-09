/*
 *  fuse_xfs.h
 *  fuse-xfs
 *
 *  Created by Alexandre Hardy on 4/16/11.
 *
 */

#define XATTR_LIST_MAX  16

#include <xfsutil.h>

struct fuse_xfs_options {
    char *device;
    xfs_mount_t *xfs_mount;
    unsigned char readonly;
    unsigned char probeonly;
    unsigned char printlabel;
    unsigned char printuuid;
};

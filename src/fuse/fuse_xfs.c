/*
 * fuse_xfs.c
 * fuse-xfs
 *
 * Created by Alexandre Hardy on 4/16/11.
 *
 */

#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fuse_xfs.h>
#include <xfsutil.h>

#ifdef DEBUG
#define log_debug printf
#else
#define log_debug(...)
#endif

xfs_mount_t *fuse_xfs_mp = NULL;

/* Global read-only flag - default to read-only for safety */
static int g_xfs_readonly = 1;

/* Helper function to check if filesystem is read-only */
static int check_readonly(void) {
    if (g_xfs_readonly || xfs_is_readonly(fuse_xfs_mp)) {
        return 1;
    }
    return 0;
}

xfs_mount_t *current_xfs_mount() {
    return fuse_xfs_mp;
}

static int
fuse_xfs_fgetattr(const char *path, struct stat *stbuf,
                  struct fuse_file_info *fi) {
    log_debug("fgetattr %s\n", path);
    
    int r;
    xfs_inode_t *inode=NULL;
    
    r = find_path(current_xfs_mount(), path, &inode);
    if (r) {
        return -ENOENT;
    }
    
    xfs_stat(inode, stbuf);

    if (xfs_is_dir(inode)) {
        log_debug("directory %s\n", path);
    }
    
    /*
     * CRITICAL FIX: Release the inode reference obtained from find_path().
     * find_path() calls libxfs_iget() which increments the inode's refcount.
     * We must call libxfs_iput() to decrement it, otherwise we leak references
     * and the inode cache will fill up, causing created files to become invisible.
     */
    libxfs_iput(inode, 0);
    
    return 0;
}

static int
fuse_xfs_getattr(const char *path, struct stat *stbuf) {
    log_debug("getattr %s\n", path);
    return fuse_xfs_fgetattr(path, stbuf, NULL);
}

static int
fuse_xfs_readlink(const char *path, char *buf, size_t size) {
    int r;
    xfs_inode_t *inode=NULL;

    log_debug("readlink %s\n", path);
    
    r = find_path(current_xfs_mount(), path, &inode);
    if (r) {
        return -ENOENT;
    }
    
    r = xfs_readlink(inode, buf, 0, size, NULL);
    /*
     * CRITICAL FIX: Always release the inode reference, even on error.
     * Previously, we leaked the inode reference on error paths.
     */
    libxfs_iput(inode, 0);
    if (r < 0) {
        return r;
    }
    return 0;
}

struct filler_info_struct {
    void *buf;
    fuse_fill_dir_t filler;
};

int fuse_xfs_filldir(void *filler_info, const char *name, int namelen, off_t offset, uint64_t inumber, unsigned flags) {
    int r;
    char dir_entry[256];
    xfs_inode_t *inode=NULL;    
    struct stat stbuf;
    struct stat *stats = NULL;
    struct filler_info_struct *filler_data = (struct filler_info_struct *) filler_info;
    
    memcpy(dir_entry, name, namelen);
    dir_entry[namelen] = '\0';
    if (libxfs_iget(current_xfs_mount(), NULL, inumber, 0, &inode, 0)) {
        return 0;
    }
    if (!xfs_stat(inode, &stbuf)) {
        stats = &stbuf;
    }
    log_debug("Direntry %s\n", dir_entry);
    r = filler_data->filler(filler_data->buf, dir_entry, stats, 0);
    libxfs_iput(inode, 0);
    return r;
}

static int
fuse_xfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi) {
    log_debug("readdir %s\n", path);
    int r;
    struct filler_info_struct filler_info;
    xfs_inode_t *inode=NULL;
    
    r = find_path(current_xfs_mount(), path, &inode);
    if (r) {
        return -ENOENT;
    }
    
    filler_info.buf = buf;
    filler_info.filler = filler;
    xfs_readdir(inode, (void *)&filler_info, 1024000, &offset, fuse_xfs_filldir);
    libxfs_iput(inode, 0);
    return 0;
}

/*
 * Create a special file (device node, FIFO, socket)
 */
static int
fuse_xfs_mknod(const char *path, mode_t mode, dev_t rdev) {
    xfs_mount_t *mp = current_xfs_mount();
    xfs_inode_t *dp = NULL;
    xfs_inode_t *ip = NULL;
    char name[MAXNAMELEN + 1];
    int error;
    
    log_debug("mknod %s mode=%o rdev=%d\n", path, mode, (int)rdev);
    
    /* Check if filesystem is read-only */
    if (check_readonly()) {
        return -EROFS;
    }
    
    /* Look up parent directory */
    error = xfs_lookup_parent(mp, path, &dp, name, sizeof(name));
    if (error) {
        return error;
    }
    
    /* Create the node */
    error = xfs_create_file(mp, dp, name, mode, rdev, &ip);
    
    libxfs_iput(dp, 0);
    
    if (error) {
        return error;
    }
    
    /* Release the new inode - we don't need to keep it open */
    libxfs_iput(ip, 0);
    
    return 0;
}

/*
 * Create a new directory
 */
static int
fuse_xfs_mkdir(const char *path, mode_t mode) {
    xfs_mount_t *mp = current_xfs_mount();
    xfs_inode_t *dp = NULL;
    xfs_inode_t *ip = NULL;
    char name[MAXNAMELEN + 1];
    int error;
    
    log_debug("mkdir %s mode=%o\n", path, mode);
    
    /* Check if filesystem is read-only */
    if (check_readonly()) {
        return -EROFS;
    }
    
    /* Look up parent directory */
    error = xfs_lookup_parent(mp, path, &dp, name, sizeof(name));
    if (error) {
        return error;
    }
    
    /* Create the directory */
    error = xfs_create_dir(mp, dp, name, mode, &ip);
    
    libxfs_iput(dp, 0);
    
    if (error) {
        return error;
    }
    
    /* Release the new inode - we don't need to keep it open */
    libxfs_iput(ip, 0);
    
    return 0;
}

/*
 * Remove a file (unlink)
 */
static int
fuse_xfs_unlink(const char *path) {
    xfs_mount_t *mp = current_xfs_mount();
    xfs_inode_t *dp = NULL;
    char name[MAXNAMELEN + 1];
    int error;
    
    log_debug("unlink %s\n", path);
    
    /* Check if filesystem is read-only */
    if (check_readonly()) {
        return -EROFS;
    }
    
    /* Look up parent directory */
    error = xfs_lookup_parent(mp, path, &dp, name, sizeof(name));
    if (error) {
        return error;
    }
    
    /* Remove the file */
    error = xfs_remove_file(mp, dp, name, NULL);
    
    libxfs_iput(dp, 0);
    
    return error;
}

/*
 * Remove an empty directory
 */
static int
fuse_xfs_rmdir(const char *path) {
    xfs_mount_t *mp = current_xfs_mount();
    xfs_inode_t *dp = NULL;
    char name[MAXNAMELEN + 1];
    int error;
    
    log_debug("rmdir %s\n", path);
    
    /* Check if filesystem is read-only */
    if (check_readonly()) {
        return -EROFS;
    }
    
    /* Look up parent directory */
    error = xfs_lookup_parent(mp, path, &dp, name, sizeof(name));
    if (error) {
        return error;
    }
    
    /* Remove the directory */
    error = xfs_remove_dir(mp, dp, name, NULL);
    
    libxfs_iput(dp, 0);
    
    return error;
}

/*
 * Create a symbolic link
 */
static int
fuse_xfs_symlink(const char *target, const char *linkpath) {
    xfs_mount_t *mp = current_xfs_mount();
    xfs_inode_t *dp = NULL;
    xfs_inode_t *ip = NULL;
    char name[MAXNAMELEN + 1];
    int error;
    
    log_debug("symlink %s -> %s\n", linkpath, target);
    
    /* Check if filesystem is read-only */
    if (check_readonly()) {
        return -EROFS;
    }
    
    /* Look up parent directory of the new symlink */
    error = xfs_lookup_parent(mp, linkpath, &dp, name, sizeof(name));
    if (error) {
        return error;
    }
    
    /* Create the symbolic link */
    error = xfs_create_symlink(mp, dp, name, target, &ip);
    
    libxfs_iput(dp, 0);
    
    if (error) {
        return error;
    }
    
    /* Release the new inode - we don't need to keep it open */
    libxfs_iput(ip, 0);
    
    return 0;
}

/*
 * Rename a file or directory
 */
static int
fuse_xfs_rename(const char *from, const char *to) {
    xfs_mount_t *mp = current_xfs_mount();
    xfs_inode_t *src_dp = NULL;
    xfs_inode_t *dst_dp = NULL;
    char src_name[MAXNAMELEN + 1];
    char dst_name[MAXNAMELEN + 1];
    int error;
    
    log_debug("rename %s -> %s\n", from, to);
    
    /* Check if filesystem is read-only */
    if (check_readonly()) {
        return -EROFS;
    }
    
    /* Look up source parent directory */
    error = xfs_lookup_parent(mp, from, &src_dp, src_name, sizeof(src_name));
    if (error) {
        return error;
    }
    
    /* Look up destination parent directory */
    error = xfs_lookup_parent(mp, to, &dst_dp, dst_name, sizeof(dst_name));
    if (error) {
        libxfs_iput(src_dp, 0);
        return error;
    }
    
    /* Perform the rename */
    error = xfs_rename_entry(mp, src_dp, src_name, dst_dp, dst_name);
    
    libxfs_iput(src_dp, 0);
    libxfs_iput(dst_dp, 0);
    
    return error;
}

static int
fuse_xfs_exchange(const char *path1, const char *path2, unsigned long options) {
    return -ENOSYS;
}

/*
 * Create a hard link
 */
static int
fuse_xfs_link(const char *oldpath, const char *newpath) {
    xfs_mount_t *mp = current_xfs_mount();
    xfs_inode_t *ip = NULL;
    xfs_inode_t *dp = NULL;
    char name[MAXNAMELEN + 1];
    int error;
    
    log_debug("link %s -> %s\n", oldpath, newpath);
    
    /* Check if filesystem is read-only */
    if (check_readonly()) {
        return -EROFS;
    }
    
    /* Look up the existing inode to link to */
    error = find_path(mp, oldpath, &ip);
    if (error) {
        return -ENOENT;
    }
    
    /* Look up parent directory of the new link */
    error = xfs_lookup_parent(mp, newpath, &dp, name, sizeof(name));
    if (error) {
        libxfs_iput(ip, 0);
        return error;
    }
    
    /* Create the hard link */
    error = xfs_create_link(mp, ip, dp, name);
    
    libxfs_iput(dp, 0);
    libxfs_iput(ip, 0);
    
    return error;
}

/*
 * Change file mode (permissions)
 */
static int
fuse_xfs_chmod(const char *path, mode_t mode) {
    xfs_inode_t *ip = NULL;
    int error;
    
    log_debug("chmod %s mode=%o\n", path, mode);
    
    /* Check if filesystem is read-only */
    if (check_readonly()) {
        return -EROFS;
    }
    
    /* Find the inode */
    error = find_path(current_xfs_mount(), path, &ip);
    if (error) {
        return -ENOENT;
    }
    
    /* Change the mode */
    error = xfs_setattr_mode(ip, mode);
    
    libxfs_iput(ip, 0);
    return error;
}

/*
 * Change file ownership
 */
static int
fuse_xfs_chown(const char *path, uid_t uid, gid_t gid) {
    xfs_inode_t *ip = NULL;
    int error;
    
    log_debug("chown %s uid=%d gid=%d\n", path, uid, gid);
    
    /* Check if filesystem is read-only */
    if (check_readonly()) {
        return -EROFS;
    }
    
    /* Find the inode */
    error = find_path(current_xfs_mount(), path, &ip);
    if (error) {
        return -ENOENT;
    }
    
    /* Change the ownership */
    error = xfs_setattr_owner(ip, uid, gid);
    
    libxfs_iput(ip, 0);
    return error;
}

/*
 * Update file timestamps
 */
static int
fuse_xfs_utimens(const char *path, const struct timespec tv[2]) {
    xfs_inode_t *ip = NULL;
    int error;
    
    log_debug("utimens %s\n", path);
    
    /* Check if filesystem is read-only */
    if (check_readonly()) {
        return -EROFS;
    }
    
    /* Find the inode */
    error = find_path(current_xfs_mount(), path, &ip);
    if (error) {
        return -ENOENT;
    }
    
    /* Update the timestamps */
    /* tv[0] is atime, tv[1] is mtime */
    error = xfs_setattr_time(ip, &tv[0], &tv[1]);
    
    libxfs_iput(ip, 0);
    return error;
}

/*
 * Truncate file to specified length
 */
static int
fuse_xfs_truncate(const char *path, off_t size) {
    xfs_inode_t *ip = NULL;
    int error;
    
    log_debug("truncate %s size=%lld\n", path, (long long)size);
    
    /* Check if filesystem is read-only */
    if (check_readonly()) {
        return -EROFS;
    }
    
    /* Find the inode */
    error = find_path(current_xfs_mount(), path, &ip);
    if (error) {
        return -ENOENT;
    }
    
    /* Truncate the file */
    error = xfs_truncate_file(ip, size);
    
    libxfs_iput(ip, 0);
    return error;
}

static int
fuse_xfs_getxtimes(const char *path, struct timespec *bkuptime,
                   struct timespec *crtime) {
    return -ENOENT;
}

/*
 * Create and open a new file
 */
static int
fuse_xfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    xfs_mount_t *mp = current_xfs_mount();
    xfs_inode_t *dp = NULL;
    xfs_inode_t *ip = NULL;
    char name[MAXNAMELEN + 1];
    int error;
    
    log_debug("create %s mode=%o\n", path, mode);
    
    /* Check if filesystem is read-only */
    if (check_readonly()) {
        return -EROFS;
    }
    
    /* Look up parent directory */
    error = xfs_lookup_parent(mp, path, &dp, name, sizeof(name));
    if (error) {
        return error;
    }
    
    /* Create the file (ensure it's a regular file) */
    error = xfs_create_file(mp, dp, name, (mode & ~S_IFMT) | S_IFREG, 0, &ip);
    
    libxfs_iput(dp, 0);
    
    if (error) {
        return error;
    }
    
    /* Store inode in file handle for subsequent operations */
    fi->fh = (uint64_t)ip;
    
    return 0;
}

static int
fuse_xfs_open(const char *path, struct fuse_file_info *fi) {
    int r;
    xfs_inode_t *inode=NULL;
    
    log_debug("open %s\n", path); 
    
    r = find_path(current_xfs_mount(), path, &inode);
    if (r) {
        return -ENOENT;
    }
    
    fi->fh = (uint64_t)inode;
    return 0;
}

static int
fuse_xfs_read(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi) {
    int r;
    log_debug("read %s\n", path); 
    r = xfs_readfile((xfs_inode_t *)fi->fh, buf, offset, size, NULL);
    return r;
}

/*
 * Write data to an open file
 */
static int
fuse_xfs_write(const char *path, const char *buf, size_t size,
               off_t offset, struct fuse_file_info *fi) {
    xfs_inode_t *ip;
    ssize_t result;
    
    log_debug("write %s size=%zu offset=%lld\n", path, size, (long long)offset);
    
    /* Check if filesystem is read-only */
    if (check_readonly()) {
        return -EROFS;
    }
    
    /* Get inode from file handle */
    ip = (xfs_inode_t *)fi->fh;
    
    if (ip == NULL) {
        return -EBADF;
    }
    
    /* Write the data */
    result = xfs_write_file(ip, buf, offset, size);
    
    if (result < 0) {
        return (int)result;
    }
    
    return (int)result;
}

static int
fuse_xfs_statfs(const char *path, struct statvfs *stbuf) {
    xfs_mount_t *mount = current_xfs_mount();
    
    memset(stbuf, 0, sizeof(*stbuf));
    stbuf->f_bsize = mount->m_sb.sb_blocksize;
    stbuf->f_frsize = mount->m_sb.sb_blocksize;
    stbuf->f_blocks =  mount->m_sb.sb_dblocks;
    stbuf->f_bfree =  mount->m_sb.sb_fdblocks;
    stbuf->f_files = mount->m_maxicount;
    stbuf->f_ffree = mount->m_sb.sb_ifree + mount->m_maxicount - mount->m_sb.sb_icount;
    stbuf->f_favail = stbuf->f_ffree;
    stbuf->f_namemax = MAXNAMELEN;
    stbuf->f_fsid = *((unsigned long*)mount->m_sb.sb_uuid);
    log_debug("f_bsize=%ld\n", stbuf->f_bsize);
    log_debug("f_frsize=%ld\n", stbuf->f_frsize);
    log_debug("f_blocks=%d\n", stbuf->f_blocks);
    log_debug("f_bfree=%d\n", stbuf->f_bfree);
    log_debug("f_files=%d\n", stbuf->f_files);
    log_debug("f_ffree=%d\n", stbuf->f_ffree);
    log_debug("f_favail=%d\n", stbuf->f_favail);
    log_debug("f_namemax=%ld\n", stbuf->f_namemax);
    log_debug("f_fsid=%ld\n", stbuf->f_fsid);
    return 0;
}

static int
fuse_xfs_flush(const char *path, struct fuse_file_info *fi) {
    return 0;
}

static int
fuse_xfs_release(const char *path, struct fuse_file_info *fi) {
    log_debug("release %s\n", path); 
    libxfs_iput((xfs_inode_t *)fi->fh, 0);
    return 0;
}

static int
fuse_xfs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi) {
    xfs_inode_t *ip;
    int error;
    
    log_debug("fsync %s datasync=%d\n", path, isdatasync);
    
    /* If we have a file handle, use it; otherwise look up the path */
    if (fi && fi->fh) {
        ip = (xfs_inode_t *)fi->fh;
        error = xfs_sync_file(ip);
    } else {
        error = find_path(current_xfs_mount(), path, &ip);
        if (error) {
            return -ENOENT;
        }
        error = xfs_sync_file(ip);
        libxfs_iput(ip, 0);
    }
    
    return error;
}

static int
fuse_xfs_setxattr(const char *path, const char *name, const char *value,
                  size_t size, int flags, uint32_t a) {
    return -ENOTSUP;
 }

static int
fuse_xfs_getxattr(const char *path, const char *name, char *value, size_t size, uint32_t a) {
    return -ENOATTR;
}

static int
fuse_xfs_listxattr(const char *path, char *list, size_t size) {
    return 0;
}

static int
fuse_xfs_removexattr(const char *path, const char *name) {
    return -ENOATTR;
}

void *
fuse_xfs_init(struct fuse_conn_info *conn) {
    //FUSE_ENABLE_XTIMES(conn);
    struct fuse_context *cntx=fuse_get_context();
    
    struct fuse_xfs_options *opts = (struct fuse_xfs_options *)cntx->private_data;
    
    if (opts == NULL) {
        return NULL;
    }
    
    //char *progname = "fuse-xfs";

    //fuse_xfs_mp = mount_xfs(progname, opts->device);
    fuse_xfs_mp = opts->xfs_mount;
    
    return fuse_xfs_mp;
}

void
fuse_xfs_destroy(void *userdata) {
    libxfs_umount(fuse_xfs_mp);
}

int fuse_xfs_opendir(const char *path, struct fuse_file_info *fi) {
    int r;
    xfs_inode_t *inode=NULL;
    log_debug("opendir %s\n", path); 
    
    r = find_path(current_xfs_mount(), path, &inode);
    if (r) {
        return -ENOENT;
    }
    
    libxfs_iput(inode, 0);
    return 0;
}

int fuse_xfs_releasedir(const char *path, struct fuse_file_info *fi) {
    log_debug("releasedir %s\n", path); 
    return 0;
}

/*
 * Set the global read-only flag
 */
void fuse_xfs_set_readonly(int readonly) {
    g_xfs_readonly = readonly;
}

/*
 * Get the global read-only flag
 */
int fuse_xfs_get_readonly(void) {
    return g_xfs_readonly;
}

struct fuse_operations fuse_xfs_operations = {
  .init        = fuse_xfs_init,
  .destroy     = fuse_xfs_destroy,
  .getattr     = fuse_xfs_getattr,
  .fgetattr    = fuse_xfs_fgetattr,
/*  .access      = fuse_xfs_access, */
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
  .chmod       = fuse_xfs_chmod,       /* Phase 1: chmod support */
  .chown       = fuse_xfs_chown,       /* Phase 1: chown support */
  .truncate    = fuse_xfs_truncate,    /* Phase 1: truncate support */
  .utimens     = fuse_xfs_utimens,     /* Phase 1: utimens support */
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
  //Not supported:
  //.exchange    = fuse_xfs_exchange,
  //.getxtimes   = fuse_xfs_getxtimes,
  //.setattr_x   = fuse_xfs_setattr_x,
  //.fsetattr_x  = fuse_xfs_fsetattr_x,
};

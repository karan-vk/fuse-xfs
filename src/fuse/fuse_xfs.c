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
    if (r < 0) {
        return r;
    }
    libxfs_iput(inode, 0);
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

static int
fuse_xfs_mknod(const char *path, mode_t mode, dev_t rdev) {
    return -ENOSYS;
}

static int
fuse_xfs_mkdir(const char *path, mode_t mode) {
    return -ENOSYS;
}

static int
fuse_xfs_unlink(const char *path) {
    return -ENOSYS;
}

static int
fuse_xfs_rmdir(const char *path) {
    return -ENOSYS;
}

static int
fuse_xfs_symlink(const char *from, const char *to) {
    return -ENOSYS;
}

static int
fuse_xfs_rename(const char *from, const char *to) {
    return -ENOSYS;
}

static int
fuse_xfs_exchange(const char *path1, const char *path2, unsigned long options) {
    return -ENOSYS;
}

static int
fuse_xfs_link(const char *from, const char *to) {
    return -ENOSYS;
}

static int
fuse_xfs_getxtimes(const char *path, struct timespec *bkuptime,
                   struct timespec *crtime) {
    return -ENOENT;
}

static int
fuse_xfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    return -ENOSYS;
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

static int
fuse_xfs_write(const char *path, const char *buf, size_t size,
               off_t offset, struct fuse_file_info *fi) {
    log_debug("write %s\n", path); 
    return -ENOSYS;
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
    return 0;
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

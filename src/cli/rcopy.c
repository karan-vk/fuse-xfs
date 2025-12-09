#include <xfsutil.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFSIZE 16384

struct filldir_data {
    xfs_mount_t *mp;
    char *base;
    char *local;
};

void copy_tree(xfs_mount_t *mp, char *parent, char *local, xfs_inode_t *inode);

int copy_filldir(void *dirents, const char *name, int namelen, off_t offset, uint64_t inumber, unsigned flags) {
    char dname[256];
    char lname[256];
    int r;
    xfs_inode_t *inode=NULL;
    struct stat fstats;
    char mode[]="rwxrwxrwx";
    int tests[]={S_IRUSR,S_IWUSR,S_IXUSR,S_IRGRP,S_IWGRP,S_IXGRP,S_IROTH,S_IWOTH,S_IXOTH};

    struct filldir_data *data = (struct filldir_data *) dirents;

    strcpy(dname, data->base);
    strcat(dname, "/");
    strncat(dname, name, namelen);

    strcpy(lname, data->local);
    strcat(lname, "/");
    strncat(lname, name, namelen);

    r = libxfs_iget(data->mp, NULL, inumber, 0, &inode, 0);
    if (r) printf("Panic! %d\n", r);
    r = xfs_stat(inode, &fstats);
    if (r) printf("Panic! stats %d\n", r);
    if (xfs_is_dir(inode)) {
        printf("d");
    } else if (xfs_is_link(inode)) {
        printf("l");
    } else printf("-");

    for (r=0; r<9; r++) {
        if (fstats.st_mode & tests[r]) {
            printf("%c", mode[r]);
        } else {
            printf("-");
        }
    }
    
    printf(" %s -> %s\n", dname, lname);
    if (strncmp(name, ".", namelen) == 0)
        libxfs_iput(inode, 0);
    else if (strncmp(name, "..", namelen) == 0)
        libxfs_iput(inode, 0);
    else {
        copy_tree(data->mp, dname, lname, inode);
        libxfs_iput(inode, 0);
    }
    return 0;
}

void copy_tree(xfs_mount_t *mp, char *parent, char *local, xfs_inode_t *inode) {
    xfs_off_t ofs;
    int r, fd;
    struct filldir_data filldata;
    char *buffer[BUFSIZE];
    off_t offset;

    if (xfs_is_dir(inode)) {
        mkdir(local, 0770);
        filldata.mp = mp;
        filldata.base = parent;
        filldata.local = local;
        ofs = 0;
        r = xfs_readdir(inode, (void *)&filldata, 102400, &ofs, copy_filldir);
        if (r != 0) {
            printf("Not a directory\n");
        }
    } else if (!xfs_is_link(inode)) {
        r = 10;
        offset = 0;
        fd = open(local, O_WRONLY|O_CREAT, 0660);
        if (fd < 0) {
            printf("Failed to open local file\n");
        } else {
            while (r) {
                r = xfs_readfile(inode, buffer, offset, BUFSIZE, NULL);
                if (r) {
                    write(fd, buffer, r);
                    offset += r;
                }
           }
        }
        close(fd);
    }
}

char *last(char *path) {
    char *ret = path;
    while (*path) {
        if (*path == '/') ret = path + 1;
        path++;
    }
    return ret;
}

int main(int argc, char *argv[]) {
    xfs_mount_t	*mp;
    xfs_inode_t *inode = NULL;
    char *progname;
    char *source_name;
    char *parent;
    int r;

    if (argc != 3) {
        printf("Usage: xfs-rcopy raw_device directory\n");
        printf("Copies the named directory from an XFS file system to the current directory\n");
        return 1;
    }
    progname = argv[0];
    source_name = argv[1];
    parent = argv[2];
    
    mp = mount_xfs(progname, source_name);
    
    if (mp == NULL) 
        return 1;
    
    r = find_path(mp, parent, &inode);
    if (r) {
        printf("Can't find %s\n", parent);
        libxfs_umount(mp);
        return 1;
    }

    copy_tree(mp, parent, last(parent), inode);
    libxfs_iput(inode, 0);
    libxfs_umount(mp);
    return 0;
}

/*
 * main.c
 * fuse-xfs
 *
 * Created by Alexandre Hardy on 4/16/11.
 *
 */
#include <fuse.h>
#include "fuse_xfs.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <xfsutil.h>

#define RAW_SECTOR_SIZE 512

extern struct fuse_operations fuse_xfs_operations;

void usage(int argc, char *argv[]) {
    fprintf(stderr, "fuse-xfs [-p] [-l] [-u] [-rw] device/file [-- fuse-opts]\n");
    fprintf(stderr, "         [-p]     Only probe if the device contains an XFS filesystem.\n");
    fprintf(stderr, "         [-l]     Print the label of the XFS filesystem.\n");
    fprintf(stderr, "         [-u]     Print the UUID of the XFS filesystem.\n");
    fprintf(stderr, "         [-rw]    Mount read-write (default is read-only).\n");
    fprintf(stderr, "         --       All options after -- are passed on to fuse.\n");
    fprintf(stderr, "                  The mount point must be supplied as the first argument.\n");
}

int parse_options(struct fuse_xfs_options* opts, int argc, char *argv[], int *new_argc, char *new_argv[]) {
    int i;
    
    if (argc < 2) {
        return 0;
    }
    
    opts->device = NULL;
    opts->readonly = 1;  /* Default to read-only for safety */
    
    new_argv[0] = argv[0];
        
    for (i=1; i<argc && strcmp(argv[i], "--"); i++) {
        if (!strcmp(argv[i], "-p")) {
            opts->probeonly = 1;
        }
        else if (!strcmp(argv[i], "-l")) {
            opts->printlabel = 1;
        }
        else if (!strcmp(argv[i], "-u")) {
            opts->printuuid = 1;
        }
        else if (!strcmp(argv[i], "-rw")) {
            opts->readonly = 0;  /* Enable read-write mode */
        }
        else opts->device = argv[i];
    }
    
    *new_argc = 1;
    i++;
    for (; i<argc; i++) {
        new_argv[*new_argc] = argv[i];
        (*new_argc)++;
    }
    
    return 1;
}

int xfs_probe(struct fuse_xfs_options* opts) {
    int status, fd;
    char magic[RAW_SECTOR_SIZE+1];
    struct stat statbuf;
    xfs_mount_t *fuse_xfs_mp;

    status = stat(opts->device, &statbuf);
    if (status != 0) {
        fprintf(stderr, "Failed to open %s\n", opts->device);
        return 0;
    }
    
    fd = open(opts->device, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open device");
        fprintf(stderr, "Failed to open %s for reading\n", opts->device);
        return 0;
    }
    
    status = read(fd, magic, RAW_SECTOR_SIZE);
    if (status != RAW_SECTOR_SIZE) {
        perror("Failure reading signature");
        fprintf(stderr, "Failed to read XFS signature on %s\n", opts->device);
        close(fd);
        return 0;
    }
    
    close(fd);
    
    if (strncmp(magic, "XFSB", 4)) {
        fprintf(stderr, "No XFS signature on %s\n", opts->device);
        close(fd);
        return 0;
    }
    
    /* Mount with appropriate read-only flag */
    fuse_xfs_mp = mount_xfs_ex("fuse-xfs", opts->device, opts->readonly);
    if (fuse_xfs_mp == NULL) {
        fprintf(stderr, "%s doesn't appear to have a valid XFS filesystem\n", opts->device);
        return 0;
    }
    
    opts->xfs_mount = fuse_xfs_mp;
    return 1;
}

int main(int argc, char* argv[], char* envp[], char** exec_path) {
    struct fuse_xfs_options opts;
    char *fuse_argv[256];
    char fsname[20];
    int fuse_argc = argc;
    int i;
    
    memset(&opts, 0, sizeof(opts));
    
    /* All arguments for xfs must precede fuse arguments */
    if (!parse_options(&opts, argc, argv, &fuse_argc, fuse_argv)) {
        usage(argc, argv);
        return 1;
    }
    
    /* All arguments for xfs must precede fuse arguments */
    if (!opts.device) {
        usage(argc, argv);
        return 1;
    }

    if (!xfs_probe(&opts)) {
        return 2;
    }
    
    if (opts.probeonly) {
        libxfs_umount(opts.xfs_mount);
        return 0;
    }

    if (opts.printlabel) {
        memcpy(fsname, opts.xfs_mount->m_sb.sb_fname, 12);
        fsname[12] = '\0';
        printf("%s\n", fsname);
        libxfs_umount(opts.xfs_mount);
        return 0;
    }

    if (opts.printuuid) {
        for (i=0; i<16; i++) {
            printf("%02x", opts.xfs_mount->m_sb.sb_uuid[i]);
            if ((i==3) || (i==5) || (i==7) || (i==9))
                printf("-");
        }
        printf("\n");
        libxfs_umount(opts.xfs_mount);
        return 0;
    }
    
    /* Set the global read-only flag for FUSE handlers */
    fuse_xfs_set_readonly(opts.readonly);
    
    if (!opts.readonly) {
        fprintf(stderr, "Mounting %s read-write\n", opts.device);
    }
        
    return fuse_main(fuse_argc, fuse_argv, &fuse_xfs_operations, &opts);
}

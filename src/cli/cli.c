#include <xfsutil.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <string.h>
#include <sys/stat.h>

#define BUFSIZE 16384

struct filldir_data {
    xfs_mount_t *mp;
    xfs_inode_t *base;
};

void print_int(uint64_t size, int chars) {
    char result[256];
    int i, n;

    sprintf(result, "%lld", size);
    n = strlen(result);
    printf(" ");
    for (i=n; i<chars; i++) printf(" ");
    printf("%s", result);
}

int cli_ls_xfs_filldir(void *dirents, const char *name, int namelen, off_t offset, uint64_t inumber, unsigned flags) {
    char dname[256];
    char symlink[256];
    int r;
    xfs_inode_t *inode=NULL;
    struct stat fstats;
    char mode[]="rwxrwxrwx";
    int tests[]={S_IRUSR,S_IWUSR,S_IXUSR,S_IRGRP,S_IWGRP,S_IXGRP,S_IROTH,S_IWOTH,S_IXOTH};

    struct filldir_data *data = (struct filldir_data *) dirents;
    memcpy(dname, name, namelen);
    dname[namelen] = '\0';
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
    
    print_int(fstats.st_uid, 6);
    print_int(fstats.st_gid, 6);
    print_int(fstats.st_size, 12);
    
    printf(" %s", dname);
    if (xfs_is_link(inode)) {
        r = xfs_readlink(inode, symlink, 0, 255, NULL);
        if (r > 0) {
            symlink[r] = '\0';
            printf("->%s", symlink);
        }
    }
    printf("\n");
    libxfs_iput(inode, 0);
    return 0;
}

char *completions[65536];
int ncompletions = 0;

int cli_complete_xfs_filldir(void *dirents, const char *name, int namelen, off_t offset, uint64_t inumber, unsigned flags) {
    completions[ncompletions] = malloc(namelen + 1);
    memcpy(completions[ncompletions], name, namelen);
    completions[ncompletions][namelen] = '\0';
    ncompletions++;
    return 0;
}

char *dir_generator(char* text, int state) {
    static int iter=0;
    static int len;

    if (state == 0) iter = 2;
    len = strlen(text);

    while (iter < ncompletions) {
        if (strncmp(completions[iter], text, len) == 0) {
            return strdup(completions[iter++]); 
        }
        iter++;
    }
    return NULL;
}

static char *get_prompt(const char *path) {
    static char prompt[FILENAME_MAX + 2 /*"> "*/ + 1 /*"\0"*/ ];
    
    snprintf(prompt, sizeof(prompt), "%s> ", path);
    return prompt;  
}

void free_completions(void) {
    int i;

    for (i=0; i<ncompletions; i++) {
        free(completions[i]);
    }
    ncompletions = 0;
}

char *fetchline(xfs_mount_t *mp, char *path) {
    xfs_inode_t *inode;
    xfs_off_t ofs = 0;
    char *line;
    int r;

    free_completions();
    r = find_path(mp, path, &inode);
    if (r == 0) {
        r = xfs_readdir(inode, NULL, 102400, &ofs, cli_complete_xfs_filldir);
        libxfs_iput(inode, 0);
    }

    rl_completion_entry_function = (Function *)dir_generator;
    rl_bind_key('\t', rl_complete);
    line = readline(get_prompt(path));
    if (line && *line)
        add_history(line);
    return line;
}

void goto_parent(char *path) {
    int pos;
    if (strcmp(path, "/") == 0) return;
    pos = strlen(path) - 2;
    while (path[pos] != '/') pos--;
    path[pos+1] = '\0';
}

void strip(char *buffer, char c) {
    int ptr = strlen(buffer) - 1;
    while ((ptr >= 0) && (buffer[ptr] == c)) {
        buffer[ptr] = '\0';
        ptr--;
    }
}

int main(int argc, char *argv[]) {
    xfs_mount_t	*mp;
    xfs_inode_t *inode = NULL;
    xfs_off_t ofs;
    struct filldir_data filldata;
    char *progname;
    char *source_name;
    int r, fd;
    char *line;
    char path[FILENAME_MAX] = "/";
    char newpath[FILENAME_MAX] = "/";
    char *buffer[BUFSIZE];
    off_t offset;

    if (argc != 2) {
        printf("Usage: xfs-cli raw_device\n");
        return 1;
    }
    progname = argv[0];
    source_name = argv[1];
    
    mp = mount_xfs(progname, source_name);
    
    if (mp == NULL) 
        return 1;
    
    while (line = fetchline(mp, path)) {
        inode = NULL;
        strip(line, ' ');
        if (strncmp(line, "cd ", 3) == 0) {
            if (strcmp(line+3, "..") == 0) {
                goto_parent(path);
            } else {
                strcpy(newpath, path);
                strcat(newpath, line+3);
                r = find_path(mp, newpath, &inode);
                if ((!r) && (xfs_is_dir(inode))) {
                    //TODO: check if it is a directory
                    strcpy(path, newpath);
                    strcat(path, "/");
                    printf("%s\n", path);
                } else {
                    if (r) {
                        printf("No such directory\n");
                    } else {
                        printf("Not a directory\n");
                    }
                }
            }
        }
        else if (strcmp(line, "ls") == 0) {
            r = find_path(mp, path, &inode);
            if (!r) {
                //TODO: check if it is a directory
                ofs=0;
                filldata.mp = mp;
                filldata.base = inode;
                r = xfs_readdir(inode, (void *)&filldata, 102400, &ofs, cli_ls_xfs_filldir);
                if (r != 0) {
                    printf("Not a directory\n");
                }
            } else {
                printf("No such directory\n");
            }
        }
        else if (strncmp(line, "cat ", 4) == 0) {
            strcpy(newpath, path);
            strcat(newpath, line+4);
            r = find_path(mp, newpath, &inode);
            if (r)
                printf("File not found\n");
            else if (xfs_is_regular(inode)) {
                //TODO: check if it is a file
                r = 10;
                offset = 0;
                while (r) {
                    r = xfs_readfile(inode, buffer, offset, BUFSIZE, NULL);
                    if (r) {
                        write(1, buffer, r);
                        offset += r;
                    }
                }
            } else if (xfs_is_link(inode)) {
                r = 10;
                offset = 0;
                while (r) {
                    r = xfs_readlink(inode, buffer, offset, BUFSIZE, NULL);
                    if (r) {
                        write(1, buffer, r);
                        offset += r;
                    }
                }
            } else {
                printf("Not a regular file\n");
            }
        }
        else if (strncmp(line, "get ", 4) == 0) {
            strcpy(newpath, path);
            strcat(newpath, line+4);
            r = find_path(mp, newpath, &inode);
            if ((!r) && (xfs_is_regular(inode))) {
                //TODO: check if it is a file
                r = 10;
                offset = 0;
                fd = open(line+4, O_WRONLY|O_CREAT, 0660);
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
                    close(fd);
                    printf("Retrieved %lld bytes\n", offset);
                }
            } else {
                if (r) 
                    printf("File not found\n");
                else
                    printf("Not a regular file\n");
            }
        }
        else if (strcmp(line, "exit") == 0) {
            libxfs_umount(mp);
            return 0;
        }
        else if (strcmp(line, "pwd") == 0) {
            printf("%s\n", path);
        } else {
            printf("Unknown command\n");
        }
        free(line);
        if (inode) {
            libxfs_iput(inode, 0);
        }
    }
    libxfs_umount(mp);
    return 0;
}

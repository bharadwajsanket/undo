#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <time.h>
#include "utils.h"

// Path resolution functions
void get_undo_dir(char *buf, size_t len) {
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        }
    }
    if (!home) {
        home = ".";
    }
    snprintf(buf, len, "%s/.undo", home);
}

void get_config_path(char *buf, size_t len) {
    char dir[1024];
    get_undo_dir(dir, sizeof(dir));
    snprintf(buf, len, "%s/config", dir);
}

void get_journal_path(char *buf, size_t len) {
    char dir[1024];
    get_undo_dir(dir, sizeof(dir));
    snprintf(buf, len, "%s/journal.log", dir);
}

void get_objects_dir(char *buf, size_t len) {
    char dir[1024];
    get_undo_dir(dir, sizeof(dir));
    snprintf(buf, len, "%s/objects", dir);
}

int get_absolute_path_safe(const char *path, char *abs_path, size_t abs_len) {
    char parent[4096];
    char base[256];
    
    // Separate directory and filename
    strncpy(parent, path, sizeof(parent));
    parent[sizeof(parent) - 1] = '\0';
    
    char *last_slash = strrchr(parent, '/');
    if (last_slash) {
        strncpy(base, last_slash + 1, sizeof(base));
        base[sizeof(base) - 1] = '\0';
        if (last_slash == parent) {
            strcpy(parent, "/");
        } else {
            *last_slash = '\0';
        }
    } else {
        strcpy(parent, ".");
        strncpy(base, path, sizeof(base));
        base[sizeof(base) - 1] = '\0';
    }
    
    char real_parent[4096];
    if (realpath(parent, real_parent) == NULL) {
        return -1;
    }
    
    if (strcmp(real_parent, "/") == 0) {
        snprintf(abs_path, abs_len, "/%s", base);
    } else {
        snprintf(abs_path, abs_len, "%s/%s", real_parent, base);
    }
    return 0;
}

// Directory utility to create folders recursively (equivalent to mkdir -p)
int make_dir_recursive(const char *path, mode_t mode) {
    char tmp[4096];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len == 0) return 0;
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            struct stat st;
            if (stat(tmp, &st) != 0) {
                if (mkdir(tmp, mode) != 0) {
                    return -1;
                }
            } else if (!S_ISDIR(st.st_mode)) {
                return -1;
            }
            *p = '/';
        }
    }
    struct stat st;
    if (stat(tmp, &st) != 0) {
        if (mkdir(tmp, mode) != 0) {
            return -1;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        return -1;
    }
    return 0;
}

// Hex encoding / decoding for safe logging of weird paths
void string_to_hex(const char *src, char *dst) {
    while (*src) {
        sprintf(dst, "%02x", (unsigned char)*src);
        src++;
        dst += 2;
    }
    *dst = '\0';
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void hex_to_string(const char *src, char *dst) {
    while (*src && *(src + 1)) {
        int h = hex_val(*src);
        int l = hex_val(*(src + 1));
        if (h >= 0 && l >= 0) {
            *dst = (char)((h << 4) | l);
            dst++;
        }
        src += 2;
    }
    *dst = '\0';
}

// Random ID generation (6-hex-digit string)
void generate_tx_id(char *buf) {
    unsigned char bytes[3];
    int success = 0;
    FILE *urandom = fopen("/dev/urandom", "rb");
    if (urandom) {
        if (fread(bytes, 1, 3, urandom) == 3) {
            success = 1;
        }
        fclose(urandom);
    }
    if (!success) {
        static bool seeded = false;
        if (!seeded) {
            srand(time(NULL) ^ getpid());
            seeded = true;
        }
        bytes[0] = rand() & 0xFF;
        bytes[1] = rand() & 0xFF;
        bytes[2] = rand() & 0xFF;
    }
    sprintf(buf, "%02x%02x%02x", bytes[0], bytes[1], bytes[2]);
}

// File size queries
long long get_file_size(const char *path) {
    struct stat st;
    if (lstat(path, &st) == 0) {
        return (long long)st.st_size;
    }
    return -1;
}

long long get_dir_size_recursive(const char *path) {
    long long total_size = 0;
    DIR *dir = opendir(path);
    if (!dir) return 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char subpath[4096];
        snprintf(subpath, sizeof(subpath), "%s/%s", path, entry->d_name);
        struct stat st;
        if (lstat(subpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                total_size += get_dir_size_recursive(subpath);
            } else {
                total_size += st.st_size;
            }
        }
    }
    closedir(dir);
    return total_size;
}

// File copying (regular files)
int copy_file(const char *src, const char *dst, mode_t mode) {
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    char buf[32768];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }
    fflush(out);
    int fd = fileno(out);
    if (fd != -1) {
        fsync(fd);
    }
    fclose(in);
    fclose(out);
    
    chmod(dst, mode);
    return 0;
}

// Recursive directory copy
int copy_dir_recursive(const char *src, const char *dst) {
    struct stat src_st;
    if (lstat(src, &src_st) != 0) return -1;
    if (mkdir(dst, src_st.st_mode) != 0) {
        struct stat dst_st;
        if (lstat(dst, &dst_st) != 0 || !S_ISDIR(dst_st.st_mode)) {
            return -1;
        }
    }
    
    DIR *dir = opendir(src);
    if (!dir) return -1;
    struct dirent *entry;
    int status = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char src_sub[4096];
        char dst_sub[4096];
        snprintf(src_sub, sizeof(src_sub), "%s/%s", src, entry->d_name);
        snprintf(dst_sub, sizeof(dst_sub), "%s/%s", dst, entry->d_name);
        
        struct stat st;
        if (lstat(src_sub, &st) != 0) {
            status = -1;
            break;
        }
        
        if (S_ISLNK(st.st_mode)) {
            char target[1024];
            ssize_t len = readlink(src_sub, target, sizeof(target) - 1);
            if (len == -1) {
                status = -1;
                break;
            }
            target[len] = '\0';
            if (symlink(target, dst_sub) != 0) {
                status = -1;
                break;
            }
        } else if (S_ISDIR(st.st_mode)) {
            if (copy_dir_recursive(src_sub, dst_sub) != 0) {
                status = -1;
                break;
            }
        } else {
            if (copy_file(src_sub, dst_sub, st.st_mode) != 0) {
                status = -1;
                break;
            }
        }
    }
    closedir(dir);
    
    int dfd = open(dst, O_RDONLY);
    if (dfd != -1) {
        fsync(dfd);
        close(dfd);
    }
    return status;
}

// Recursive directory deletion
int remove_dir_recursive(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return -1;
    struct dirent *entry;
    int status = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char subpath[4096];
        snprintf(subpath, sizeof(subpath), "%s/%s", path, entry->d_name);
        struct stat st;
        if (lstat(subpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                if (remove_dir_recursive(subpath) != 0) {
                    status = -1;
                }
            } else {
                if (unlink(subpath) != 0) {
                    status = -1;
                }
            }
        }
    }
    closedir(dir);
    if (rmdir(path) != 0) {
        status = -1;
    }
    return status;
}

// Prompts
int confirm_prompt(const char *message) {
    printf("%s [y/N] ", message);
    fflush(stdout);
    char buf[64];
    if (!fgets(buf, sizeof(buf), stdin)) {
        return 0;
    }
    // Trim newline/whitespace
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r' || buf[len-1] == ' ' || buf[len-1] == '\t')) {
        buf[--len] = '\0';
    }
    if (strcasecmp(buf, "y") == 0 || strcasecmp(buf, "yes") == 0) {
        return 1;
    }
    return 0;
}

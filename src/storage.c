#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <zlib.h>
#include "storage.h"
#include "utils.h"
#include "config.h"

// Helper to format file sizes
static void format_size(long long size, char *buf, size_t len) {
    if (size < 1024) {
        snprintf(buf, len, "%lld B", size);
    } else if (size < 1024 * 1024) {
        snprintf(buf, len, "%.1f KB", (double)size / 1024.0);
    } else if (size < 1024 * 1024 * 1024) {
        snprintf(buf, len, "%.1f MB", (double)size / (1024.0 * 1024.0));
    } else {
        snprintf(buf, len, "%.1f GB", (double)size / (1024.0 * 1024.0 * 1024.0));
    }
}

// Find a transaction by ID in our loaded array
static int find_tx(Transaction *list, int count, const char *id) {
    for (int i = 0; i < count; i++) {
        if (strcmp(list[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

int load_transactions(Transaction **txs, int *count) {
    char journal_path[1024];
    get_journal_path(journal_path, sizeof(journal_path));
    
    FILE *f = fopen(journal_path, "r");
    if (!f) {
        *txs = NULL;
        *count = 0;
        return 0;
    }
    
    Transaction *list = NULL;
    int cap = 0;
    int cnt = 0;
    
    char line[16384];
    while (fgets(line, sizeof(line), f)) {
        char cmd[32];
        char tx_id[64];
        int parsed = sscanf(line, "%31s %63s", cmd, tx_id);
        if (parsed < 2) continue;
        
        // Remove trailing newline or spaces from command/tx_id
        cmd[31] = '\0';
        tx_id[6] = '\0'; // transaction IDs are 6 chars long
        
        if (strcmp(cmd, "START") == 0) {
            int idx = find_tx(list, cnt, tx_id);
            if (idx == -1) {
                if (cnt >= cap) {
                    cap = cap == 0 ? 16 : cap * 2;
                    Transaction *new_list = realloc(list, cap * sizeof(Transaction));
                    if (!new_list) {
                        fclose(f);
                        free_transactions(list, cnt);
                        return -1;
                    }
                    list = new_list;
                }
                idx = cnt++;
                strcpy(list[idx].id, tx_id);
                list[idx].file_count = 0;
                list[idx].files = NULL;
            }
            list[idx].state = TX_PENDING;
            
            long long ts = 0;
            if (sscanf(line, "%*s %*s %lld", &ts) == 1) {
                list[idx].timestamp = (time_t)ts;
            } else {
                list[idx].timestamp = time(NULL);
            }
        } 
        else if (strcmp(cmd, "FILE") == 0) {
            int idx = find_tx(list, cnt, tx_id);
            if (idx != -1) {
                int index;
                char hex_path[8192];
                long long size;
                unsigned int mode;
                int is_dir, is_symlink, comp;
                
                int n = sscanf(line, "%*s %*s %d %8191s %lld %o %d %d %d", 
                               &index, hex_path, &size, &mode, &is_dir, &is_symlink, &comp);
                if (n == 7) {
                    Transaction *tx = &list[idx];
                    tx->files = realloc(tx->files, (tx->file_count + 1) * sizeof(TxFile));
                    TxFile *tf = &tx->files[tx->file_count];
                    tf->index = index;
                    hex_to_string(hex_path, tf->original_path);
                    tf->size = size;
                    tf->mode = (mode_t)mode;
                    tf->is_dir = is_dir;
                    tf->is_symlink = is_symlink;
                    tf->compression_status = comp;
                    tx->file_count++;
                }
            }
        } 
        else if (strcmp(cmd, "COMMIT") == 0) {
            int idx = find_tx(list, cnt, tx_id);
            if (idx != -1) {
                list[idx].state = TX_COMMITTED;
            }
        } 
        else if (strcmp(cmd, "ABORT") == 0) {
            int idx = find_tx(list, cnt, tx_id);
            if (idx != -1) {
                list[idx].state = TX_ABORTED;
            }
        } 
        else if (strcmp(cmd, "UNDO") == 0) {
            int idx = find_tx(list, cnt, tx_id);
            if (idx != -1) {
                list[idx].state = TX_UNDONE;
            }
        }
    }
    
    fclose(f);
    *txs = list;
    *count = cnt;
    return 0;
}

void free_transactions(Transaction *txs, int count) {
    if (!txs) return;
    for (int i = 0; i < count; i++) {
        free(txs[i].files);
    }
    free(txs);
}

int recover_crashed_transactions(void) {
    Transaction *txs = NULL;
    int count = 0;
    if (load_transactions(&txs, &count) != 0) {
        return 0;
    }
    
    char journal_path[1024];
    get_journal_path(journal_path, sizeof(journal_path));
    
    char objects_dir[1024];
    get_objects_dir(objects_dir, sizeof(objects_dir));
    
    FILE *jf = NULL; // Open append-only log to write ABORT entries if needed
    
    int recovered = 0;
    for (int i = 0; i < count; i++) {
        if (txs[i].state == TX_PENDING) {
            printf("undo: recovering crashed transaction %s...\n", txs[i].id);
            
            // Open journal for appending if not already open
            if (!jf) {
                jf = fopen(journal_path, "a");
                if (!jf) {
                    fprintf(stderr, "undo: failed to open journal for rollback\n");
                    free_transactions(txs, count);
                    return -1;
                }
            }
            
            // For each file in transaction, see if it was moved to objects
            // and move it back if it is missing from the original path.
            for (int j = 0; j < txs[i].file_count; j++) {
                TxFile *tf = &txs[i].files[j];
                char obj_path[4096];
                snprintf(obj_path, sizeof(obj_path), "%s/%s/%d", objects_dir, txs[i].id, tf->index);
                
                struct stat st;
                int obj_exists = (lstat(obj_path, &st) == 0);
                int orig_exists = (lstat(tf->original_path, &st) == 0);
                
                if (obj_exists) {
                    if (!orig_exists) {
                        // Restore it
                        // Create parent directories if missing
                        char parent[4096];
                        strncpy(parent, tf->original_path, sizeof(parent));
                        parent[sizeof(parent) - 1] = '\0';
                        char *last_slash = strrchr(parent, '/');
                        if (last_slash) {
                            *last_slash = '\0';
                            make_dir_recursive(parent, 0755);
                        }
                        
                        // Move it back
                        if (rename(obj_path, tf->original_path) != 0) {
                            // Cross-device rename fallback
                            if (tf->is_dir) {
                                copy_dir_recursive(obj_path, tf->original_path);
                                remove_dir_recursive(obj_path);
                            } else {
                                copy_file(obj_path, tf->original_path, tf->mode);
                                unlink(obj_path);
                            }
                        }
                    } else {
                        // Original exists, we can delete the object file duplicate
                        if (tf->is_dir) {
                            remove_dir_recursive(obj_path);
                        } else {
                            unlink(obj_path);
                        }
                    }
                }
            }
            
            // Delete the transaction directory in objects
            char tx_obj_dir[4096];
            snprintf(tx_obj_dir, sizeof(tx_obj_dir), "%s/%s", objects_dir, txs[i].id);
            rmdir(tx_obj_dir);
            
            // Append ABORT to journal
            fprintf(jf, "ABORT %s\n", txs[i].id);
            fflush(jf);
            int fd = fileno(jf);
            if (fd != -1) {
                fsync(fd);
            }
            printf("undo: transaction %s successfully rolled back.\n", txs[i].id);
            recovered++;
        }
    }
    
    if (jf) {
        fclose(jf);
    }
    
    free_transactions(txs, count);
    return recovered;
}

// Compress a file using zlib
static int compress_file_zlib(const char *src, const char *dst, mode_t mode) {
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    gzFile out = gzopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    
    char buf[16384];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (gzwrite(out, buf, n) <= 0) {
            fclose(in);
            gzclose(out);
            return -1;
        }
    }
    
    fclose(in);
    if (gzclose(out) != Z_OK) {
        return -1;
    }
    
    chmod(dst, mode);
    return 0;
}

// Decompress a file using zlib
static int decompress_file_zlib(const char *src, const char *dst, mode_t mode) {
    gzFile in = gzopen(src, "rb");
    if (!in) return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) {
        gzclose(in);
        return -1;
    }
    
    char buf[16384];
    int len;
    while ((len = gzread(in, buf, sizeof(buf))) > 0) {
        if (fwrite(buf, 1, len, out) != (size_t)len) {
            gzclose(in);
            fclose(out);
            return -1;
        }
    }
    
    gzclose(in);
    fclose(out);
    chmod(dst, mode);
    return 0;
}

// Check if compression would be beneficial on first 256 KB
static int is_compression_beneficial(const char *path, long long file_size) {
    if (file_size < 1024) return 0; // Don't check tiny files
    
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    
    unsigned char src_buf[262144]; // 256 KB
    size_t bytes_read = fread(src_buf, 1, sizeof(src_buf), f);
    fclose(f);
    
    if (bytes_read < 1024) return 0;
    
    uLongf dest_len = compressBound(bytes_read);
    unsigned char *dest_buf = malloc(dest_len);
    if (!dest_buf) return 0;
    
    int res = compress(dest_buf, &dest_len, src_buf, bytes_read);
    int beneficial = 0;
    if (res == Z_OK) {
        if (dest_len < (bytes_read * 90) / 100) {
            beneficial = 1;
        }
    }
    free(dest_buf);
    return beneficial;
}

int create_transaction(const char *tx_id, int file_count, char **paths, const Config *cfg, int force, int verbose) {
    // 1. Gather all files and resolve absolute paths, verify exists
    char **abs_paths = malloc(file_count * sizeof(char *));
    long long *sizes = malloc(file_count * sizeof(long long));
    struct stat *stats = malloc(file_count * sizeof(struct stat));
    int *is_dirs = malloc(file_count * sizeof(int));
    int *is_symlinks = malloc(file_count * sizeof(int));
    
    int valid_count = 0;
    long long total_size = 0;
    
    for (int i = 0; i < file_count; i++) {
        char abs_path[4096];
        struct stat st;
        
        // Use safe absolute path finder that doesn't follow symlinks
        if (get_absolute_path_safe(paths[i], abs_path, sizeof(abs_path)) != 0 || lstat(abs_path, &st) != 0) {
            if (force) {
                continue; // Ignore nonexistent files under force mode
            } else {
                fprintf(stderr, "undo: cannot remove '%s': No such file or directory\n", paths[i]);
                free(abs_paths);
                free(sizes);
                free(stats);
                free(is_dirs);
                free(is_symlinks);
                return -1;
            }
        }
        
        // Prevent deleting UNDO storage itself
        char undo_dir[1024];
        get_undo_dir(undo_dir, sizeof(undo_dir));
        if (strncmp(abs_path, undo_dir, strlen(undo_dir)) == 0) {
            fprintf(stderr, "undo: operation not permitted: attempting to delete UNDO internal storage '%s'\n", paths[i]);
            // Free allocated memory
            for (int k = 0; k < valid_count; k++) free(abs_paths[k]);
            free(abs_paths);
            free(sizes);
            free(stats);
            free(is_dirs);
            free(is_symlinks);
            return -1;
        }
        
        abs_paths[valid_count] = strdup(abs_path);
        stats[valid_count] = st;
        is_symlinks[valid_count] = S_ISLNK(st.st_mode);
        is_dirs[valid_count] = S_ISDIR(st.st_mode);
        
        long long sz = 0;
        if (is_dirs[valid_count]) {
            sz = get_dir_size_recursive(abs_path);
        } else {
            sz = st.st_size;
        }
        sizes[valid_count] = sz;
        total_size += sz;
        
        valid_count++;
    }
    
    if (valid_count == 0) {
        free(abs_paths);
        free(sizes);
        free(stats);
        free(is_dirs);
        free(is_symlinks);
        return 0; // Nothing to delete, equivalent to rm -f on nonexistent files
    }
    
    // 2. Perform large file prompt check
    if (total_size > cfg->large_file_threshold) {
        char sz_buf[64];
        format_size(total_size, sz_buf, sizeof(sz_buf));
        printf("Warning: Total deletion size is %s (exceeds prompt threshold of %lld MB).\n", 
               sz_buf, cfg->large_file_threshold / (1024 * 1024));
        if (!confirm_prompt("Store in UNDO anyway?")) {
            if (confirm_prompt("Delete permanently (cannot be undone)?")) {
                // Delete permanently
                for (int i = 0; i < valid_count; i++) {
                    if (is_dirs[i]) {
                        remove_dir_recursive(abs_paths[i]);
                    } else {
                        unlink(abs_paths[i]);
                    }
                }
                for (int k = 0; k < valid_count; k++) free(abs_paths[k]);
                free(abs_paths);
                free(sizes);
                free(stats);
                free(is_dirs);
                free(is_symlinks);
                return 0;
            } else {
                printf("Operation cancelled.\n");
                for (int k = 0; k < valid_count; k++) free(abs_paths[k]);
                free(abs_paths);
                free(sizes);
                free(stats);
                free(is_dirs);
                free(is_symlinks);
                return 0;
            }
        }
    }
    
    // 3. Write START to journal and fsync
    char journal_path[1024];
    get_journal_path(journal_path, sizeof(journal_path));
    
    char objects_dir[1024];
    get_objects_dir(objects_dir, sizeof(objects_dir));
    make_dir_recursive(objects_dir, 0700);
    
    char tx_dir[4096];
    snprintf(tx_dir, sizeof(tx_dir), "%s/%s", objects_dir, tx_id);
    if (mkdir(tx_dir, 0700) != 0) {
        fprintf(stderr, "undo: failed to create transaction directory %s\n", tx_dir);
        for (int k = 0; k < valid_count; k++) free(abs_paths[k]);
        free(abs_paths); free(sizes); free(stats); free(is_dirs); free(is_symlinks);
        return -1;
    }
    
    FILE *jf = fopen(journal_path, "a");
    if (!jf) {
        fprintf(stderr, "undo: failed to append to journal\n");
        rmdir(tx_dir);
        for (int k = 0; k < valid_count; k++) free(abs_paths[k]);
        free(abs_paths); free(sizes); free(stats); free(is_dirs); free(is_symlinks);
        return -1;
    }
    
    fprintf(jf, "START %s %lld\n", tx_id, (long long)time(NULL));
    
    // Log FILE entries
    int *comp_statuses = malloc(valid_count * sizeof(int));
    for (int i = 0; i < valid_count; i++) {
        int comp = 0;
        if (!is_dirs[i] && !is_symlinks[i]) {
            if (strcmp(cfg->compression, "on") == 0) {
                if (sizes[i] >= cfg->compression_threshold) {
                    comp = 1;
                }
            } else if (strcmp(cfg->compression, "auto") == 0) {
                if (sizes[i] >= cfg->compression_threshold && is_compression_beneficial(abs_paths[i], sizes[i])) {
                    comp = 1;
                }
            }
        }
        comp_statuses[i] = comp;
        
        char hex_path[8192];
        string_to_hex(abs_paths[i], hex_path);
        
        fprintf(jf, "FILE %s %d %s %lld %o %d %d %d\n", 
                tx_id, i, hex_path, sizes[i], (unsigned int)stats[i].st_mode, is_dirs[i], is_symlinks[i], comp);
    }
    
    fflush(jf);
    int jfd = fileno(jf);
    if (jfd != -1) {
        fsync(jfd);
    }
    
    // 4. Move files to object storage
    int success = 1;
    for (int i = 0; i < valid_count; i++) {
        char obj_path[4096];
        snprintf(obj_path, sizeof(obj_path), "%s/%d", tx_dir, i);
        
        if (is_symlinks[i]) {
            // Read link and store as symlink in storage
            char target[1024];
            ssize_t len = readlink(abs_paths[i], target, sizeof(target) - 1);
            if (len == -1) {
                success = 0;
                break;
            }
            target[len] = '\0';
            if (symlink(target, obj_path) != 0) {
                success = 0;
                break;
            }
        } else if (is_dirs[i]) {
            // Directories are moved as a unit (renamed) or recursively copied
            if (rename(abs_paths[i], obj_path) != 0) {
                if (copy_dir_recursive(abs_paths[i], obj_path) != 0) {
                    success = 0;
                    break;
                }
            }
        } else {
            // Regular files
            if (comp_statuses[i] == 1) {
                if (compress_file_zlib(abs_paths[i], obj_path, stats[i].st_mode) != 0) {
                    // Fallback to raw copy if zlib failed
                    comp_statuses[i] = 0;
                    // Rewrite journal entry status to 0 - actually we can't easily rewrite
                    // append-only journal, but we can write a fallback raw copy. 
                    // Let's ensure zlib doesn't fail, or if it does, fallback to normal copy
                    // and let the journal index be raw. To keep journal clean, we try raw rename first if zlib fails:
                    if (rename(abs_paths[i], obj_path) != 0) {
                        if (copy_file(abs_paths[i], obj_path, stats[i].st_mode) != 0) {
                            success = 0;
                            break;
                        }
                    }
                }
            } else {
                if (rename(abs_paths[i], obj_path) != 0) {
                    if (copy_file(abs_paths[i], obj_path, stats[i].st_mode) != 0) {
                        success = 0;
                        break;
                    }
                }
            }
        }
        
        if (verbose) {
            printf("Stored '%s' in transaction object %d\n", abs_paths[i], i);
        }
    }
    
    if (!success) {
        fprintf(stderr, "undo: failed to store files in UNDO objects storage\n");
        // Log ABORT to journal
        fprintf(jf, "ABORT %s\n", tx_id);
        fclose(jf);
        // Recovery will handle deleting directory objects later, or we remove them now
        remove_dir_recursive(tx_dir);
        for (int k = 0; k < valid_count; k++) free(abs_paths[k]);
        free(abs_paths); free(sizes); free(stats); free(is_dirs); free(is_symlinks); free(comp_statuses);
        return -1;
    }
    
    // 5. Append COMMIT to journal and fsync
    fprintf(jf, "COMMIT %s\n", tx_id);
    fflush(jf);
    if (jfd != -1) {
        fsync(jfd);
    }
    fclose(jf);
    
    // 6. Delete original files (only those that were copied recursively and not renamed)
    for (int i = 0; i < valid_count; i++) {
        struct stat check_st;
        if (lstat(abs_paths[i], &check_st) == 0) {
            // Still exists, meaning we did a copy fallback or compression copy
            if (is_dirs[i]) {
                remove_dir_recursive(abs_paths[i]);
            } else {
                unlink(abs_paths[i]);
            }
        }
    }
    
    // Output standard confirmation
    printf("Stored in Undo\nID: %s\n", tx_id);
    
    // Clean up memory
    for (int k = 0; k < valid_count; k++) {
        free(abs_paths[k]);
    }
    free(abs_paths);
    free(sizes);
    free(stats);
    free(is_dirs);
    free(is_symlinks);
    free(comp_statuses);
    
    return 0;
}

int restore_transaction(const char *tx_id, int verbose) {
    Transaction *txs = NULL;
    int count = 0;
    if (load_transactions(&txs, &count) != 0) {
        fprintf(stderr, "undo: failed to load transaction log\n");
        return -1;
    }
    
    int idx = -1;
    if (tx_id == NULL) {
        // Find latest COMMITTED transaction
        for (int i = count - 1; i >= 0; i--) {
            if (txs[i].state == TX_COMMITTED) {
                idx = i;
                break;
            }
        }
        if (idx == -1) {
            fprintf(stderr, "undo: no transaction available to restore\n");
            free_transactions(txs, count);
            return -1;
        }
    } else {
        idx = find_tx(txs, count, tx_id);
        if (idx == -1) {
            fprintf(stderr, "undo: transaction '%s' not found\n", tx_id);
            free_transactions(txs, count);
            return -1;
        }
        if (txs[idx].state == TX_UNDONE) {
            fprintf(stderr, "undo: transaction '%s' has already been restored\n", tx_id);
            free_transactions(txs, count);
            return -1;
        }
        if (txs[idx].state != TX_COMMITTED) {
            fprintf(stderr, "undo: transaction '%s' cannot be restored (status is not COMMITTED)\n", tx_id);
            free_transactions(txs, count);
            return -1;
        }
    }
    
    Transaction *tx = &txs[idx];
    
    // Check conflicts: None of the original paths must exist
    for (int i = 0; i < tx->file_count; i++) {
        struct stat st;
        if (lstat(tx->files[i].original_path, &st) == 0) {
            fprintf(stderr, "undo: cannot restore: '%s' already exists. Rename or remove the existing file before restoring.\n", 
                    tx->files[i].original_path);
            free_transactions(txs, count);
            return -1;
        }
    }
    
    char objects_dir[1024];
    get_objects_dir(objects_dir, sizeof(objects_dir));
    
    // Recreate / move files back
    int success = 1;
    for (int i = 0; i < tx->file_count; i++) {
        TxFile *tf = &tx->files[i];
        char obj_path[4096];
        snprintf(obj_path, sizeof(obj_path), "%s/%s/%d", objects_dir, tx->id, tf->index);
        
        // Recreate parent directory if missing
        char parent[4096];
        strncpy(parent, tf->original_path, sizeof(parent));
        char *last_slash = strrchr(parent, '/');
        if (last_slash) {
            *last_slash = '\0';
            make_dir_recursive(parent, 0755);
        }
        
        if (tf->is_symlink) {
            char target[1024];
            ssize_t len = readlink(obj_path, target, sizeof(target) - 1);
            if (len == -1) {
                success = 0;
                break;
            }
            target[len] = '\0';
            if (symlink(target, tf->original_path) != 0) {
                success = 0;
                break;
            }
            unlink(obj_path);
        } else if (tf->is_dir) {
            if (rename(obj_path, tf->original_path) != 0) {
                if (copy_dir_recursive(obj_path, tf->original_path) != 0) {
                    success = 0;
                    break;
                }
                remove_dir_recursive(obj_path);
            }
        } else {
            // Regular file
            if (tf->compression_status == 1) {
                if (decompress_file_zlib(obj_path, tf->original_path, tf->mode) != 0) {
                    success = 0;
                    break;
                }
                unlink(obj_path);
            } else {
                if (rename(obj_path, tf->original_path) != 0) {
                    if (copy_file(obj_path, tf->original_path, tf->mode) != 0) {
                        success = 0;
                        break;
                    }
                    unlink(obj_path);
                }
            }
        }
        
        if (verbose) {
            printf("Restored: %s\n", tf->original_path);
        } else {
            // Print basename as per specs UX example
            char *base = strrchr(tf->original_path, '/');
            if (base) {
                printf("Restored %s\n", base + 1);
            } else {
                printf("Restored %s\n", tf->original_path);
            }
        }
    }
    
    if (!success) {
        fprintf(stderr, "undo: error occurred during transaction restore\n");
        free_transactions(txs, count);
        return -1;
    }
    
    // Clean up transaction directory
    char tx_dir[4096];
    snprintf(tx_dir, sizeof(tx_dir), "%s/%s", objects_dir, tx->id);
    rmdir(tx_dir);
    
    // Append UNDO to journal and fsync
    char journal_path[1024];
    get_journal_path(journal_path, sizeof(journal_path));
    FILE *jf = fopen(journal_path, "a");
    if (jf) {
        fprintf(jf, "UNDO %s\n", tx->id);
        fflush(jf);
        int fd = fileno(jf);
        if (fd != -1) {
            fsync(fd);
        }
        fclose(jf);
    }
    
    free_transactions(txs, count);
    return 0;
}

int clean_storage(void) {
    if (!confirm_prompt("Are you sure you want to delete all stored undo data?")) {
        printf("Operation cancelled.\n");
        return 0;
    }
    
    char objects_dir[1024];
    get_objects_dir(objects_dir, sizeof(objects_dir));
    remove_dir_recursive(objects_dir);
    make_dir_recursive(objects_dir, 0700);
    
    char journal_path[1024];
    get_journal_path(journal_path, sizeof(journal_path));
    FILE *jf = fopen(journal_path, "w"); // Truncate to 0
    if (jf) {
        fclose(jf);
    }
    
    printf("UNDO storage cleaned successfully.\n");
    return 0;
}

void show_stats(const Config *cfg) {
    char undo_dir[1024];
    get_undo_dir(undo_dir, sizeof(undo_dir));
    
    char objects_dir[1024];
    get_objects_dir(objects_dir, sizeof(objects_dir));
    
    long long total_usage = get_dir_size_recursive(undo_dir);
    long long objects_usage = get_dir_size_recursive(objects_dir);
    
    Transaction *txs = NULL;
    int count = 0;
    load_transactions(&txs, &count);
    
    int committed_count = 0;
    long long uncompressed_sum = 0;
    for (int i = 0; i < count; i++) {
        if (txs[i].state == TX_COMMITTED) {
            committed_count++;
            for (int j = 0; j < txs[i].file_count; j++) {
                uncompressed_sum += txs[i].files[j].size;
            }
        }
    }
    
    char usage_buf[64];
    char uncompressed_buf[64];
    format_size(total_usage, usage_buf, sizeof(usage_buf));
    format_size(uncompressed_sum, uncompressed_buf, sizeof(uncompressed_buf));
    
    printf("\033[1;36m=== UNDO Storage Statistics ===\033[0m\n");
    printf("Storage Location           : %s/\n", undo_dir);
    printf("Active Stored Transactions : %d\n", committed_count);
    printf("Total Disk Usage           : %s\n", usage_buf);
    printf("Uncompressed Files Size    : %s\n", uncompressed_buf);
    
    if (objects_usage > 0 && uncompressed_sum > 0) {
        double ratio = (double)uncompressed_sum / (double)objects_usage;
        printf("Compression Savings Ratio  : %.2fx\n", ratio);
    } else {
        printf("Compression Savings Ratio  : 1.00x\n");
    }
    
    char config_path[1024];
    get_config_path(config_path, sizeof(config_path));
    printf("Active Config File         : %s\n", config_path);
    printf("Large File Prompt Limit    : %lld MB\n", cfg->large_file_threshold / (1024 * 1024));
    printf("Compression Mode           : %s (Threshold: %lld MB)\n", 
           cfg->compression, cfg->compression_threshold / (1024 * 1024));
           
    free_transactions(txs, count);
}

void show_history(void) {
    Transaction *txs = NULL;
    int count = 0;
    load_transactions(&txs, &count);
    
    int printed = 0;
    printf("\033[1;36m=== UNDO Deletion History ===\033[0m\n");
    printf("%-8s %-20s %-12s %s\n", "ID", "Date & Time", "Size", "Deleted Path(s)");
    printf("--------------------------------------------------------------------------------\n");
    
    // Print in reverse order (latest first)
    for (int i = count - 1; i >= 0; i--) {
        if (txs[i].state == TX_COMMITTED) {
            char time_buf[64];
            struct tm *tm_info = localtime(&txs[i].timestamp);
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
            
            long long tx_size = 0;
            for (int j = 0; j < txs[i].file_count; j++) {
                tx_size += txs[i].files[j].size;
            }
            char sz_buf[32];
            format_size(tx_size, sz_buf, sizeof(sz_buf));
            
            // Print first file path
            char path_summary[4096] = "";
            if (txs[i].file_count == 1) {
                snprintf(path_summary, sizeof(path_summary), "%s", txs[i].files[0].original_path);
            } else if (txs[i].file_count > 1) {
                snprintf(path_summary, sizeof(path_summary), "%s (+ %d more files)", 
                         txs[i].files[0].original_path, txs[i].file_count - 1);
            }
            
            printf("%-8s %-20s %-12s %s\n", txs[i].id, time_buf, sz_buf, path_summary);
            
            // If user wants more details, we could list them, but a neat summary per line is standard
            printed++;
        }
    }
    
    if (printed == 0) {
        printf("(No deletions recorded in UNDO storage)\n");
    }
    
    free_transactions(txs, count);
}

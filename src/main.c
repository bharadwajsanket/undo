#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <dirent.h>
#include "utils.h"
#include "config.h"
#include "storage.h"

static void print_help(void) {
    printf("UNDO v1.0 — Ctrl+Z for terminal file deletions\n\n");
    printf("Usage:\n");
    printf("  undo                     Restore the latest deletion\n");
    printf("  undo <transaction_id>    Restore a specific deletion\n");
    printf("  undo history             Show deletion history\n");
    printf("  undo stats               Show storage statistics\n");
    printf("  undo config              Open interactive configuration menu\n");
    printf("  undo clean               Purge all stored UNDO data\n");
    printf("  undo install             Enable shell integration (aliases rm to undo --rm)\n");
    printf("  undo uninstall           Disable shell integration\n");
    printf("  undo -h, --help          Show this help message\n");
    printf("  undo -v, --version       Show version info\n");
}

static int is_tx_id(const char *s) {
    if (strlen(s) != 6) return 0;
    for (int i = 0; i < 6; i++) {
        if (!isxdigit((unsigned char)s[i])) {
            return 0;
        }
    }
    return 1;
}

static int is_dir_empty(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return 1;
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            count++;
            break;
        }
    }
    closedir(dir);
    return count == 0;
}

static void install_shell_integration(void) {
    const char *shell_env = getenv("SHELL");
    if (!shell_env) {
        shell_env = "/bin/zsh";
    }
    
    char rc_path[1024];
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (!home) home = ".";
    
    if (strstr(shell_env, "zsh")) {
        snprintf(rc_path, sizeof(rc_path), "%s/.zshrc", home);
    } else if (strstr(shell_env, "bash")) {
        snprintf(rc_path, sizeof(rc_path), "%s/.bashrc", home);
    } else {
        printf("undo: unsupported shell '%s'. Only bash and zsh are supported.\n", shell_env);
        return;
    }
    
    FILE *rf = fopen(rc_path, "r");
    char *content = NULL;
    long long len = 0;
    if (rf) {
        fseek(rf, 0, SEEK_END);
        len = ftell(rf);
        fseek(rf, 0, SEEK_SET);
        content = malloc(len + 1);
        if (content) {
            fread(content, 1, len, rf);
            content[len] = '\0';
        }
        fclose(rf);
    }
    
    const char *block_start = "# >>> UNDO Shell Integration >>>";
    const char *block_end = "# <<< UNDO Shell Integration <<<";
    
    if (content && strstr(content, block_start)) {
        printf("Shell integration already exists in %s\n", rc_path);
        free(content);
        return;
    }
    
    FILE *wf = fopen(rc_path, "a");
    if (!wf) {
        fprintf(stderr, "undo: failed to open %s for writing\n", rc_path);
        free(content);
        return;
    }
    
    fprintf(wf, "\n%s\nalias rm=\"undo --rm\"\n%s\n", block_start, block_end);
    fclose(wf);
    free(content);
    
    printf("Shell integration enabled in %s\n", rc_path);
    printf("Please run: source %s  or restart your terminal.\n", rc_path);
}

static void uninstall_shell_integration(void) {
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (!home) home = ".";
    
    const char *files[] = {".zshrc", ".bashrc"};
    const char *block_start = "# >>> UNDO Shell Integration >>>";
    const char *block_end = "# <<< UNDO Shell Integration <<<";
    
    for (int f_idx = 0; f_idx < 2; f_idx++) {
        char rc_path[1024];
        snprintf(rc_path, sizeof(rc_path), "%s/%s", home, files[f_idx]);
        
        FILE *rf = fopen(rc_path, "r");
        if (!rf) continue;
        
        char **lines = NULL;
        int line_count = 0;
        char buf[1024];
        int in_block = 0;
        int block_found = 0;
        
        while (fgets(buf, sizeof(buf), rf)) {
            if (strstr(buf, block_start)) {
                in_block = 1;
                block_found = 1;
                continue;
            }
            if (strstr(buf, block_end)) {
                in_block = 0;
                continue;
            }
            if (in_block) continue;
            
            lines = realloc(lines, (line_count + 1) * sizeof(char *));
            lines[line_count++] = strdup(buf);
        }
        fclose(rf);
        
        if (block_found) {
            FILE *wf = fopen(rc_path, "w");
            if (wf) {
                for (int i = 0; i < line_count; i++) {
                    fputs(lines[i], wf);
                }
                fclose(wf);
                printf("Shell integration removed from %s\n", rc_path);
            }
        }
        
        for (int i = 0; i < line_count; i++) {
            free(lines[i]);
        }
        free(lines);
    }
}

// Parses options matching typical rm CLI arguments
static int handle_rm(int argc, char **argv, const Config *cfg) {
    int force = 0;
    int recursive = 0;
    int dir_flag = 0;
    int verbose = 0;
    int prompt_all = 0;
    
    int files_start = -1;
    
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            files_start = i + 1;
            break;
        }
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            // Option parsing
            char *p = argv[i] + 1;
            while (*p) {
                switch (*p) {
                    case 'f': force = 1; prompt_all = 0; break;
                    case 'r':
                    case 'R': recursive = 1; break;
                    case 'd': dir_flag = 1; break;
                    case 'v': verbose = 1; break;
                    case 'i': prompt_all = 1; force = 0; break;
                    default:
                        // Ignore other standard options like -I or --preserve-root
                        break;
                }
                p++;
            }
        } else {
            files_start = i;
            break;
        }
    }
    
    if (files_start == -1 || files_start >= argc) {
        if (force) {
            return 0; // rm -f with no arguments exits with 0
        }
        fprintf(stderr, "undo: missing operand\n");
        return 1;
    }
    
    int paths_count = argc - files_start;
    char **paths = argv + files_start;
    
    // Validate directory vs recursive/dir flags
    int checked_count = 0;
    char **checked_paths = malloc(paths_count * sizeof(char *));
    
    for (int i = 0; i < paths_count; i++) {
        struct stat st;
        int exists = (lstat(paths[i], &st) == 0);
        if (!exists) {
            if (force) {
                continue; // Ignore non-existent files in force mode
            }
            fprintf(stderr, "undo: cannot remove '%s': No such file or directory\n", paths[i]);
            free(checked_paths);
            return 1;
        }
        
        if (S_ISDIR(st.st_mode)) {
            if (!recursive && !dir_flag) {
                fprintf(stderr, "undo: cannot remove '%s': Is a directory\n", paths[i]);
                free(checked_paths);
                return 1;
            }
            if (dir_flag && !recursive && !is_dir_empty(paths[i])) {
                fprintf(stderr, "undo: cannot remove '%s': Directory not empty\n", paths[i]);
                free(checked_paths);
                return 1;
            }
        }
        
        // Prompt if -i was passed
        if (prompt_all) {
            char prompt_msg[4096];
            snprintf(prompt_msg, sizeof(prompt_msg), "remove '%s'?", paths[i]);
            if (!confirm_prompt(prompt_msg)) {
                continue; // Skip file
            }
        }
        
        checked_paths[checked_count++] = paths[i];
    }
    
    if (checked_count == 0) {
        free(checked_paths);
        return 0;
    }
    
    // Generate unique transaction ID
    char tx_id[7];
    generate_tx_id(tx_id);
    
    int status = create_transaction(tx_id, checked_count, checked_paths, cfg, force, verbose);
    free(checked_paths);
    
    return status == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    // 1. Ensure UNDO directories exist
    char undo_dir[1024];
    get_undo_dir(undo_dir, sizeof(undo_dir));
    make_dir_recursive(undo_dir, 0700);
    
    char objects_dir[1024];
    get_objects_dir(objects_dir, sizeof(objects_dir));
    make_dir_recursive(objects_dir, 0700);
    
    // 2. Perform crashed transaction recovery on startup
    recover_crashed_transactions();
    
    // 3. Load configurations
    Config cfg;
    load_config(&cfg);
    
    // 4. Parse commands
    if (argc < 2) {
        // Restore latest committed transaction
        int status = restore_transaction(NULL, 0);
        return status == 0 ? 0 : 1;
    }
    
    if (strcmp(argv[1], "--rm") == 0) {
        return handle_rm(argc, argv, &cfg);
    } else if (strcmp(argv[1], "history") == 0) {
        show_history();
        return 0;
    } else if (strcmp(argv[1], "stats") == 0) {
        show_stats(&cfg);
        return 0;
    } else if (strcmp(argv[1], "config") == 0) {
        run_config_ui(&cfg);
        return 0;
    } else if (strcmp(argv[1], "clean") == 0) {
        int status = clean_storage();
        return status == 0 ? 0 : 1;
    } else if (strcmp(argv[1], "install") == 0) {
        install_shell_integration();
        return 0;
    } else if (strcmp(argv[1], "uninstall") == 0) {
        uninstall_shell_integration();
        return 0;
    } else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_help();
        return 0;
    } else if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        printf("UNDO v1.0\n");
        return 0;
    } else if (is_tx_id(argv[1])) {
        int status = restore_transaction(argv[1], 0);
        return status == 0 ? 0 : 1;
    } else {
        fprintf(stderr, "undo: unknown command: '%s'\n", argv[1]);
        print_help();
        return 1;
    }
}

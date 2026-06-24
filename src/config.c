#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "config.h"
#include "utils.h"

// Set defaults
static void set_default_config(Config *cfg) {
    cfg->large_file_threshold = 100 * 1024 * 1024; // 100 MB
    strcpy(cfg->compression, "auto");
    cfg->compression_threshold = 1 * 1024 * 1024; // 1 MB
}

// Clean whitespace from strings
static char *trim_whitespace(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void load_config(Config *cfg) {
    set_default_config(cfg);
    
    char path[1024];
    get_config_path(path, sizeof(path));
    
    FILE *f = fopen(path, "r");
    if (!f) {
        // Save the defaults if no config exists yet
        save_config(cfg);
        return;
    }
    
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = trim_whitespace(line);
        if (trimmed[0] == '\0' || trimmed[0] == '#') {
            continue;
        }
        
        char *eq = strchr(trimmed, '=');
        if (!eq) continue;
        
        *eq = '\0';
        char *key = trim_whitespace(trimmed);
        char *val = trim_whitespace(eq + 1);
        
        if (strcmp(key, "large_file_threshold") == 0) {
            cfg->large_file_threshold = atoll(val);
        } else if (strcmp(key, "compression") == 0) {
            strncpy(cfg->compression, val, sizeof(cfg->compression) - 1);
            cfg->compression[sizeof(cfg->compression) - 1] = '\0';
        } else if (strcmp(key, "compression_threshold") == 0) {
            cfg->compression_threshold = atoll(val);
        }
    }
    
    fclose(f);
}

void save_config(const Config *cfg) {
    char path[1024];
    get_config_path(path, sizeof(path));
    
    char dir[1024];
    get_undo_dir(dir, sizeof(dir));
    make_dir_recursive(dir, 0700);
    
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "undo: failed to write config file %s\n", path);
        return;
    }
    
    fprintf(f, "# UNDO v1.0 Configuration File\n\n");
    fprintf(f, "# Prompt user if deleting files/directories larger than this threshold (in bytes)\n");
    fprintf(f, "large_file_threshold = %lld\n\n", cfg->large_file_threshold);
    fprintf(f, "# Compression mode: auto (compress if beneficial), on (always), off (never)\n");
    fprintf(f, "compression = %s\n\n", cfg->compression);
    fprintf(f, "# Compress files larger than this threshold (in bytes)\n");
    fprintf(f, "compression_threshold = %lld\n", cfg->compression_threshold);
    
    fclose(f);
}

void run_config_ui(Config *cfg) {
    char choice_str[64];
    while (1) {
        printf("\n\033[1;36m=== UNDO Configuration Menu ===\033[0m\n");
        printf("1) Large File Prompt Threshold : %lld MB (%lld bytes)\n", 
               cfg->large_file_threshold / (1024 * 1024), cfg->large_file_threshold);
        printf("2) Compression Mode            : %s\n", cfg->compression);
        printf("3) Compression Threshold       : %lld MB (%lld bytes)\n", 
               cfg->compression_threshold / (1024 * 1024), cfg->compression_threshold);
        printf("4) Edit Config File directly in Editor\n");
        printf("5) Save and Exit\n");
        printf("6) Cancel (Discard changes)\n");
        printf("Select an option (1-6): ");
        fflush(stdout);
        
        if (!fgets(choice_str, sizeof(choice_str), stdin)) {
            break;
        }
        
        int choice = atoi(choice_str);
        if (choice == 1) {
            printf("Enter new large file prompt threshold in MB (e.g. 50): ");
            fflush(stdout);
            char val_str[64];
            if (fgets(val_str, sizeof(val_str), stdin)) {
                long long val_mb = atoll(val_str);
                if (val_mb > 0) {
                    cfg->large_file_threshold = val_mb * 1024 * 1024;
                    printf("Threshold updated to %lld MB.\n", val_mb);
                } else {
                    printf("Invalid value.\n");
                }
            }
        } else if (choice == 2) {
            printf("Select compression mode:\n");
            printf("  1. auto (compress when beneficial)\n");
            printf("  2. on (always compress)\n");
            printf("  3. off (never compress)\n");
            printf("Select option (1-3): ");
            fflush(stdout);
            char val_str[64];
            if (fgets(val_str, sizeof(val_str), stdin)) {
                int m = atoi(val_str);
                if (m == 1) {
                    strcpy(cfg->compression, "auto");
                    printf("Compression mode set to auto.\n");
                } else if (m == 2) {
                    strcpy(cfg->compression, "on");
                    printf("Compression mode set to on.\n");
                } else if (m == 3) {
                    strcpy(cfg->compression, "off");
                    printf("Compression mode set to off.\n");
                } else {
                    printf("Invalid option.\n");
                }
            }
        } else if (choice == 3) {
            printf("Enter new compression threshold in MB (e.g. 5): ");
            fflush(stdout);
            char val_str[64];
            if (fgets(val_str, sizeof(val_str), stdin)) {
                long long val_mb = atoll(val_str);
                if (val_mb >= 0) {
                    cfg->compression_threshold = val_mb * 1024 * 1024;
                    printf("Compression threshold updated to %lld MB.\n", val_mb);
                } else {
                    printf("Invalid value.\n");
                }
            }
        } else if (choice == 4) {
            char path[1024];
            get_config_path(path, sizeof(path));
            
            // Save current state first so they are editing the latest values
            save_config(cfg);
            
            const char *editor = getenv("EDITOR");
            if (!editor) {
                editor = "nano";
            }
            
            char cmd[2048];
            snprintf(cmd, sizeof(cmd), "%s \"%s\"", editor, path);
            printf("Opening config file with command: %s\n", cmd);
            int res = system(cmd);
            if (res == 0) {
                // Reload after editing
                load_config(cfg);
                printf("Configuration reloaded from editor.\n");
            } else {
                printf("Failed to edit file via editor. Fallback to vi?\n");
                snprintf(cmd, sizeof(cmd), "vi \"%s\"", path);
                system(cmd);
                load_config(cfg);
            }
        } else if (choice == 5) {
            save_config(cfg);
            printf("Configuration saved successfully.\n");
            break;
        } else if (choice == 6) {
            printf("Discarding changes.\n");
            break;
        } else {
            printf("Invalid option. Please enter 1-6.\n");
        }
    }
}

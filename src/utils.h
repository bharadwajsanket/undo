#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>

// Path resolution functions
void get_undo_dir(char *buf, size_t len);
void get_config_path(char *buf, size_t len);
void get_journal_path(char *buf, size_t len);
void get_objects_dir(char *buf, size_t len);
int get_absolute_path_safe(const char *path, char *abs_path, size_t abs_len);

// Directory utility to create folders recursively
int make_dir_recursive(const char *path, mode_t mode);

// Hex encoding / decoding for safe logging of weird paths
void string_to_hex(const char *src, char *dst);
void hex_to_string(const char *src, char *dst);

// Random ID generation (6-hex-digit string)
void generate_tx_id(char *buf);

// File size queries
long long get_file_size(const char *path);
long long get_dir_size_recursive(const char *path);

// File copying and recursive directory deletion / copying
int copy_file(const char *src, const char *dst, mode_t mode);
int copy_dir_recursive(const char *src, const char *dst);
int remove_dir_recursive(const char *path);

// Prompts
int confirm_prompt(const char *message);

#endif // UTILS_H

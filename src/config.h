#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    long long large_file_threshold;   // In bytes
    char compression[8];              // "auto", "on", "off"
    long long compression_threshold;  // In bytes
} Config;

void load_config(Config *cfg);
void save_config(const Config *cfg);
void run_config_ui(Config *cfg);

#endif // CONFIG_H

#ifndef STORAGE_H
#define STORAGE_H

#include <sys/types.h>
#include <time.h>
#include "config.h"

typedef struct {
    int index;
    char original_path[4096];
    long long size;
    mode_t mode;
    int is_dir;
    int is_symlink;
    int compression_status; // 0 = raw, 1 = zlib compressed
} TxFile;

typedef enum {
    TX_PENDING,
    TX_COMMITTED,
    TX_UNDONE,
    TX_ABORTED
} TxState;

typedef struct {
    char id[7];
    time_t timestamp;
    TxState state;
    TxFile *files;
    int file_count;
} Transaction;

// Journal processing & history loading
int load_transactions(Transaction **txs, int *count);
void free_transactions(Transaction *txs, int count);

// Crash Recovery
int recover_crashed_transactions(void);

// CLI operations
int create_transaction(const char *tx_id, int file_count, char **paths, const Config *cfg, int force, int verbose);
int restore_transaction(const char *tx_id, int verbose);
int clean_storage(void);
void show_stats(const Config *cfg);
void show_history(void);

#endif // STORAGE_H

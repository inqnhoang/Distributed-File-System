#ifndef WAL_H
#define WAL_H

#include "operation.h"
#include "fs.h"
#include "dfs.h"

#define WAL_SIZE 256

typedef struct {
    operation_type_h op_type;
    time_t time_stamp;
    int sequence_number;
    union {
        struct { char name[4]; } create_params;
        struct { char name[4]; } destroy_params;
        struct { int oft_idx; int m; int n; byte data[512]; } write_params;
        struct { int oft_idx; int position; } seek_params;
    } params;
} wal_entry_t;

void wal_init(fs_node_t* fs);

wal_entry_t wal_log_create(dfs_t* dfs, char name[4]);

wal_entry_t wal_log_destroy(dfs_t* dfs, char name[4]);

wal_entry_t wal_log_write(dfs_t* dfs, int oft_idx, int m, int n, byte data[512]);

wal_entry_t wal_log_seek(dfs_t* dfs, int oft_idx, int position);

// void wal_print(dfs_t* dfs);

// void wal_clear(dfs_t* dfs);

// void wal_stats(dfs_t* dfs);

static int wal_apply_entry(fs_node_t * fs, int node_id, wal_entry_t* entry);

// int wal_replay(dfs_t* dfs);

#endif
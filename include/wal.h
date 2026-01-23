#ifndef WAL_H
#define WAL_H

#include "operation.h"
#define WAL_SIZE 256

typedef struct {
    int sequence_number;
    operation_type_h op_type;
    time_t time_stamp;
    
    union {
        struct { char name[4]; } create_params;
        struct { char name[4]; } destroy_params;
        struct { int oft_idx; int m; int n; byte data[512]; } write_params;
        struct { int oft_idx; int position; } seek_params;
    } params;
} wal_entry_t;

void wal_init(fs_node_t* fs);

int wal_log_create(fs_node_t* fs, char name[4]);

int wal_log_destroy(fs_node_t* fs, char name[4]);

int wal_log_write(fs_node_t* fs, int oft_idx, int m, int n);

int wal_log_seek(fs_node_t* fs, int oft_idx, int position);

void wal_print(fs_node_t* fs);

void wal_clear(fs_node_t* fs);

void wal_stats(fs_node_t* fs);

#endif
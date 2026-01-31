#ifndef DISTRIBUTED_FILE_SYSTEM
#define DISTRIBUTED_FILE_SYSTEM

#define NUM_NODES 3
#define CHECK_POINT_INTERVAL 10

#define MAX_ARGC 256

#include "node.h"

typedef struct {
    node_t nodes[NUM_NODES];
    fs_node_t file_systems[NUM_NODES];
    int leader;
    int global_sequence_counter;
} dfs_t;

void dfs_init(dfs_t* dfs);

int dfs_replicate_operation(dfs_t* dfs, wal_entry_t* entry);

#endif
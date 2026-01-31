#include "wal.h"

void wal_init(fs_node_t* fs) {
    fs->wal_head = 0;
    fs->wal_tail = 0;
    fs->wal_count = 0;
    fs->operations_applied = 0;
    fs->operations_failed = 0;
    fs->log_replays = 0;
    fs->last_checkpoint = time(NULL);

    memset(fs->wal, 0, sizeof(wal_entry_t) * WAL_SIZE);
}

static int wal_apply_entry(fs_node_t* fs, int node_id, wal_entry_t* entry) {
    int result = 0;
    
    switch(entry->op_type) {
        case OP_CREATE:
            result = create(fs, entry->params.create_params.name);
            break;
            
        case OP_DESTROY:
            result = destroy(fs, entry->params.destroy_params.name);
            break;
            
        case OP_WRITE:
            // Restore the data to memory buffer first
            memcpy(&fs->M[entry->params.write_params.m], 
                   entry->params.write_params.data, 
                   entry->params.write_params.n);
            result = f_write(fs, 
                           entry->params.write_params.oft_idx,
                           entry->params.write_params.m,
                           entry->params.write_params.n);
            break;
            
        case OP_SEEK:
            result = seek(fs, 
                         entry->params.seek_params.oft_idx,
                         entry->params.seek_params.position);
            break;
            
        default:
            printf("ERROR: Unknown operation type %d\n", entry->op_type);
            return -1;
    }
    
    if (result < 0) {
        fs->operations_failed++;
    } else {
        fs->operations_applied++;
    }
    
    return result;
}

static int wal_add_entry(fs_node_t* fs, wal_entry_t* entry, int global_seq) {
    if (fs->wal_count >= WAL_SIZE) {
        return -1;
    }
    
    entry->sequence_number = global_seq;
    entry->time_stamp = time(NULL);
    
    fs->wal[fs->wal_tail] = *entry;
    fs->wal_tail = (fs->wal_tail + 1) % WAL_SIZE;
    fs->wal_count++;
    
    return 0;
}

wal_entry_t wal_log_create(dfs_t* dfs, char name[4]) {
    wal_entry_t entry;
    entry.op_type = OP_CREATE;
    memcpy(entry.params.create_params.name, name, 4);
    
    int seq = dfs->global_sequence_counter++;
    
    for (int i = 0; i < NUM_NODES; i++) {
        if (wal_add_entry(&dfs->file_systems[i], &entry, seq) < 0) {
            return entry;
        }
    }
    return entry;
}

wal_entry_t wal_log_destroy(dfs_t* dfs, char name[4]) {
    wal_entry_t entry;
    entry.op_type = OP_DESTROY;
    memcpy(entry.params.destroy_params.name, name, 4);

    int seq = dfs->global_sequence_counter++;
    for (int i = 0; i < NUM_NODES; i++) {
        if (wal_add_entry(&dfs->file_systems[i], &entry, seq) < 0) {
            return entry;
        }
    }
    return entry;
}

wal_entry_t wal_log_write(dfs_t* dfs, int oft_idx, int m, int n, byte data[512]) {
    wal_entry_t entry;
    entry.op_type = OP_WRITE;
    entry.params.write_params.oft_idx = oft_idx;
    entry.params.write_params.m = m;
    entry.params.write_params.n = n;
    memcpy(entry.params.write_params.data, data, sizeof(data));

    int seq = dfs->global_sequence_counter++;
    
    for (int i = 0; i < NUM_NODES; i++) {
        if (wal_add_entry(&dfs->file_systems[i], &entry, seq) < 0) {
            return entry;
        }
    }
    return entry;
}

wal_entry_t wal_log_seek(dfs_t* dfs, int oft_idx, int position) {
    wal_entry_t entry;
    entry.op_type = OP_SEEK;
    entry.params.seek_params.oft_idx = oft_idx;
    entry.params.seek_params.position = position;

    int seq = dfs->global_sequence_counter++;
    
    for (int i = 0; i < NUM_NODES; i++) {
        if (wal_add_entry(&dfs->file_systems[i], &entry, seq) < 0) {
            return entry;
        }
    }
    return entry;
}


// void wal_print(dfs_t* dfs);

// void wal_clear(dfs_t* dfs);

// void wal_stats(dfs_t* dfs);

// int wal_replay(dfs_t* dfs);
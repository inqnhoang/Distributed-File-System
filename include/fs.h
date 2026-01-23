#ifndef FS_H
#define FS_H

#include "types.h"
#include "wal.h"

#define N_BLOCKS 64
#define BLOCK_SIZE 512
#define BITS_PER_BYTE 8
#define N_FILE_DESC 192

#define FD_BLOCK(x) ((x * 16) / BLOCK_SIZE + 1)
#define FD_OFFSET(x) ((x * 16) % BLOCK_SIZE)
#define BIT_MAP_BLOCK(x) (x / BITS_PER_BYTE)
#define BIT_MAP_OFFSET(x) (x % BITS_PER_BYTE)

typedef enum {
    NAME = 0,
    FD = 1
} dir_section;

typedef struct {
    byte rw_buffer[BLOCK_SIZE];
    int file_size;
    int curr_pos;
    int fd;
} OFT_entry;

typedef struct {
    OFT_entry OFT[4];
    byte D[N_BLOCKS][BLOCK_SIZE];
    byte I[BLOCK_SIZE];
    byte O[BLOCK_SIZE];
    byte M[BLOCK_SIZE];

    wal_entry_t wal[WAL_SIZE];
    int wal_head;
    int wal_tail;
    int wal_count;

    int operations_applied;
    int operations_failed;
    int log_replays;
    time_t last_checkpoint;
} fs_node_t;

#endif
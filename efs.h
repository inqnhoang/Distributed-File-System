#ifndef EMULATED_FILE_SYSTEM_EFS_H
#define EMULATED_FILE_SYSTEM_EFS_H

// #define dir_k 1 // block 0 is for initial dir
// #define fd_k 7 // blocks 1 - 6 hold file descriptors
// #define space_k 64 // blocks 7 - 63 is storage
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define N_BLOCKS 64
#define BLOCK_SIZE 512
#define BYTE 8
#define N_FILE_DESC 192

#define FD_BLOCK(x) ((x * 16) / BLOCK_SIZE + 1) // 192 file descriptors held in 6 blocks ~ 16 bytes for each file descriptor info
#define FD_OFFSET(x) ((x * 16) % BLOCK_SIZE)  // gets offset within fd block 
#define BIT_MAP_BLOCK(x) (x / BYTE)
#define BIT_MAP_OFFSET(x) (x % BYTE)

typedef enum {
    NAME = 0,
    FD = 1
} dir_section;

typedef unsigned char byte;
typedef struct {
    byte rw_buffer[BLOCK_SIZE];
    int file_size;
    int curr_pos;
    int fd;
} OFT_entry;

int init();
int read_memory(int m, int n);
int write_memory(int m, char* string);

int create(char file_name[4]);
int destroy(char name[4]);
int open(char name[4]);
int close(int i);

// Copy n bytes from open file i (starting from curr_pos) to memory buffer m
int f_read(int i, int m, int n);

// Write n bytes to file i (starting from curr_pos) to memory buffer m
int f_write(int i, int m, int n);

// move curr pos within an open file i to a new pos p
int seek(int i, int p);
 
int directory();

// helpers
int name_to_int (char name[4]);

// gets file length w/ file descriptor
int get_fd_info(int i, int section);

int write_fd_info(int info, int fd, int section);

int get_dir_info_name(int block, char name[4]);

int get_dir_info_desc (int block);

int write_dir_info(void * info, int block, int section);

int get_bit_map_info(int block);

int write_bit_map_info(int info, int block);

// compares int and char[4]
int str_cmp_int_file_name(int int_file_name, const char file_name[4]);

int direct_read_memory(int m, int n);
int direct_write_memory(int m, char* string);

#endif // EMULATED_FILE_SYSTEM_EFS_H

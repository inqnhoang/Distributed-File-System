#ifndef EMULATED_FILE_SYSTEM_EFS_H
#define EMULATED_FILE_SYSTEM_EFS_H

#include <memory.h>
#include "fs.h"

int init(fs_node_t* fs);

int read_memory(fs_node_t* fs, int m, int n);

int write_memory(fs_node_t* fs, int m, char* string);

int create(fs_node_t* fs, char file_name[4]);

int destroy(fs_node_t* fs, char name[4]);

int open(fs_node_t* fs, char name[4]);

int close(fs_node_t* fs, int i);

int f_read(fs_node_t* fs, int i, int m, int n);

int f_write(fs_node_t* fs, int i, int m, int n);

int seek(fs_node_t* fs, int i, int p);
 
int directory(fs_node_t* fs);

int name_to_int (fs_node_t* fs, char name[4]);

int get_fd_info(fs_node_t* fs, int i, int section);

int write_fd_info(fs_node_t* fs,int info, int fd, int section);

int get_dir_info_name(fs_node_t* fs,int block, char name[4]);

int get_dir_info_desc (fs_node_t* fs, int block);

int write_dir_info(fs_node_t* fs, void * info, int block, int section);

int get_bit_map_info(fs_node_t* fs, int block);

int write_bit_map_info(fs_node_t* fs, int info, int block);

int str_cmp_int_file_name(fs_node_t* fs, int int_file_name, const char file_name[4]);

int direct_read_memory(fs_node_t* fs, int m, int n);

int direct_write_memory(fs_node_t* fs, int m, char* string);

#endif

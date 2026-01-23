#include "efs.h"
///// Helper Functions /////
int get_fd_info(fs_node_t* fs, int i, int section) // 4 SECTIONS (BYTES): FILE_LENGTH (4) | BLOCK 0 (4) | BLOCK 1 (4) | BLOCK 2 (4) | 
{
    if (i < 0 || i >= N_FILE_DESC) return -1;

    int fd_block = FD_BLOCK(i);
    int fd_offset = FD_OFFSET(i);

    byte * fd_pos = fs->I + fd_offset + section * 4; // there are four sections in a fd info piece, ea. 4 bytes wide
    int info = 0;
    for (int j = 0; j < 4; j++) {
        info |= ((int)fd_pos[j]) << (j * BITS_PER_BYTE); // rebuilding int of fd info section  
    }

    return info;
}

int write_fd_info(fs_node_t* fs, int info, int fd, int section)
{
    if (fd < 0 || fd >= N_FILE_DESC) return -1;

    int fd_block = FD_BLOCK(fd);
    int fd_offset = FD_OFFSET(fd);
    
    byte * fd_pos = fs->I + fd_offset + section * 4;

    for (int j = 0; j < 4; j++) {
        *(fd_pos + j) = (info >> (j * BITS_PER_BYTE)) & 0xff; 
    }

    return 0;
}

int get_dir_info_name (fs_node_t* fs, int block, char name[4]) // 2 SECTIONS (BYTES): FILE_NAME (4) | DESCRIPTOR_INDEX (4)
{
    // OFT[0] ~ dir
    byte *start_pos = fs->OFT[0].rw_buffer + block * 8;
    for (int i = 0; i < 4; i++) {
        name[i] = start_pos[i];
    }
    return 0;
}

int get_dir_info_desc (fs_node_t* fs, int block) // 2 SECTIONS (BYTES): FILE_NAME (4) | DESCRIPTOR_INDEX (4)
{
    // OFT[0] ~ dir
    byte * start_pos = fs->OFT[0].rw_buffer + block * 8 + 4;
    int info = 0;
    for (int i = 0; i < 4; i++) {
        info |= ((int) start_pos[i]) << (i * BITS_PER_BYTE);
    }
    
    return info;
}

int write_dir_info(fs_node_t* fs, void * info, int block, int section) 
{
    if (block < 0 || block >= 64) return -1;

    byte * start_pos = fs->OFT[0].rw_buffer + block * BITS_PER_BYTE + section * 4;

    if (section == 0) {
        byte * info_value = (byte *) info;
        for (int i = 0; i < 4; i++) {
            start_pos[i] = info_value[i];
        }

    } else {
        int info_value = *(int *)info;
        for (int i = 0; i < 4; i++) {
            start_pos[i] = (info_value >> (i * BITS_PER_BYTE)) & 0xff;
        }
    }
    
    return 0;
}

int get_bit_map_info(fs_node_t* fs, int block) {
    byte byte = fs->O[BIT_MAP_BLOCK(block)];
    return (byte >> BIT_MAP_OFFSET(block)) & 0x1;
}

int write_bit_map_info(fs_node_t* fs, int info, int block) {
    if (block < 1 || block > N_BLOCKS - 1) return -1;

    byte * byte = fs->O[BIT_MAP_BLOCK(block)];
    int offset = BIT_MAP_OFFSET(block);
    // turn off
    if (info == 0) {
        int mask = ~(1 << offset);
        *byte &= mask;
    } else {
        int mask = (1 << offset);
        *byte |= mask;
    }
    
    return 0;
}                                       

///// FILE SYS MANIP OPERATIONS /////

int create(fs_node_t* fs, char name[4]) {
    int seek_err = seek(fs, 0, 0);
    if (seek_err < 0) {
        return -1;
    }
    
    // check if file_name exists
    int free_entry = -1;
    for (int i = 0; i < BLOCK_SIZE / 8; i++) {
        char file_name[4];
        get_dir_info_name(fs, i, file_name);
        
        if (strcmp(name, file_name) == 0) {
            return -1;  // File already exists
        }
        
        if (free_entry == -1 && memcmp(file_name, "\0\0\0\0", 4) == 0) {
            free_entry = i;
        }
    }

    if (free_entry == -1) return -1;  // No free directory entry

    // search for free file descriptor
    int free_fd = -1;
    for (int i = 1; i < N_FILE_DESC; i++) {
        // load in new block of fd every 32 descriptors
        if (i % 32 == 0 || i == 1) {
            memcpy(fs->I, fs->D[1 + (i / 32)], BLOCK_SIZE);
        }

        int file_length = get_fd_info(fs, i, 0);
        if (file_length == -1) {
            free_fd = i;
            write_fd_info(fs, 0, i, 0);
            break;
        }
    }

    if (free_fd == -1) return -1;  // No free descriptor

    // Update directory
    write_dir_info(fs, name, free_entry, 0);
    write_dir_info(fs, &free_fd, free_entry, 1);

    // Write descriptor and directory back to disk
    memcpy(fs->D[1 + free_fd/32], fs->I, BLOCK_SIZE);
    memcpy(fs->D[7], fs->OFT[0].rw_buffer, BLOCK_SIZE);
    
    return 0;
}

int destroy(fs_node_t* fs, char name[4]) 
{
    int seek_err = seek(fs, 0, 0);
    if (seek_err < 0) {
        return -1;
    }

    // search through directory for matching filenames
    int fd = -1;
    int dir_index = -1;
    
    for (int i = 0; i < BLOCK_SIZE / 8; i++) {
        char file_name[4];
        get_dir_info_name(fs, i, file_name);

        if (strcmp(file_name, name) == 0) {
            fd = get_dir_info_desc(fs, i);
            dir_index = i;
            break;
        }
    }

    if (fd == -1) return -1;  // File not found

    // Check if file is open
    for (int k = 1; k < 4; k++) {
        if (fs->OFT[k].curr_pos != -1 && fs->OFT[k].fd == fd) {
            return -1; // cannot destroy open file
        }
    }

    // Free the file descriptor and blocks
    int fd_block = FD_BLOCK(fd);
    memcpy(fs->I, fs->D[fd_block], BLOCK_SIZE);
    memcpy(fs->O, fs->D[0], BLOCK_SIZE);

    // Mark descriptor as free and free all blocks
    write_fd_info(fs, -1, fd, 0);
    
    for (int i = 1; i < 4; i++) {
        int block = get_fd_info(fs, fd, i);

        if (block != -1 && block > 0) {
            write_bit_map_info(fs, 0, block);
            write_fd_info(fs, -1, fd, i);  // FIX 2: Set to -1, not 0
        }
    }

    // Clear directory entry
    int zero = 0;
    write_dir_info(fs, &zero, dir_index, 0);
    write_dir_info(fs, &zero, dir_index, 1);  // FIX 3: Clear descriptor index too
    
    // Write changes back to disk
    memcpy(fs->D[fd_block], fs->I, BLOCK_SIZE);
    memcpy(fs->D[0], fs->O, BLOCK_SIZE);
    memcpy(fs->D[7], fs->OFT[0].rw_buffer, BLOCK_SIZE);

    return 0;
}

int open(fs_node_t* fs, char name[4]) 
{   
    int fd = -1;

    // find file in dir, search through directory entries
    for (int i = 0; i < BLOCK_SIZE / 8; i++) {  // 64 entries max in one block
        char dir_name[4];
        get_dir_info_name(fs, i, dir_name);
        if (strcmp(name, dir_name) == 0) {
            fd = get_dir_info_desc(fs, i);
            break;
        }
    }

    if (fd == -1) return -1;

    // file is already opened
    for (int k = 1; k < 4; k++) {
        if (fs->OFT[k].curr_pos != -1 && fs->OFT[k].fd == fd) {
            return -1;
        }
    }

    int free_oft = -1;
    for (int i = 1; i < 4; i++) {
        if (fs->OFT[i].curr_pos == -1) {
            memcpy(fs->I, fs->D[FD_BLOCK(fd)], BLOCK_SIZE);

            free_oft = i;
            fs->OFT[i].fd = fd;
            fs->OFT[i].curr_pos = 0;
            fs->OFT[i].file_size = get_fd_info(fs, fd, 0);
            
            // allocate properly if file_size == 0
            if (fs->OFT[i].file_size == 0) {
                int free_block = -1;
                for (int j = 8; j < N_BLOCKS; j++) {
                    if (get_bit_map_info(fs, j) == 0) {
                        memcpy(fs->O, fs->D[0], BLOCK_SIZE);

                        free_block = j;
                        write_fd_info(fs, free_block, fd, 1);
                        write_bit_map_info(fs, 1, free_block);
                        
                        memcpy(fs->D[FD_BLOCK(fd)], fs->I, BLOCK_SIZE);
                        memset(fs->OFT[i].rw_buffer, 0, BLOCK_SIZE);
                        memcpy(fs->D[0], fs->O, BLOCK_SIZE);
                        break;
                    }
                }
                
                if (free_block == -1) {
                    fs->OFT[i].fd = -1;
                    fs->OFT[i].curr_pos = -1;
                    fs->OFT[i].file_size = 0;
                    free_oft = -1;
                }

            } else {
                int fd_first_block = get_fd_info(fs, fd, 1);
                if (fd_first_block != -1) {
                    memcpy(fs->OFT[i].rw_buffer, fs->D[fd_first_block], BLOCK_SIZE);
                }
            }

            break;
        }
    }
    return free_oft;
}

int close(fs_node_t* fs, int i) 
{
    if (i < 0 || i >= 4)
        return -1;

    if (fs->OFT[i].curr_pos == -1)  // Check if entry is free
        return -1;

    // Write current block back to disk
    int curr_block = fs->OFT[i].curr_pos / BLOCK_SIZE;
    int fd_section = curr_block + 1;
    
    memcpy(fs->I, fs->D[FD_BLOCK(fs->OFT[i].fd)], BLOCK_SIZE);
    int disk_block = get_fd_info(fs, fs->OFT[i].fd, fd_section);

    if (disk_block != -1) {
        memcpy(fs->D[disk_block], fs->OFT[i].rw_buffer, BLOCK_SIZE);
    }

    // Update file size in descriptor
    write_fd_info(fs, fs->OFT[i].file_size, fs->OFT[i].fd, 0);
    memcpy(fs->D[FD_BLOCK(fs->OFT[i].fd)], fs->I, BLOCK_SIZE);

    // Mark OFT entry as free
    fs->OFT[i].fd = -1;
    fs->OFT[i].curr_pos = -1;
    fs->OFT[i].file_size = 0;
    memset(fs->OFT[i].rw_buffer, 0, BLOCK_SIZE);

    return 0;
}

int f_read(fs_node_t* fs, int i, int m, int n)
{
    if (i < 0 || i >= 4 || fs->OFT[i].curr_pos == -1) return -1;
    if (m < 0 || n < 0) return -1;
    
    memcpy(fs->I, fs->D[FD_BLOCK(fs->OFT[i].fd)], BLOCK_SIZE);
    
    int bytes_read = 0;
    
    while (bytes_read < n && fs->OFT[i].curr_pos < fs->OFT[i].file_size) {
        int buf_offset = fs->OFT[i].curr_pos % BLOCK_SIZE;
        fs->M[m + bytes_read] = fs->OFT[i].rw_buffer[buf_offset];

        bytes_read++;
        fs->OFT[i].curr_pos++;

        if (fs->OFT[i].curr_pos % BLOCK_SIZE == 0 && fs->OFT[i].curr_pos < fs->OFT[i].file_size) {
            int next_block_section = (fs->OFT[i].curr_pos / BLOCK_SIZE) + 1;
            int next_block = get_fd_info(fs, fs->OFT[i].fd, next_block_section);

            if (next_block == -1) break;

            memcpy(fs->OFT[i].rw_buffer, fs->D[next_block], BLOCK_SIZE);
        }
    }

    return bytes_read;
}

int f_write(fs_node_t* fs, int i, int m, int n)
{
    if (i < 0 || i >= 4 || fs->OFT[i].curr_pos == -1) return -1;
    
    memcpy(fs->I, fs->D[FD_BLOCK(fs->OFT[i].fd)], BLOCK_SIZE);
    memcpy(fs->O, fs->D[0], BLOCK_SIZE);
    
    int bytes_written = 0;
    int max_file_size = 3 * BLOCK_SIZE;  // 1536 bytes max

    while (bytes_written < n && fs->OFT[i].curr_pos < max_file_size) {
        int buf_offset = fs->OFT[i].curr_pos % BLOCK_SIZE;
        fs->OFT[i].rw_buffer[buf_offset] = fs->M[m + bytes_written];
        fs->OFT[i].curr_pos++;

        if (fs->OFT[i].curr_pos > fs->OFT[i].file_size) {
            fs->OFT[i].file_size = fs->OFT[i].curr_pos;
        }

        // Check if we've filled the current block
        if (fs->OFT[i].curr_pos % BLOCK_SIZE == 0 && bytes_written < n - 1) {
            int curr_fd_section = (fs->OFT[i].curr_pos / BLOCK_SIZE);
            
            // Check if we've reached max file size (3 blocks)
            if (curr_fd_section >= 3) {
                break;
            }

            // Write current block to disk
            int curr_block = get_fd_info(fs, fs->OFT[i].fd, curr_fd_section);
            memcpy(fs->D[curr_block], fs->OFT[i].rw_buffer, BLOCK_SIZE);

            // Get or allocate next block
            int next_block = get_fd_info(fs, fs->OFT[i].fd, curr_fd_section + 1);

            if (next_block != -1) {
                // Block already allocated, load it
                memcpy(fs->OFT[i].rw_buffer, fs->D[next_block], BLOCK_SIZE);
            } else {
                // Need to allocate new block
                for (int k = 8; k < N_BLOCKS; k++) {  // Start from block 8 (after directory)
                    if (get_bit_map_info(fs, k) == 0) {
                        next_block = k;
                        write_fd_info(fs, next_block, fs->OFT[i].fd, curr_fd_section + 1);
                        write_bit_map_info(fs, 1, next_block);
                        memset(fs->OFT[i].rw_buffer, 0, BLOCK_SIZE);
                        break;
                    }
                }
                
                if (next_block == -1) {
                    // No free blocks available
                    memcpy(fs->D[FD_BLOCK(fs->OFT[i].fd)], fs->I, BLOCK_SIZE);
                    memcpy(fs->D[0], fs->O, BLOCK_SIZE);
                    return bytes_written;
                }
            }
        }

        bytes_written++;
    }

    // Write updated descriptor and bitmap back to disk
    memcpy(fs->D[FD_BLOCK(fs->OFT[i].fd)], fs->I, BLOCK_SIZE);
    memcpy(fs->D[0], fs->O, BLOCK_SIZE);

    return bytes_written;
}

int seek(fs_node_t* fs, int i, int p)
{
    if (i < 0 || i >= 4 || fs->OFT[i].curr_pos == -1) return -1;
    
    if (p < 0 || p > fs->OFT[i].file_size) return -1;

    int p_block = p / BLOCK_SIZE;
    int curr_block = fs->OFT[i].curr_pos / BLOCK_SIZE;
    
    if (curr_block != p_block) {
        memcpy(fs->I, fs->D[FD_BLOCK(fs->OFT[i].fd)], BLOCK_SIZE);
        
        int prev_block = get_fd_info(fs, fs->OFT[i].fd, curr_block + 1);

        if (prev_block != -1) {
            memcpy(fs->D[prev_block], fs->OFT[i].rw_buffer, BLOCK_SIZE);
            int new_block = get_fd_info(fs, fs->OFT[i].fd, p_block + 1);
            if (new_block != -1) {
                memcpy(fs->OFT[i].rw_buffer, fs->D[new_block], BLOCK_SIZE);
            }
        }
    }

    fs->OFT[i].curr_pos = p;

    return 0;
}


int read_memory(fs_node_t* fs, int m, int n) 
{
    if (m < 0 || m >= BLOCK_SIZE || n < 0) return -1;

    int bytes_read = 0;

    while (bytes_read < n && (m + bytes_read) < BLOCK_SIZE) {
        printf("%c", fs->M[m + bytes_read]);
        bytes_read++;
    }
    printf("\n");

    return bytes_read;
}

int write_memory(fs_node_t* fs, int m, char* string) {
    if (m < 0 || m >= BLOCK_SIZE) return -1;

    int bytes_written = 0;
    int n = strlen(string);

    while (bytes_written < n && (m + bytes_written) < BLOCK_SIZE) {
        fs->M[m + bytes_written] = string[bytes_written];
        bytes_written++;
    }

    return bytes_written;
}

///// INIT /////
int init(fs_node_t* fs) {
    fs->D[0][0] = 0xff;
    memset(fs->D[0] + 1, 0, BLOCK_SIZE - 1);

    memset(fs->D[1], 0, BLOCK_SIZE * 6);

    for (int i = 1; i < N_FILE_DESC; i++) {
        int fd_block = FD_BLOCK(i);
        int fd_offset = FD_OFFSET(i);

        byte * fd_pos = fs->D[fd_block][fd_offset];

        for (int j = 0; j < 4; j++) {
            *(fd_pos + j) = (-1 >> (j * BITS_PER_BYTE)) & 0xff; 
        }
    }

    memset(fs->I, 0, sizeof(fs->I));
    memset(fs->O, 0, sizeof(fs->O));
    memset(fs->M, 0, sizeof(fs->M));

    memcpy(fs->O, fs->D[0], BLOCK_SIZE);

    fs->OFT[0].file_size = 0;
    fs->OFT[0].curr_pos = 0;
    fs->OFT[0].fd = 0;

    memcpy(fs->I, fs->D[1], BLOCK_SIZE);
    write_fd_info(fs, 7, 0, 1);
    memcpy(fs->D[1], fs->I, BLOCK_SIZE);

    seek(fs, 0, 0);

    for (int i = 1; i < 4; i++) {
        fs->OFT[i].fd = -1;
        fs->OFT[i].curr_pos = -1;
        fs->OFT[i].file_size = 0;
    } 
    
    return 0; 
}

int directory(fs_node_t* fs)
{
    int seek_err = seek(fs, 0, 0);
    if (seek_err < 0) return -1;
    
    memcpy(fs->I, fs->D[1], BLOCK_SIZE);
    
    printf("=================== directory ====================\n");
    
    for (int i = 0; i < BLOCK_SIZE / 8; i++) {
        char file_name[4];
        get_dir_info_name(fs, i, file_name);

        if (memcmp(file_name, "\0\0\0\0", 4) != 0) {
            int fd = get_dir_info_desc(fs, i);
            
            // Load correct descriptor block if needed
            if (fd >= 32) {
                memcpy(fs->I, fs->D[1 + (fd / 32)], BLOCK_SIZE);
            }
            
            int file_size = get_fd_info(fs, fd, 0);
            printf("file_name: %s | index_field: %d | file size: %d\n", 
                   file_name, fd, file_size);
        }
    }
    
    return 0;
}

#include "efs.h"

// The reserved blocks 0 through 6 must be accesses frequently to access the bitmap and the
// descriptors. It would not be practical to keep reading the blocks repeatedly from the disk.
// Instead, these blocks may be copied into a dedicated data structure after each init command.
// The data structure then serves as a form of main memory cache, where bytes or integers can
// be accessed directly (without any buffering or disk accesses).

// 2. The input and output buffers (I and O) can be eliminated if pointers are used. The read_block
// and write_block functions take a second parameter, which is pointer into memory. The data
// transfers can then take place directly between the disk and a read/write buffer in the OFT.
// Similarly, the functions can also access the reserved blocks 0 through 6 on the disk.

// 3. Some functions, such as create, open, etc. use the read/write functions internally to access the
// directory. Since the directory data is kept in the same array M as the data for files, make sure
// no file data is overwritten when reading/writing the directory. The easiest way is to use the
// high-end bytes of M for accessing the directory, while file data starts at M[0]. A more
// general approach is to have the write_memory function keep track of which portion of M is
// used for file data.

// stored in big-endian
byte D[N_BLOCKS][BLOCK_SIZE];
byte I[BLOCK_SIZE]; // fd manip
byte O[BLOCK_SIZE]; // bitmap
byte M[BLOCK_SIZE]; // block memory
OFT_entry OFT[4];

///// Helper Functions /////

int name_to_int (char name[4]) 
{
    int char_int = 0;
    for (int i = 0; i < 4; i++) {
        char_int |= ((int) name[i]) << (i * BYTE);
    }
    return char_int;
}

int get_fd_info(int i, int section) // 4 SECTIONS (BYTES): FILE_LENGTH (4) | BLOCK 0 (4) | BLOCK 1 (4) | BLOCK 2 (4) | 
{
    if (i < 0 || i >= N_FILE_DESC) return -1;

    int fd_block = FD_BLOCK(i);
    int fd_offset = FD_OFFSET(i);

    byte * fd_pos = I + fd_offset + section * 4; // there are four sections in a fd info piece, ea. 4 bytes wide
    int info = 0;
    for (int j = 0; j < 4; j++) {
        info |= ((int)fd_pos[j]) << (j * BYTE); // rebuilding int of fd info section  
    }

    return info;
}

int write_fd_info(int info, int fd, int section)
{
    if (fd < 0 || fd >= N_FILE_DESC) return -1;

    int fd_block = FD_BLOCK(fd);
    int fd_offset = FD_OFFSET(fd);
    
    byte * fd_pos = I + fd_offset + section * 4;

    for (int j = 0; j < 4; j++) {
        *(fd_pos + j) = (info >> (j * BYTE)) & 0xff; 
    }

    return 0;
}

int get_dir_info_name (int block, char name[4]) // 2 SECTIONS (BYTES): FILE_NAME (4) | DESCRIPTOR_INDEX (4)
{
    // OFT[0] ~ dir
    byte *start_pos = OFT[0].rw_buffer + block * 8;
    for (int i = 0; i < 4; i++) {
        name[i] = start_pos[i];
    }
    return 0;
}

int get_dir_info_desc (int block) // 2 SECTIONS (BYTES): FILE_NAME (4) | DESCRIPTOR_INDEX (4)
{
    // OFT[0] ~ dir
    byte * start_pos = OFT[0].rw_buffer + block * 8 + 4;
    int info = 0;
    for (int i = 0; i < 4; i++) {
        info |= ((int) start_pos[i]) << (i * BYTE);
    }
    
    return info;
}

int write_dir_info(void * info, int block, int section) 
{
    if (block < 0 || block >= 64) return -1;

    byte * start_pos = OFT[0].rw_buffer + block * BYTE + section * 4;

    if (section == 0) {
        byte * info_value = (byte *) info;
        for (int i = 0; i < 4; i++) {
            start_pos[i] = info_value[i];
        }

    } else {
        int info_value = *(int *)info;
        for (int i = 0; i < 4; i++) {
            start_pos[i] = (info_value >> (i * BYTE)) & 0xff;
        }
    }
    
    return 0;
}

int get_bit_map_info(int block) {
    byte byte = O[BIT_MAP_BLOCK(block)];
    return (byte >> BIT_MAP_OFFSET(block)) & 0x1;
}

int write_bit_map_info(int info, int block) {
    if (block < 1 || block > N_BLOCKS - 1) return -1;

    byte * byte = &O[BIT_MAP_BLOCK(block)];
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


int str_cmp_int_file_name(int int_file_name, const char file_name[4]) {
    char int_to_char_arr[4];

    for (int i = 0; i < 4; i++) {
        int_to_char_arr[i] = (char) ((int_file_name >> (i * BYTE)) & 0xff);
    }

    return strcmp(int_to_char_arr, file_name);
}

///// TESTING FUNCTIONS /////

int direct_read_memory(int m, int n) // m = start_pos, n = number of bytes
{
    if (m < 0 || m > N_BLOCKS * BLOCK_SIZE) return -1;

    int bytes_read = 0;

    while (bytes_read < n && m + bytes_read < N_BLOCKS * BLOCK_SIZE) {
        // printf("BYTES READ %d INT n: %d\n", bytes_read, n);
        int m_block = (m + bytes_read) / BLOCK_SIZE;
        int m_offset = (m + bytes_read) % BLOCK_SIZE;

        switch (m_block) {
            case 0: {
                
                // printf("%d", D[m_block][bit_block]);
                for (int i = 0; i < BYTE; i++) {
                    printf("%d", (D[m_block][m_offset] >> i) & 0x1);
                }
                

                printf("\n");
                break;
            }

            case 1 ... 7:
                
                printf("%02x ", D[m_block][m_offset]);
                if ((bytes_read + 1) % 16  == 0) printf("\n");
                break;

            case 8 ... 63:
                printf("%c ", (char)D[m_block][m_offset]);
                if ((bytes_read + 1) % 16  == 0) printf("\n");
                break;
        };
        
        bytes_read++;
    }

    return bytes_read;
}

int direct_write_memory(int m, char* string) 
{
    if (m < 0 || m > N_BLOCKS * BLOCK_SIZE) return -1;

    int bytes_written = 0;
    int n = strlen(string);

    while (bytes_written < n && (m + bytes_written) < N_BLOCKS * BLOCK_SIZE) {
        int m_block = (m + bytes_written) / BLOCK_SIZE;
        int m_offset = (m + bytes_written) % BLOCK_SIZE;
        D[m_block][m_offset] = string[bytes_written];

        bytes_written++;
    }

    return bytes_written;
}
         
///// FILE SYS MANIP OPERATIONS /////

int create(char name[4]) {
    int seek_err = seek(0, 0);
    if (seek_err < 0) {
        return -1;
    }
    
    // check if file_name exists
    int free_entry = -1;
    for (int i = 0; i < BLOCK_SIZE / 8; i++) {
        char file_name[4];
        get_dir_info_name(i, file_name);
        
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
            memcpy(I, D[1 + (i / 32)], BLOCK_SIZE);
        }

        int file_length = get_fd_info(i, 0);
        if (file_length == -1) {
            free_fd = i;
            write_fd_info(0, i, 0);
            break;
        }
    }

    if (free_fd == -1) return -1;  // No free descriptor

    // Update directory
    write_dir_info(name, free_entry, 0);
    write_dir_info(&free_fd, free_entry, 1);

    // Write descriptor and directory back to disk
    memcpy(D[1 + free_fd/32], I, BLOCK_SIZE);
    memcpy(D[7], OFT[0].rw_buffer, BLOCK_SIZE);
    
    return 0;
}

int destroy(char name[4]) 
{
    int seek_err = seek(0, 0);
    if (seek_err < 0) {
        return -1;
    }

    // search through directory for matching filenames
    int fd = -1;
    int dir_index = -1;
    
    for (int i = 0; i < BLOCK_SIZE / 8; i++) {
        char file_name[4];
        get_dir_info_name(i, file_name);

        if (strcmp(file_name, name) == 0) {
            fd = get_dir_info_desc(i);
            dir_index = i;
            break;
        }
    }

    if (fd == -1) return -1;  // File not found

    // Check if file is open
    for (int k = 1; k < 4; k++) {
        if (OFT[k].curr_pos != -1 && OFT[k].fd == fd) {
            return -1; // cannot destroy open file
        }
    }

    // Free the file descriptor and blocks
    int fd_block = FD_BLOCK(fd);
    memcpy(I, &D[fd_block], BLOCK_SIZE);
    memcpy(O, &D[0], BLOCK_SIZE);

    // Mark descriptor as free and free all blocks
    write_fd_info(-1, fd, 0);
    
    for (int i = 1; i < 4; i++) {
        int block = get_fd_info(fd, i);

        if (block != -1 && block > 0) {
            write_bit_map_info(0, block);
            write_fd_info(-1, fd, i);  // FIX 2: Set to -1, not 0
        }
    }

    // Clear directory entry
    int zero = 0;
    write_dir_info(&zero, dir_index, 0);
    write_dir_info(&zero, dir_index, 1);  // FIX 3: Clear descriptor index too
    
    // Write changes back to disk
    memcpy(&D[fd_block], I, BLOCK_SIZE);
    memcpy(&D[0], O, BLOCK_SIZE);
    memcpy(D[7], OFT[0].rw_buffer, BLOCK_SIZE);

    return 0;
}

int open(char name[4]) 
{   
    int fd = -1;

    // find file in dir, search through directory entries
    for (int i = 0; i < BLOCK_SIZE / 8; i++) {  // 64 entries max in one block
        char dir_name[4];
        get_dir_info_name(i, dir_name);
        if (strcmp(name, dir_name) == 0) {
            fd = get_dir_info_desc(i);
            break;
        }
    }

    if (fd == -1) return -1;

    // file is already opened
    for (int k = 1; k < 4; k++) {
        if (OFT[k].curr_pos != -1 && OFT[k].fd == fd) {
            return -1;
        }
    }

    int free_oft = -1;
    for (int i = 1; i < 4; i++) {
        if (OFT[i].curr_pos == -1) {
            memcpy(I, &D[FD_BLOCK(fd)], BLOCK_SIZE);

            free_oft = i;
            OFT[i].fd = fd;
            OFT[i].curr_pos = 0;
            OFT[i].file_size = get_fd_info(fd, 0);
            
            // allocate properly if file_size == 0
            if (OFT[i].file_size == 0) {
                int free_block = -1;
                for (int j = 8; j < N_BLOCKS; j++) {
                    if (get_bit_map_info(j) == 0) {
                        memcpy(O, &D[0], BLOCK_SIZE);

                        free_block = j;
                        write_fd_info(free_block, fd, 1);
                        write_bit_map_info(1, free_block);
                        
                        memcpy(&D[FD_BLOCK(fd)], I, BLOCK_SIZE);
                        memset(OFT[i].rw_buffer, 0, BLOCK_SIZE);
                        memcpy(&D[0], O, BLOCK_SIZE);
                        break;
                    }
                }
                
                if (free_block == -1) {
                    OFT[i].fd = -1;
                    OFT[i].curr_pos = -1;
                    OFT[i].file_size = 0;
                    free_oft = -1;
                }

            } else {
                int fd_first_block = get_fd_info(fd, 1);
                if (fd_first_block != -1) {
                    memcpy(OFT[i].rw_buffer, &D[fd_first_block], BLOCK_SIZE);
                }
            }

            break;
        }
    }
    return free_oft;
}

int close(int i) 
{
    if (i < 0 || i >= 4)
        return -1;

    if (OFT[i].curr_pos == -1)  // Check if entry is free
        return -1;

    // Write current block back to disk
    int curr_block = OFT[i].curr_pos / BLOCK_SIZE;
    int fd_section = curr_block + 1;
    
    memcpy(I, D[FD_BLOCK(OFT[i].fd)], BLOCK_SIZE);
    int disk_block = get_fd_info(OFT[i].fd, fd_section);

    if (disk_block != -1) {
        memcpy(D[disk_block], OFT[i].rw_buffer, BLOCK_SIZE);
    }

    // Update file size in descriptor
    write_fd_info(OFT[i].file_size, OFT[i].fd, 0);
    memcpy(D[FD_BLOCK(OFT[i].fd)], I, BLOCK_SIZE);

    // Mark OFT entry as free
    OFT[i].fd = -1;
    OFT[i].curr_pos = -1;
    OFT[i].file_size = 0;
    memset(OFT[i].rw_buffer, 0, BLOCK_SIZE);

    return 0;
}

int f_read(int i, int m, int n)
{
    if (i < 0 || i >= 4 || OFT[i].curr_pos == -1) return -1;
    if (m < 0 || n < 0) return -1;
    
    memcpy(I, D[FD_BLOCK(OFT[i].fd)], BLOCK_SIZE);
    
    int bytes_read = 0;
    
    while (bytes_read < n && OFT[i].curr_pos < OFT[i].file_size) {
        int buf_offset = OFT[i].curr_pos % BLOCK_SIZE;
        M[m + bytes_read] = OFT[i].rw_buffer[buf_offset];

        bytes_read++;
        OFT[i].curr_pos++;

        if (OFT[i].curr_pos % BLOCK_SIZE == 0 && OFT[i].curr_pos < OFT[i].file_size) {
            int next_block_section = (OFT[i].curr_pos / BLOCK_SIZE) + 1;
            int next_block = get_fd_info(OFT[i].fd, next_block_section);

            if (next_block == -1) break;

            memcpy(OFT[i].rw_buffer, D[next_block], BLOCK_SIZE);
        }
    }

    return bytes_read;
}

int f_write(int i, int m, int n)
{
    if (i < 0 || i >= 4 || OFT[i].curr_pos == -1) return -1;
    
    memcpy(I, D[FD_BLOCK(OFT[i].fd)], BLOCK_SIZE);
    memcpy(O, D[0], BLOCK_SIZE);
    
    int bytes_written = 0;
    int max_file_size = 3 * BLOCK_SIZE;  // 1536 bytes max

    while (bytes_written < n && OFT[i].curr_pos < max_file_size) {
        int buf_offset = OFT[i].curr_pos % BLOCK_SIZE;
        OFT[i].rw_buffer[buf_offset] = M[m + bytes_written];
        OFT[i].curr_pos++;

        if (OFT[i].curr_pos > OFT[i].file_size) {
            OFT[i].file_size = OFT[i].curr_pos;
        }

        // Check if we've filled the current block
        if (OFT[i].curr_pos % BLOCK_SIZE == 0 && bytes_written < n - 1) {
            int curr_fd_section = (OFT[i].curr_pos / BLOCK_SIZE);
            
            // Check if we've reached max file size (3 blocks)
            if (curr_fd_section >= 3) {
                break;
            }

            // Write current block to disk
            int curr_block = get_fd_info(OFT[i].fd, curr_fd_section);
            memcpy(D[curr_block], OFT[i].rw_buffer, BLOCK_SIZE);

            // Get or allocate next block
            int next_block = get_fd_info(OFT[i].fd, curr_fd_section + 1);

            if (next_block != -1) {
                // Block already allocated, load it
                memcpy(OFT[i].rw_buffer, D[next_block], BLOCK_SIZE);
            } else {
                // Need to allocate new block
                for (int k = 8; k < N_BLOCKS; k++) {  // Start from block 8 (after directory)
                    if (get_bit_map_info(k) == 0) {
                        next_block = k;
                        write_fd_info(next_block, OFT[i].fd, curr_fd_section + 1);
                        write_bit_map_info(1, next_block);
                        memset(OFT[i].rw_buffer, 0, BLOCK_SIZE);
                        break;
                    }
                }
                
                if (next_block == -1) {
                    // No free blocks available
                    memcpy(D[FD_BLOCK(OFT[i].fd)], I, BLOCK_SIZE);
                    memcpy(D[0], O, BLOCK_SIZE);
                    return bytes_written;
                }
            }
        }

        bytes_written++;
    }

    // Write updated descriptor and bitmap back to disk
    memcpy(D[FD_BLOCK(OFT[i].fd)], I, BLOCK_SIZE);
    memcpy(D[0], O, BLOCK_SIZE);

    return bytes_written;
}

int seek(int i, int p)
{
    if (i < 0 || i >= 4 || OFT[i].curr_pos == -1) return -1;
    
    if (p < 0 || p > OFT[i].file_size) return -1;

    int p_block = p / BLOCK_SIZE;
    int curr_block = OFT[i].curr_pos / BLOCK_SIZE;
    
    if (curr_block != p_block) {
        memcpy(I, D[FD_BLOCK(OFT[i].fd)], BLOCK_SIZE);
        
        int prev_block = get_fd_info(OFT[i].fd, curr_block + 1);

        if (prev_block != -1) {
            memcpy(D[prev_block], OFT[i].rw_buffer, BLOCK_SIZE);
            int new_block = get_fd_info(OFT[i].fd, p_block + 1);
            if (new_block != -1) {
                memcpy(OFT[i].rw_buffer, D[new_block], BLOCK_SIZE);
            }
        }
    }

    OFT[i].curr_pos = p;

    return 0;
}


int read_memory(int m, int n) 
{
    if (m < 0 || m >= BLOCK_SIZE || n < 0) return -1;

    int bytes_read = 0;

    while (bytes_read < n && (m + bytes_read) < BLOCK_SIZE) {
        printf("%c", M[m + bytes_read]);
        bytes_read++;
    }
    printf("\n");

    return bytes_read;
}

int write_memory(int m, char* string) {
    if (m < 0 || m >= BLOCK_SIZE) return -1;

    int bytes_written = 0;
    int n = strlen(string);

    while (bytes_written < n && (m + bytes_written) < BLOCK_SIZE) {
        M[m + bytes_written] = string[bytes_written];
        bytes_written++;
    }

    return bytes_written;
}

///// INIT /////
int init() {
    D[0][0] = 0xff;
    memset(D[0] + 1, 0, BLOCK_SIZE - 1);

    memset(D[1], 0, BLOCK_SIZE * 6);

    for (int i = 1; i < N_FILE_DESC; i++) {
        int fd_block = FD_BLOCK(i);
        int fd_offset = FD_OFFSET(i);

        byte * fd_pos = &D[fd_block][fd_offset];

        for (int j = 0; j < 4; j++) {
            *(fd_pos + j) = (-1 >> (j * BYTE)) & 0xff; 
        }
    }

    memset(I, 0, sizeof(I));
    memset(O, 0, sizeof(O));
    memset(M, 0, sizeof(M));

    memcpy(O, D[0], BLOCK_SIZE);

    OFT[0].file_size = 0;
    OFT[0].curr_pos = 0;
    OFT[0].fd = 0;

    memcpy(I, D[1], BLOCK_SIZE);
    write_fd_info(7, 0, 1);
    memcpy(D[1], I, BLOCK_SIZE);

    seek(0, 0);

    for (int i = 1; i < 4; i++) {
        OFT[i].fd = -1;
        OFT[i].curr_pos = -1;
        OFT[i].file_size = 0;
    } 
    
    return 0; 
}

int directory()
{
    int seek_err = seek(0, 0);
    if (seek_err < 0) return -1;
    
    memcpy(I, D[1], BLOCK_SIZE);
    
    printf("=================== directory ====================\n");
    
    for (int i = 0; i < BLOCK_SIZE / 8; i++) {
        char file_name[4];
        get_dir_info_name(i, file_name);

        if (memcmp(file_name, "\0\0\0\0", 4) != 0) {
            int fd = get_dir_info_desc(i);
            
            // Load correct descriptor block if needed
            if (fd >= 32) {
                memcpy(I, D[1 + (fd / 32)], BLOCK_SIZE);
            }
            
            int file_size = get_fd_info(fd, 0);
            printf("file_name: %s | index_field: %d | file size: %d\n", 
                   file_name, fd, file_size);
        }
    }
    
    return 0;
}

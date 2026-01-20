#include "efs.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

extern byte I[512];
extern byte D[64][512];
extern byte O[512];

void print_test(const char* test_name)
{
    printf("\n===== Testing %s =====\n", test_name);
}

void test_direct_write_read_memory () 
{
    print_test("Testing writing and reading to memory");
    write_memory(512, "ABCDEFGHIJKLMNO\0");
    direct_read_memory(BLOCK_SIZE * 1, 512);
}

void test_dir_info_read_write () 
{
    init();
    print_test("DIR Info Write ~ FILE NAMES");

    char * names[] = {"ABC\0", "DEF\0"};
    int names_in_int[] = {0x00434241, 0x00464544};

    for (int i = 0; i < 2; i++) {
        write_dir_info(names[i], i + 2, 0);
        char file_name[4];
        int info = get_dir_info_name(i + 2, file_name);
        assert(strcmp(file_name, names[i]) == 0);

        printf("asserting name %s to block %d worked successfully\n", names[i], i);
    }
}

void test_fd_info_read_write () 
{
    init();
    print_test("FD Info Write ~ FILE LENGTHS");

    int lengths[] = {10, 1000};

    for (int i = 0; i < 2; i++) {
        write_fd_info(lengths[i], i, 0);
        int info = get_fd_info(i, 0);
        assert(info == lengths[i]);

        printf("asserting length %d to fd %d worked successfully\n", lengths[i], i);
    }
}

void test_bit_map_read_write () 
{
    init();
    print_test("Bitmap Read Write");
    memcpy(O, D[0], BLOCK_SIZE);

    int free_blocks[] = {8, 9, 53, 64};
    int used_blocks[] = {0, 1, 2, 3, 4, 5, 6, 7};

    // testing init stage
    for (int i = 0; i < 4; i++) {
        int bit = get_bit_map_info(free_blocks[i]);
        assert(bit == 0);
    }
    printf("Init free blocks in bitmap to 0 correctly\n");

    for (int i = 0; i < 8; i++) {
        int bit = get_bit_map_info(used_blocks[i]);
        assert(bit == 1);
    }
    printf("Init used blocks in bitmap to 1 correctly\n");

    for (int i = 0; i < 4; i++) {
        int write_err = write_bit_map_info(1, free_blocks[i]);
        assert(get_bit_map_info(free_blocks[i] ==  1));
    }
    printf("Turning used bit on worked sucessfully\n");

    for (int i = 0; i < 4; i++) {
        int write_err = write_bit_map_info(0, free_blocks[i]);
        assert(get_bit_map_info(free_blocks[i] ==  0));
    }
    printf("Turning used bit off worked sucessfully\n");

}

void test_create_destroy_file () 
{
    init();
    print_test("Creating & Destroying File");
    char * names[] = {"ABC\0", "DEF\0"};
    int fd[] = {1, 2};
    int blocks_in_dir[] = {1, 2};
    
    for (int i = 0; i < 2; i++) { // should get fd and blocks 1 & 2 respectively
        create(names[i]);
        char file_name[4];
        get_dir_info_name(blocks_in_dir[i], file_name);
        assert(strcmp(file_name, names[i]) == 0);

        int fd_info = get_dir_info_desc(blocks_in_dir[i]);
        assert(fd_info == fd[i]);
    }

    //  test_write_direct_read_memory();

    printf("Created files correclty & matching file names and fd\n");
    // directory();

    for (int i = 0; i < 2; i++) {
        destroy(names[i]);
        char file_name[4];
        int file_name_info = get_dir_info_name(blocks_in_dir[i], file_name);
        assert(memcmp(file_name, "\0\0\0\0", 4) == 0);

        int file_length_info = get_fd_info(fd[i], 0);
        assert(file_length_info == -1);

        // implement checking blocks when finished with write functions
    }
    printf("Destroyed files successfully, dir_info correctly erased, fd info correctly erased\n");
}


void test_init () {
    print_test("Testing initialization");
    init();

    int size_err = 0;
    for (int i = 1; i < N_FILE_DESC; i++) {
        int fd_block = FD_BLOCK(i);
        int fd_offset = FD_OFFSET(i);

        byte * fd_pos = &D[fd_block][fd_offset]; // there are four sections in a fd info piece, ea. 4 bytes wide
        int info = 0;
        for (int j = 0; j < 4; j++) {
            info |= ((int)fd_pos[j]) << (j * BYTE);
        }

        // printf("INFO = %d FD = %d\n", info, i);
        if (info != -1) {
            size_err = -1;
            break;
        }
    }
    assert(size_err == 0);
    printf("All fd length info is -1 except dir\n");
}


void test_run () 
{
    print_test("full test run");

    init();
    char * names[] = {"ABC\0", "DEF\0"};
    int fd[] = {1, 2};
    int blocks_in_dir[] = {1, 2};
    
    for (int i = 0; i < 2; i++) { // should get fd and blocks 1 & 2 respectively
        create(names[i]);
        char file_name[4];
        get_dir_info_name(blocks_in_dir[i], file_name);
        assert(strcmp(file_name, names[i]) == 0);

        int fd_info = get_dir_info_desc(blocks_in_dir[i]);
        // printf("fd info : %d\n", fd_info);
        assert(fd_info == fd[i]);
    }

    for (int i = 0; i < 2; i++) {
        int open_flag = open(names[i]);
        assert(open_flag != -1);
    }



    printf("\ndirectory and bitmap after creating and opening two files\n");
    directory();

    printf("\nbitmap\n");
    direct_read_memory(0, 3);
    printf("\n");

    write_memory(0, "abcdefghijk");

    printf("reading memory after writing abcdefghijk\n");
    read_memory(0, 11);
    printf("\n");


    write(1, 0, 11);
    printf("after writing abcdefghijk into OFT[i] ~ fd 1, block 8\n");
    directory();
    printf("D array memory: \n");
    direct_read_memory(BLOCK_SIZE * 8, 11);

    close(1);

    printf("\nafter closing OFT index 1\n");
    directory();

    printf("\nafter destroying ABC\n");
    destroy("ABC\0");
    directory();
}

int main () {
    // test_init();
    // test_bit_map_read_write();
    // test_dir_info_read_write();
    // test_fd_info_read_write();
    // test_create_destroy_file();
    // test_direct_write_read_memory();
    test_run();
}
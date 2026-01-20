#include "efs.h"
#include <unistd.h>

#define max_line 1024
#define max_argc 512

extern byte I[512];
extern byte O[512];
extern byte M[512];
extern byte D[64][512];
extern OFT_entry OFT[4];

static int init_flag = 0;

int convert_to_int (char * string) {
    char * endptr = NULL;
    int i = strtol(string, &endptr, 10);

    if (*endptr != '\0') {
        printf("Conversion error %s\n", endptr);
        return -1;
    }
    return i;
}

void process_user_commands(char command[3], char * parameters[max_argc], int argc)
{
    if (strcmp("in", command) == 0 && argc == 1) {
        if (init_flag) {
            printf("error\n");  // Already initialized
            return;
        }
        init();
        init_flag = 1;
        printf("system initialized\n"); 

    } else if (strcmp("wm", command) == 0 && argc >= 3) {
        char *endptr;
        int num = strtol(parameters[0], &endptr, 10);

        if (*endptr != '\0') {
            printf("error\n");
        } else {
            int total_len = 0;
            for (int i = 1; i < argc - 1; i++) {
                total_len += strlen(parameters[i]) + 1;
            }
            
            char combined[total_len]; 
            combined[0] = '\0';

            for (int i = 1; i < argc - 1; i++) {
                strcat(combined, parameters[i]);
                if (i < argc - 2) {
                    strcat(combined, " ");
                }
            }

            int write_memory_flag = write_memory(num, combined);
            if (write_memory_flag > 0) {
                printf("%d bytes written to M\n", write_memory_flag);
            } else {
                printf("error\n");
            }
        }

    } else if (strcmp("cr", command) == 0 && argc == 2) {
        if (strlen(parameters[0]) > 4) {
            printf("error\n");
            return;
        }
        int create_flag = create(parameters[0]);
        if (create_flag == 0) {
            printf("%s created\n", parameters[0]); 
        } else {
            printf("error\n"); 
        }
    
    } else if (strcmp("op", command) == 0 && argc == 2) {
        if (strlen(parameters[0]) > 4) {
            printf("error\n");
            return;
        }
        int open_flag = open(parameters[0]);
        if (open_flag >= 0) {
            printf("%s opened %d\n", parameters[0], open_flag);
        } else {
            printf("error\n");
        }

    } else if (strcmp("de", command) == 0 && argc == 2) {
        if (strlen(parameters[0]) > 4) {
            printf("error\n");
            return;
        }
        int destroy_flag = destroy(parameters[0]);
        if (destroy_flag == 0) {
            printf("%s destroyed\n", parameters[0]);
        } else {
            printf("error\n");
        }

    } else if (strcmp("wr", command) == 0 && argc == 4) {
        int i = convert_to_int(parameters[0]);
        if (i == -1) {
            printf("error\n");
            return;
        }

        int m = convert_to_int(parameters[1]);
        if (m == -1) {
            printf("error\n");
            return;
        }

        int n = convert_to_int(parameters[2]);
        if (n == -1) {
            printf("error\n");
            return;
        }

        int bytes = f_write(i, m, n);
        if (bytes >= 0) {
            printf("%d bytes written to %d\n", bytes, i);
        } else {
            printf("error\n");
        }
    
    } else if (strcmp("sk", command) == 0 && argc == 3) {
        int i = convert_to_int(parameters[0]);
        if (i == -1) {
            printf("error\n");
            return;
        }

        int p = convert_to_int(parameters[1]);
        if (p == -1) {
            printf("error\n");
            return;
        }

        int seek_flag = seek(i, p);
        if (seek_flag == 0) {
            printf("position is %d\n", p); 
        } else {
            printf("error\n");
        }

    } else if (strcmp("rd", command) == 0 && argc == 4) {
        int i = convert_to_int(parameters[0]);
        if (i == -1) {
            printf("error\n");
            return;
        }

        int m = convert_to_int(parameters[1]);
        if (m == -1) {
            printf("error\n");
            return;
        }

        int n = convert_to_int(parameters[2]);
        if (n == -1) {
            printf("error\n");
            return;
        }

        int bytes = f_read(i, m, n);
        if (bytes >= 0) {
            printf("%d bytes read from %d\n", bytes, i); 
        } else {
            printf("error\n");
        }

    } else if (strcmp("rm", command) == 0 && argc == 3) {
        int m = convert_to_int(parameters[0]);
        if (m == -1) {
            printf("error\n");
            return;
        }

        int n = convert_to_int(parameters[1]);
        if (n == -1) {
            printf("error\n");
            return;
        }

        int result = read_memory(m, n);
        if (result < 0) {
            printf("error\n");
        }
        // read_memory already prints the characters

    } else if (strcmp("cl", command) == 0 && argc == 2) {
        int i = convert_to_int(parameters[0]);
        if (i == -1) {
            printf("error\n");
            return;
        }

        int close_flag = close(i);
        if (close_flag == 0) {
            printf("%d closed\n", i); 
        } else {
            printf("error\n");
        }

    } else if (strcmp("dr", command) == 0 && argc == 1) {
        directory();
        // directory function handles its own output

    } else {
        printf("error\n");
    }
}

int main ()
{
    char buf[max_line];
    
    printf("File System Shell\n");
    printf("Commands: in, cr <name>, de <name>, op <name>, cl <i>, rd <i> <m> <n>, wr <i> <m> <n>, sk <i> <p>, dr, rm <m> <n>, wm <m> <string>\n");
    printf("Type 'exit' to quit\n\n");
    
    while (1) 
    {
        write(STDOUT_FILENO, "> ", 2);

        ssize_t n = read(STDIN_FILENO, buf, max_line);
        if (n <= 0) break;
        
        if (buf[n-1] == '\n') {
            buf[n-1] = '\0';
        } else {
            buf[n] = '\0';
        }

        if (strcmp(buf, "exit") == 0)   
            break;
         
        char command[3];
        char * argv[max_argc];
        int argc = 0;
        
        char * tok = strtok(buf, " ");

        while (tok && argc < max_argc) {
            argv[argc] = tok;
            argc++;
            tok = strtok(NULL, " ");
        }

        if (argc == 0) {
            continue;
        }

        // Check command length
        if (strlen(argv[0]) != 2) {
            printf("Invalid command '%s' (commands must be 2 characters)\n", argv[0]);
            continue;
        }
        
        // Copy command
        strncpy(command, argv[0], 2);
        command[2] = '\0';

        process_user_commands(command, argv + 1, argc);
    }

    return 0;
}
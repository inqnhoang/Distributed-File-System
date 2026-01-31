#include "dfs.h"
#include "efs.h"
#include "wal.h"
#include <stdio.h>
#include <stdlib.h>


int dfs_replication_operation(dfs_t* dfs, wal_entry_t* entry) {
    if (dfs == NULL || entry == NULL) {
        return -1;
    }
    
    // Apply the operation to all nodes
    for (int i = 0; i < NUM_NODES; i++) {
        int result = wal_apply_entry(&dfs->file_systems[i], i, entry);
        
        if (result < 0) {
            printf("Failed to replicate to node %d\n", i);
            return -1;  // Fail if any node fails
        }
    }
    
    return 0;  // All nodes succeeded
}

void dfs_process_command(dfs_t *dfs, char command[3], char *parameters[MAX_ARGC], int argc)
{
    if (strcmp("in", command) == 0 && argc == 1) {
        dfs_init(dfs);
        printf("distributed system initialized\n");

    } else if (strcmp("wm", command) == 0 && argc >= 3) {
        // Parse node_id and memory position
        int node_id = convert_to_int(parameters[0]);
        if (node_id == -1 || node_id >= NUM_NODES) {
            printf("error\n");
            return;
        }

        int m = convert_to_int(parameters[1]);
        if (m == -1) {
            printf("error\n");
            return;
        }

        // Combine remaining parameters into a single string
        int total_len = 0;
        for (int i = 2; i < argc; i++) {
            total_len += strlen(parameters[i]) + 1;
        }
        
        char combined[total_len]; 
        combined[0] = '\0';

        for (int i = 2; i < argc; i++) {
            strcat(combined, parameters[i]);
            if (i < argc - 1) {
                strcat(combined, " ");
            }
        }

        int bytes = write_memory(&dfs->file_systems[node_id], m, combined);
        if (bytes > 0) {
            printf("%d bytes written to M on node %d\n", bytes, node_id);
        } else {
            printf("error\n");
        }

    } else if (strcmp("cr", command) == 0 && argc == 2) {
        if (strlen(parameters[0]) > 4) {
            printf("error\n");
            return;
        }
        
        wal_entry_t entry = wal_log_create(dfs, parameters[0]);
        if (entry.sequence_number >= 0) {
            int result = dfs_replicate_operation(dfs, entry);
            if (result == 0) {
                printf("%s created on all nodes\n", parameters[0]);
            } else {
                printf("error\n");
            }
        } else {
            printf("error\n");
        }

    } else if (strcmp("de", command) == 0 && argc == 2) {
        if (strlen(parameters[0]) > 4) {
            printf("error\n");
            return;
        }
        
        wal_entry_t entry = wal_log_destroy(dfs, parameters[0]);
        if (entry.sequence_number >= 0) {
            int result = dfs_replicate_operation(dfs, entry);
            if (result == 0) {
                printf("%s destroyed on all nodes\n", parameters[0]);
            } else {
                printf("error\n");
            }
        } else {
            printf("error\n");
        }

    } else if (strcmp("op", command) == 0 && argc == 3) {
        // op node_id filename
        int node_id = convert_to_int(parameters[0]);
        if (node_id == -1 || node_id >= NUM_NODES) {
            printf("error\n");
            return;
        }

        if (strlen(parameters[1]) > 4) {
            printf("error\n");
            return;
        }

        int oft_idx = open(&dfs->file_systems[node_id], parameters[1]);
        if (oft_idx >= 0) {
            printf("%s opened at %d on node %d\n", parameters[1], oft_idx, node_id);
        } else {
            printf("error\n");
        }

    } else if (strcmp("cl", command) == 0 && argc == 3) {
        // cl node_id oft_index
        int node_id = convert_to_int(parameters[0]);
        if (node_id == -1 || node_id >= NUM_NODES) {
            printf("error\n");
            return;
        }

        int i = convert_to_int(parameters[1]);
        if (i == -1) {
            printf("error\n");
            return;
        }

        int result = close(&dfs->file_systems[node_id], i);
        if (result == 0) {
            printf("%d closed on node %d\n", i, node_id);
        } else {
            printf("error\n");
        }

    } else if (strcmp("wr", command) == 0 && argc == 4) {
        // wr oft_idx m n - writes to all nodes via WAL
        int oft_idx = convert_to_int(parameters[0]);
        if (oft_idx == -1) {
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

        // Get data from leader's memory buffer
        byte data[512];
        memcpy(data, dfs->file_systems[dfs->leader].M + m, n);

        wal_entry_t entry = wal_log_write(dfs, oft_idx, m, n, data);
        if (entry.sequence_number >= 0) {
            int result = dfs_replicate_operation(dfs, entry);
            if (result >= 0) {
                printf("%d bytes written to all nodes\n", n);
            } else {
                printf("error\n");
            }
        } else {
            printf("error\n");
        }

    } else if (strcmp("rd", command) == 0 && argc == 5) {
        // rd node_id oft_idx m n
        int node_id = convert_to_int(parameters[0]);
        if (node_id == -1 || node_id >= NUM_NODES) {
            printf("error\n");
            return;
        }

        int i = convert_to_int(parameters[1]);
        if (i == -1) {
            printf("error\n");
            return;
        }

        int m = convert_to_int(parameters[2]);
        if (m == -1) {
            printf("error\n");
            return;
        }

        int n = convert_to_int(parameters[3]);
        if (n == -1) {
            printf("error\n");
            return;
        }

        int bytes = f_read(&dfs->file_systems[node_id], i, m, n);
        if (bytes >= 0) {
            printf("%d bytes read from node %d\n", bytes, node_id);
        } else {
            printf("error\n");
        }

    } else if (strcmp("sk", command) == 0 && argc == 3) {
        // sk oft_idx position - replicated via WAL
        int oft_idx = convert_to_int(parameters[0]);
        if (oft_idx == -1) {
            printf("error\n");
            return;
        }

        int position = convert_to_int(parameters[1]);
        if (position == -1) {
            printf("error\n");
            return;
        }

        wal_entry_t entry = wal_log_seek(dfs, oft_idx, position);
        if (entry.sequence_number >= 0) {
            int result = dfs_replicate_operation(dfs, entry);
            if (result == 0) {
                printf("position is %d on all nodes\n", position);
            } else {
                printf("error\n");
            }
        } else {
            printf("error\n");
        }

    } else if (strcmp("rm", command) == 0 && argc == 4) {
        // rm node_id m n
        int node_id = convert_to_int(parameters[0]);
        if (node_id == -1 || node_id >= NUM_NODES) {
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

        int result = read_memory(&dfs->file_systems[node_id], m, n);
        if (result < 0) {
            printf("error\n");
        }

    } else if (strcmp("dr", command) == 0 && argc == 2) {
        // dr node_id
        int node_id = convert_to_int(parameters[0]);
        if (node_id == -1 || node_id >= NUM_NODES) {
            printf("error\n");
            return;
        }

        directory(&dfs->file_systems[node_id]);

    } else {
        printf("error\n");
    }
}

int dfs_read_operation(dfs_t * dfs, char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("error opening file.\n");
        return -1; 
    }

    char line[256];
    char *argv[MAX_ARGC];
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        
        int argc = 0;
        char *token = strtok(line, " ");
        if (token == NULL) continue;
        
        char command[3];
        strcpy(command, token);
        
        token = strtok(NULL, " ");
        while (token != NULL && argc < MAX_ARGC) {
            argv[argc++] = token;
            token = strtok(NULL, " ");
        }
        
        dfs_process_command(dfs, command, argv, argc);
    }

    fclose(fp);
    return 0;
}

void dfs_init (dfs_t * dfs) {
    // initialize dfs structure 
    for (int i = 0; i < NUM_NODES; i++) {
        init(&dfs->file_systems[i]);
    }
    dfs->leader = 0;
    dfs->global_sequence_counter = 0;
}

int main () {
    dfs_t * dfs = malloc(sizeof(dfs_t));
    dfs_init(dfs);

    return 0;
}
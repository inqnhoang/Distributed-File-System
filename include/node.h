#ifndef NODE_H
#define NODE_H

typedef enum {
    ACTIVE,
    FAILED,
    RECOVERING,
    LAGGING
} node_status_t;

typedef struct {
    int node_id;
    node_status_t status;
} node_t;

#endif

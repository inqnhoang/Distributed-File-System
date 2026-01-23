#ifndef OPERATION_H
#define OPERATION_H

#include "time.h"
#include "types.h"

typedef enum {
    OP_CREATE,
    OP_DESTROY,
    OP_WRITE,
    OP_OPEN,
    OPEN_CLOSE,
    OP_SEEK
} operation_type_h;

#endif
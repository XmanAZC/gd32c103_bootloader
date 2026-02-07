#ifndef XLINK_PORT_STDLIB_H
#define XLINK_PORT_STDLIB_H

#include "xlink_port.h"

#include <stdlib.h>

static inline void *xlink_stdlib_malloc(size_t size)
{
    return malloc(size);
}

static inline void xlink_stdlib_free(void *ptr)
{
    free(ptr);
}

#endif // XLINK_PORT_STDLIB_H

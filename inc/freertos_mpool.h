#ifndef FREERTOS_MPOOL_H
#define FREERTOS_MPOOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <FreeRTOS.h>

typedef struct
{
    void *pool;
    uint8_t *markers;
    uint32_t pool_sz;
    uint32_t item_sz;
    uint32_t currentIndex;
} os_pool_t, *os_pool_p;

os_pool_p osPoolInit(void *pool,
                     uint8_t *markers,
                     uint32_t pool_sz,
                     uint32_t item_sz);

void *osPoolAlloc(os_pool_p pool_id);

void osPoolFree(os_pool_p pool_id, void *block);

#endif /* FREERTOS_MPOOL_H */

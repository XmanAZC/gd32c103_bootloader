#include "freertos_mpool.h"

os_pool_p osPoolInit(void *pool,
                     uint8_t *markers,
                     uint32_t pool_sz,
                     uint32_t item_sz)
{
    os_pool_p pool_id = pvPortMalloc(sizeof(os_pool_t));

    if (!pool_id)
    {
        return NULL;
    }

    if (!pool)
    {
        pool = pvPortMalloc(item_sz * pool_sz);
        if (!pool)
        {
            return NULL;
        }
        memset(pool, 0, item_sz * pool_sz);
    }

    if (!markers)
    {
        markers = pvPortMalloc(pool_sz);
        if (!markers)
        {
            return NULL;
        }
    }

    pool_id->pool = pool;
    pool_id->markers = markers;
    pool_id->pool_sz = pool_sz;
    pool_id->item_sz = item_sz;
    memset(markers, 0, pool_sz);
    return pool_id;
}

void *osPoolAlloc(os_pool_p pool_id)
{
    void *p = NULL;
    size_t i;
    size_t index;

    for (i = 0; i < pool_id->pool_sz; i++)
    {
        index = (pool_id->currentIndex + i) % pool_id->pool_sz;

        if (pool_id->markers[index] == 0)
        {
            portENTER_CRITICAL();
            pool_id->markers[index] = 1;
            p = (void *)((size_t)(pool_id->pool) + (index * pool_id->item_sz));
            pool_id->currentIndex = index;
            portEXIT_CRITICAL();
            break;
        }
    }

    return p;
}

void osPoolFree(os_pool_p pool_id, void *block)
{
    size_t index;

    if (block < pool_id->pool)
        goto __exit;

    index = (size_t)block - (size_t)(pool_id->pool);
    if (index % pool_id->item_sz)
        goto __exit;

    index = index / pool_id->item_sz;
    if (index >= pool_id->pool_sz)
        goto __exit;

    portENTER_CRITICAL();
    pool_id->markers[index] = 0;
    portEXIT_CRITICAL();

__exit:
    return;
}

#ifndef XLINK_PORT_H
#define XLINK_PORT_H

#include <stddef.h>
#include <stdint.h>


typedef void *(*xlink_mutex_create_t)(void);
typedef void (*xlink_mutex_delete_t)(void *mutex);
typedef int (*xlink_mutex_lock_t)(void *mutex);
typedef void (*xlink_mutex_unlock_t)(void *mutex);

typedef void *(*xlink_malloc_t)(size_t size);
typedef void (*xlink_free_t)(void *ptr);

typedef struct xlink_frame_def
{
    uint8_t *buffer;
    size_t size;
    struct dma_element *next, *prev;
} xlink_frame_t;

typedef xlink_frame_t *(*xlink_frame_send_alloc_t)(void *transport_handle, uint16_t needed);
typedef int (*xlink_transport_send_t)(void *transport_handle, xlink_frame_t *frame);

typedef struct xlink_port_api_def
{
    xlink_malloc_t malloc_fn;
    xlink_free_t free_fn;
    xlink_mutex_create_t mutex_create_fn;
    xlink_mutex_delete_t mutex_delete_fn;
    xlink_mutex_lock_t mutex_lock_fn;
    xlink_mutex_unlock_t mutex_unlock_fn;
    xlink_transport_send_t transport_send_fn;
    xlink_frame_send_alloc_t frame_send_alloc_fn;
} xlink_port_api_t;

static inline int xlink_port_mutex_lock(const xlink_port_api_t *api, void *mutex)
{
    if (api == NULL || api->mutex_lock_fn == NULL || mutex == NULL)
    {
        return 0;
    }
    return api->mutex_lock_fn(mutex);
}

static inline void xlink_port_mutex_unlock(const xlink_port_api_t *api, void *mutex)
{
    if (api == NULL || api->mutex_unlock_fn == NULL || mutex == NULL)
    {
        return;
    }
    api->mutex_unlock_fn(mutex);
}

#endif // XLINK_PORT_H

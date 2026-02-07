#ifndef XLINK_PORT_FREERTOS_H
#define XLINK_PORT_FREERTOS_H

#include "xlink_port.h"

#include <FreeRTOS.h>
#include <semphr.h>

static inline void *xlink_freertos_malloc(size_t size)
{
    return pvPortMalloc(size);
}

static inline void xlink_freertos_free(void *ptr)
{
    vPortFree(ptr);
}

static inline void *xlink_freertos_mutex_create(void)
{
    return (void *)xSemaphoreCreateMutex();
}

static inline void xlink_freertos_mutex_delete(void *mutex)
{
    if (mutex == NULL)
    {
        return;
    }
    vSemaphoreDelete((SemaphoreHandle_t)mutex);
}

static inline int xlink_freertos_mutex_lock(void *mutex)
{
    if (mutex == NULL)
    {
        return 0;
    }
    return (xSemaphoreTake((SemaphoreHandle_t)mutex, portMAX_DELAY) == pdTRUE) ? 0 : -1;
}

static inline void xlink_freertos_mutex_unlock(void *mutex)
{
    if (mutex == NULL)
    {
        return;
    }
    (void)xSemaphoreGive((SemaphoreHandle_t)mutex);
}

#endif // XLINK_PORT_FREERTOS_H

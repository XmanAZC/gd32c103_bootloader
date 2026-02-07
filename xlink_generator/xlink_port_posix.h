#ifndef XLINK_PORT_POSIX_H
#define XLINK_PORT_POSIX_H

#include "xlink_port.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/socket.h>

static inline void *xlink_posix_mutex_create(void)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (mutex == NULL)
    {
        return NULL;
    }
    if (pthread_mutex_init(mutex, NULL) != 0)
    {
        free(mutex);
        return NULL;
    }
    return (void *)mutex;
}

static inline void xlink_posix_mutex_delete(void *mutex)
{
    if (mutex == NULL)
    {
        return;
    }
    pthread_mutex_destroy((pthread_mutex_t *)mutex);
    free(mutex);
}

static inline int xlink_posix_mutex_lock(void *mutex)
{
    if (mutex == NULL)
    {
        return 0;
    }
    return (pthread_mutex_lock((pthread_mutex_t *)mutex) == 0) ? 0 : -1;
}

static inline void xlink_posix_mutex_unlock(void *mutex)
{
    if (mutex == NULL)
    {
        return;
    }
    (void)pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

static inline int xlink_posix_transport_send_write(void *transport_handle, const uint8_t *data, uint16_t len)
{
    int fd = (int)(intptr_t)transport_handle;
    ssize_t written = write(fd, data, (size_t)len);
    return (written == (ssize_t)len) ? 0 : -1;
}

static inline int xlink_posix_transport_send_send(void *transport_handle, const uint8_t *data, uint16_t len)
{
    int fd = (int)(intptr_t)transport_handle;
    ssize_t sent = send(fd, data, (size_t)len, 0);
    if (sent == -1 && errno == ENOTSOCK)
    {
        sent = write(fd, data, (size_t)len);
    }
    return (sent == (ssize_t)len) ? 0 : -1;
}

#endif // XLINK_PORT_POSIX_H

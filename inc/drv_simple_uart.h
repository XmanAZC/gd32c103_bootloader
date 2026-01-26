#ifndef __DRV_SIMPLE_USART_H__
#define __DRV_SIMPLE_USART_H__

#include <stdint.h>
#include <stddef.h>
#include "config.h"
#include "gd32c10x.h"
#include "drv_dma.h"

#ifdef __cplusplus
extern "C"
{
#endif

    struct dma_element
    {
        uint8_t *buffer;
        size_t size;
        struct dma_element *next, *prev;
    };

    int uart_init(void);

    /**
     * @description: 获取 UART 句柄
     * @param {const char} *name，UART 设备名称
     * @return {void*} 返回 UART 句柄，失败返回 RT_NULL
     */
    void *gd32_uart_get_handle(const char *name);

    struct dma_element * get_rx_block(void *handle);

    void free_rx_block(void *handle, struct dma_element *element);

    /**
     * @description: 注册接收回调函数
     * @param {void} *handle，UART 句柄
     * @param {size_t} size，接收数据大小
     * @param {rx_ind} 接收回调函数
     * @param {void} *userdata，用户数据
     * @return {int} 返回结果，RT_EOK 成功，其他失败
     */
    int gd32_uart_set_rx_indicate(void *handle, int (*rx_ind)(size_t size, void *userdata), void *userdata);

    /**
     * @description: 通过 DMA 发送数据
     * @param {void} *handle，UART 句柄
     * @param {const uint8_t} *buf，发送数据缓冲区
     * @param {size_t} size，发送数据大小
     * @return {int} 返回结果，RT_EOK 成功，其他失败
     */
    int gd32_uart_dma_send(void *handle, const uint8_t *buf, size_t size);

    struct dma_element *gd32_uart_alloc_dma_element(void *handle, size_t size);

    int gd32_uart_append_dma_send_list(void *handle, struct dma_element *element);

    void gd32_uart_set_baudrate(void *handle, uint32_t baudrate);

#ifdef __cplusplus
}
#endif

#endif /* __DRV_SIMPLE_USART_H__ */

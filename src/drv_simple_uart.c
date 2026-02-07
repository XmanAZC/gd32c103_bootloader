#include "drv_simple_uart.h"
#include <utlist.h>
#include <FreeRTOS.h>
#include "freertos_mpool.h"
#include <semphr.h>
#include <string.h>

#ifdef BSP_USING_SIMPLE_UART

#define TX_DMA_DATA_MAX_SIZE (270)
#define TX_DMA_DATA_MAX_CNT (10)

struct gd32_uart
{
    char *device_name;
    uint32_t periph;
    IRQn_Type irqn;
    rcu_periph_enum per_clk;
    rcu_periph_enum tx_gpio_clk;
    rcu_periph_enum rx_gpio_clk;
    uint32_t tx_port;
    uint16_t tx_pin;
    uint32_t rx_port;
    uint16_t rx_pin;
    uint32_t baudrate;
    struct dma_element *tx_dma_list;
    struct dma_element *rx_block_list;
    struct dma_element *current_rx_block;
    volatile uint32_t tx_dma_state; // 0:stop 1:running
    int (*rx_indicate)(size_t size, void *userdata);
    void *userdata;

    os_pool_p tx_dma_element_pool;
    os_pool_p tx_dma_data_pool;

    os_pool_p rx_block_pool;
    os_pool_p rx_data_pool;

    struct
    {
        struct dma_config rx;
        struct dma_config tx;
        size_t last_index;
        SemaphoreHandle_t sem_ftf;
    } dma;
};

enum
{
#ifdef BSP_USING_UART0
    UART0_INDEX,
#endif
#ifdef BSP_USING_UART1
    UART1_INDEX,
#endif
#ifdef BSP_USING_UART2
    UART2_INDEX,
#endif
#ifdef BSP_USING_UART3
    UART3_INDEX,
#endif
};

static struct gd32_uart uart_obj[] = {
#ifdef BSP_USING_UART0
    {
        "uart0",
        USART0,      // uart peripheral index
        USART0_IRQn, // uart iqrn
        RCU_USART0,
        RCU_GPIOA,
        RCU_GPIOA, // periph clock, tx gpio clock, rt gpio clock
        GPIOA,
        GPIO_PIN_9, // tx port, tx pin
        GPIOA,
        GPIO_PIN_10,        // rx port, rx pin
        BSP_UART0_BAUDRATE, // default baudrate
        NULL,               // tx_dma_list
        NULL,               // rx_block_list
        NULL,               // current_rx_block
        0,                  // tx_dma_state
        NULL,               // rx_indicate
        NULL,               // userdata
        .dma.rx = DRV_DMA_CONFIG(0, 4),
        .dma.tx = DRV_DMA_CONFIG(0, 3),
    },
#endif
#ifdef BSP_USING_UART1
    {
        "uart1",
        USART1,      // uart peripheral index
        USART1_IRQn, // uart iqrn
        RCU_USART1,
        RCU_GPIOA,
        RCU_GPIOA, // periph clock, tx gpio clock, rt gpio clock
        GPIOA,
        GPIO_PIN_2, // tx port, tx pin
        GPIOA,
        GPIO_PIN_3,         // rx port, rx pin
        BSP_UART1_BAUDRATE, // default baudrate
        NULL,               // tx_dma_list
        NULL,               // rx_block_list
        NULL,               // current_rx_block
        0,                  // tx_dma_state
        NULL,               // rx_indicate
        NULL,               // userdata
        .dma.rx = DRV_DMA_CONFIG(0, 5),
        .dma.tx = DRV_DMA_CONFIG(0, 6),
    },
#endif
#ifdef BSP_USING_UART2
    {
        "uart2",
        USART2,      // uart peripheral index
        USART2_IRQn, // uart iqrn
        RCU_USART2,
#ifdef SOC_SERIES_GD32F30x
        RCU_GPIOD,
        RCU_GPIOD, // periph clock, tx gpio clock, rt gpio clock
        GPIOD,
        GPIO_PIN_8, // tx port, tx pin
        GPIOD,
        GPIO_PIN_9, // rx port, rx pin
#else
        RCU_GPIOB,
        RCU_GPIOB, // periph clock, tx gpio clock, rt gpio clock
        GPIOB,
        GPIO_PIN_10, // tx port, tx pin
        GPIOB,
        GPIO_PIN_11, // rx port, rx pin
#endif
        BSP_UART2_BAUDRATE, // default baudrate
        NULL,               // tx_dma_list
        NULL,               // rx_block_list
        NULL,               // current_rx_block
        0,                  // tx_dma_state
        NULL,               // rx_indicate
        NULL,               // userdata
        .dma.rx = DRV_DMA_CONFIG(0, 2),
        .dma.tx = DRV_DMA_CONFIG(0, 1),
    },
#endif
#ifdef BSP_USING_UART3
    {
        "uart3",
        UART3,      // uart peripheral index
        UART3_IRQn, // uart iqrn
        RCU_UART3,
        RCU_GPIOC,
        RCU_GPIOC, // periph clock, tx gpio clock, rt gpio clock
        GPIOC,
        GPIO_PIN_10, // tx port, tx pin
        GPIOC,
        GPIO_PIN_11,        // rx port, rx pin
        BSP_UART3_BAUDRATE, // default baudrate
        NULL,               // tx_dma_list
        NULL,               // rx_block_list
        NULL,               // current_rx_block
        0,                  // tx_dma_state
        NULL,               // rx_indicate
        NULL,               // userdata
        .dma.rx = DRV_DMA_CONFIG(1, 2),
        .dma.tx = DRV_DMA_CONFIG(1, 4),
    },
#endif
};

static void _gd32_dma_receive(struct gd32_uart *uart, uint8_t *buffer, uint32_t size);

static inline size_t gd32_uart_buf_size(const struct gd32_uart *uart)
{
#ifdef BSP_USING_UART0
    if (uart == &uart_obj[UART0_INDEX])
        return BSP_UART0_RX_BUFSIZE;
#endif
#ifdef BSP_USING_UART1
    if (uart == &uart_obj[UART1_INDEX])
        return BSP_UART1_RX_BUFSIZE;
#endif
#ifdef BSP_USING_UART2
    if (uart == &uart_obj[UART2_INDEX])
        return BSP_UART2_RX_BUFSIZE;
#endif
#ifdef BSP_USING_UART3
    if (uart == &uart_obj[UART3_INDEX])
        return BSP_UART3_RX_BUFSIZE;
#endif
    return 512;
}

static inline void _uart_dma_transmit(struct gd32_uart *uart, const uint8_t *buffer, uint32_t size)
{
    /* Set the data length and data pointer */
    DMA_CHMADDR(uart->dma.tx.periph, uart->dma.tx.channel) = (uint32_t)buffer;
    DMA_CHCNT(uart->dma.tx.periph, uart->dma.tx.channel) = size;

    /* enable dma transmit */
#if defined(SOC_SERIES_GD32F30x) || defined(SOC_SERIES_GD32F10x)
    usart_dma_transmit_config(uart->periph, USART_DENT_ENABLE);
#else
    usart_dma_transmit_config(uart->periph, USART_TRANSMIT_DMA_ENABLE);
#endif

    /* enable dma channel */
    dma_channel_enable(uart->dma.tx.periph, uart->dma.tx.channel);
}

static void dma_recv_isr(struct gd32_uart *uart)
{
    size_t counter;
    counter = dma_transfer_number_get(uart->dma.rx.periph, uart->dma.rx.channel);
    uart->current_rx_block->size = gd32_uart_buf_size(uart) - counter;
    DL_APPEND(uart->rx_block_list, uart->current_rx_block);
    if (uart->rx_indicate)
    {
        uart->rx_indicate(uart->rx_block_list->size, uart->userdata);
    }
_back:
    uart->current_rx_block = osPoolAlloc(uart->rx_block_pool);
    if (uart->current_rx_block)
    {
        uart->current_rx_block->prev = uart->current_rx_block->next = NULL;
        uart->current_rx_block->buffer = osPoolAlloc(uart->rx_data_pool);
        if (!uart->current_rx_block->buffer)
        {
            osPoolFree(uart->rx_block_pool, uart->current_rx_block);
            uart->current_rx_block = NULL;
            return;
        }
        else
        {
            _gd32_dma_receive(uart,
                              uart->current_rx_block->buffer,
                              gd32_uart_buf_size(uart));
        }
    }
    else
    {
        /* no memory */
        /* free list head */
        struct dma_element *node = uart->rx_block_list;
        if (node)
        {
            DL_DELETE(uart->rx_block_list, node);
            osPoolFree(uart->rx_data_pool, node->buffer);
            osPoolFree(uart->rx_block_pool, node);
        }
        goto _back;
    }
}

struct dma_element *get_rx_block(void *handle)
{
    struct gd32_uart *uart = (struct gd32_uart *)handle;
    taskENTER_CRITICAL();
    struct dma_element *node = uart->rx_block_list;
    if (node)
    {
        DL_DELETE(uart->rx_block_list, node);
        taskEXIT_CRITICAL();
        return node;
    }
    taskEXIT_CRITICAL();
    return NULL;
}

void free_rx_block(void *handle, struct dma_element *element)
{
    struct gd32_uart *uart = (struct gd32_uart *)handle;
    taskENTER_CRITICAL();
    osPoolFree(uart->rx_data_pool, element->buffer);
    osPoolFree(uart->rx_block_pool, element);
    taskEXIT_CRITICAL();
}

static void gd32_uart_isr(struct gd32_uart *uart)
{

    if (usart_interrupt_flag_get(uart->periph, USART_INT_FLAG_IDLE) != RESET)
    {
        volatile uint8_t data = (uint8_t)usart_data_receive(uart->periph);
        /* clear all the interrupt flags */
        dma_flag_clear(uart->dma.rx.periph, uart->dma.rx.channel, DMA_FLAG_G);
        dma_flag_clear(uart->dma.rx.periph, uart->dma.rx.channel, DMA_FLAG_ERR);
        dma_flag_clear(uart->dma.rx.periph, uart->dma.rx.channel, DMA_FLAG_HTF);
        dma_flag_clear(uart->dma.rx.periph, uart->dma.rx.channel, DMA_FLAG_FTF);
        dma_channel_disable(uart->dma.rx.periph, uart->dma.rx.channel);
        dma_deinit(uart->dma.rx.periph, uart->dma.rx.channel);
        dma_recv_isr(uart);

        usart_interrupt_flag_clear(uart->periph, USART_INT_FLAG_IDLE);
    }
    else
    {
        if (usart_interrupt_flag_get(uart->periph, USART_INT_FLAG_ERR_ORERR) != RESET)
        {
            usart_interrupt_flag_clear(uart->periph, USART_INT_FLAG_ERR_ORERR);
        }

        if (usart_interrupt_flag_get(uart->periph, USART_INT_FLAG_ERR_NERR) != RESET)
        {
            usart_interrupt_flag_clear(uart->periph, USART_INT_FLAG_ERR_NERR);
        }

        if (usart_interrupt_flag_get(uart->periph, USART_INT_FLAG_ERR_FERR) != RESET)
        {
            usart_interrupt_flag_clear(uart->periph, USART_INT_FLAG_ERR_FERR);
        }

        if (usart_interrupt_flag_get(uart->periph, USART_INT_FLAG_RBNE_ORERR) != RESET)
        {
            usart_interrupt_flag_clear(uart->periph, USART_INT_FLAG_RBNE_ORERR);
        }

        if (usart_interrupt_flag_get(uart->periph, USART_INT_FLAG_PERR) != RESET)
        {
            usart_interrupt_flag_clear(uart->periph, USART_INT_FLAG_PERR);
        }

        if (usart_interrupt_flag_get(uart->periph, USART_INT_FLAG_CTS) != RESET)
        {
            usart_interrupt_flag_clear(uart->periph, USART_INT_FLAG_CTS);
        }

        if (usart_interrupt_flag_get(uart->periph, USART_INT_FLAG_LBD) != RESET)
        {
            usart_interrupt_flag_clear(uart->periph, USART_INT_FLAG_LBD);
        }

        if (usart_interrupt_flag_get(uart->periph, USART_INT_FLAG_EB) != RESET)
        {
            usart_interrupt_flag_clear(uart->periph, USART_INT_FLAG_EB);
        }

        if (usart_interrupt_flag_get(uart->periph, USART_INT_FLAG_RT) != RESET)
        {
            usart_interrupt_flag_clear(uart->periph, USART_INT_FLAG_RT);
        }
    }
}

static void dma_tx_isr(struct gd32_uart *uart)
{
    if (dma_interrupt_flag_get(uart->dma.tx.periph, uart->dma.tx.channel, DMA_INT_FLAG_FTF) != RESET)
    {
        dma_interrupt_flag_clear(uart->dma.tx.periph, uart->dma.tx.channel, DMA_INT_FLAG_FTF);

        /* disable dma tx channel */
        dma_channel_disable(uart->dma.tx.periph, uart->dma.tx.channel);

        taskENTER_CRITICAL();
        struct dma_element *node = uart->tx_dma_list;
        if (node)
        {
            DL_DELETE(uart->tx_dma_list, node);
            osPoolFree(uart->tx_dma_data_pool, node->buffer);
            osPoolFree(uart->tx_dma_element_pool, node);
        }
        if (uart->tx_dma_list)
        {
            _uart_dma_transmit(uart, uart->tx_dma_list->buffer, uart->tx_dma_list->size);
        }
        else
        {
            uart->tx_dma_state = 0; // stop
        }
        taskEXIT_CRITICAL();
    }
}

#if defined(BSP_USING_UART0)
void USART0_IRQHandler(void)
{
    taskENTER_CRITICAL();
    gd32_uart_isr(&uart_obj[UART0_INDEX]);
    taskEXIT_CRITICAL();
}
#endif /* BSP_USING_UART0 */

#if defined(BSP_USING_UART1)
void USART1_IRQHandler(void)
{
    taskENTER_CRITICAL();
    gd32_uart_isr(&uart_obj[UART1_INDEX]);
    taskEXIT_CRITICAL();
}
#endif /* BSP_USING_UART1 */

#if defined(BSP_USING_UART2)
void USART2_IRQHandler(void)
{
    taskENTER_CRITICAL();
    gd32_uart_isr(&uart_obj[UART2_INDEX]);
    taskEXIT_CRITICAL();
}
#endif /* BSP_USING_UART2 */

#if defined(BSP_USING_UART3)
void UART3_IRQHandler(void)
{
    taskENTER_CRITICAL();
    gd32_uart_isr(&uart_obj[UART3_INDEX]);
    taskEXIT_CRITICAL();
}
#endif /* BSP_USING_UART3 */

#ifdef BSP_UART0_TX_USING_DMA
void DMA0_Channel3_IRQHandler(void)
{
    dma_tx_isr(&uart_obj[UART0_INDEX]);
}
#endif

#ifdef BSP_UART1_TX_USING_DMA
void DMA0_Channel6_IRQHandler(void)
{
    dma_tx_isr(&uart_obj[UART1_INDEX]);
}
#endif

#ifdef BSP_UART2_TX_USING_DMA
void DMA0_Channel1_IRQHandler(void)
{
    dma_tx_isr(&uart_obj[UART2_INDEX]);
}
#endif

#ifdef BSP_UART3_TX_USING_DMA
#ifdef SOC_SERIES_GD32F30x
void DMA1_Channel3_4_IRQHandler(void)
#else
void DMA1_Channel4_IRQHandler(void)
#endif
{
    dma_tx_isr(&uart_obj[UART3_INDEX]);
}
#endif

static void _gd32_dma_receive(struct gd32_uart *uart, uint8_t *buffer, uint32_t size)
{
    static dma_parameter_struct dma_init_struct;
    dma_struct_para_init(&dma_init_struct);

    /* configure receive DMA */
    rcu_periph_clock_enable(uart->dma.rx.rcu);
    dma_deinit(uart->dma.rx.periph, uart->dma.rx.channel);

    dma_init_struct.number = size;
    dma_init_struct.memory_addr = (uint32_t)buffer;
    dma_init_struct.periph_addr = (uint32_t)&USART_DATA(uart->periph);
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.direction = DMA_PERIPHERAL_TO_MEMORY;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;
    dma_init(uart->dma.rx.periph, uart->dma.rx.channel, &dma_init_struct);
    dma_circulation_enable(uart->dma.rx.periph, uart->dma.rx.channel);

    /* enable dma channel */
    dma_channel_enable(uart->dma.rx.periph, uart->dma.rx.channel);

    /* enable usart idle interrupt */
    usart_interrupt_enable(uart->periph, USART_INT_IDLE);

    /* enable dma receive */
    usart_dma_receive_config(uart->periph, USART_RECEIVE_DMA_ENABLE);
}

static void gd32_dma_config(struct gd32_uart *uart)
{
    dma_parameter_struct dma_init_struct = {0};

    dma_flag_clear(uart->dma.tx.periph, uart->dma.tx.channel, DMA_FLAG_G);
    dma_flag_clear(uart->dma.tx.periph, uart->dma.tx.channel, DMA_FLAG_ERR);
    dma_flag_clear(uart->dma.tx.periph, uart->dma.tx.channel, DMA_FLAG_HTF);
    dma_flag_clear(uart->dma.tx.periph, uart->dma.tx.channel, DMA_FLAG_FTF);
    dma_channel_disable(uart->dma.tx.periph, uart->dma.tx.channel);
    dma_deinit(uart->dma.tx.periph, uart->dma.tx.channel);

    rcu_periph_clock_enable(uart->dma.tx.rcu);
    dma_deinit(uart->dma.tx.periph, uart->dma.tx.channel);

    dma_init_struct.periph_addr = (uint32_t)&USART_DATA(uart->periph);
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.direction = DMA_MEMORY_TO_PERIPHERAL;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;
    dma_init(uart->dma.tx.periph, uart->dma.tx.channel, &dma_init_struct);
    dma_circulation_disable(uart->dma.tx.periph, uart->dma.tx.channel);

    /* enable tx dma interrupt */
    NVIC_SetPriority(uart->dma.tx.irq, configMAX_SYSCALL_INTERRUPT_PRIORITY + 1);
    NVIC_EnableIRQ(uart->dma.tx.irq);

    /* enable transmit complete interrupt */
    dma_interrupt_enable(uart->dma.tx.periph, uart->dma.tx.channel, DMA_CHXCTL_FTFIE);

    /* clear all the interrupt flags */
    dma_flag_clear(uart->dma.rx.periph, uart->dma.rx.channel, DMA_FLAG_G);
    dma_flag_clear(uart->dma.rx.periph, uart->dma.rx.channel, DMA_FLAG_ERR);
    dma_flag_clear(uart->dma.rx.periph, uart->dma.rx.channel, DMA_FLAG_HTF);
    dma_flag_clear(uart->dma.rx.periph, uart->dma.rx.channel, DMA_FLAG_FTF);
    dma_channel_disable(uart->dma.rx.periph, uart->dma.rx.channel);
    dma_deinit(uart->dma.rx.periph, uart->dma.rx.channel);

    uart->current_rx_block = osPoolAlloc(uart->rx_block_pool);
    if (uart->current_rx_block)
    {
        uart->current_rx_block->prev = uart->current_rx_block->next = NULL;
        uart->current_rx_block->buffer = osPoolAlloc(uart->rx_data_pool);
        if (!uart->current_rx_block->buffer)
        {
            osPoolFree(uart->rx_block_pool, uart->current_rx_block);
            uart->current_rx_block = NULL;
            return;
        }
        else
        {
            _gd32_dma_receive(uart,
                              uart->current_rx_block->buffer,
                              gd32_uart_buf_size(uart));
        }
    }
}

static void inline gd32_uart_init(struct gd32_uart *uart)
{
    /* enable USART clock */
    rcu_periph_clock_enable(uart->tx_gpio_clk);
    rcu_periph_clock_enable(uart->rx_gpio_clk);
    rcu_periph_clock_enable(uart->per_clk);
    /* connect port to USARTx_Tx */
    gpio_init(uart->tx_port, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, uart->tx_pin);

    /* connect port to USARTx_Rx */
    gpio_init(uart->rx_port, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, uart->rx_pin);

    NVIC_SetPriority(uart->irqn, configMAX_SYSCALL_INTERRUPT_PRIORITY + 1);
    NVIC_EnableIRQ(uart->irqn);

    usart_baudrate_set(uart->periph, uart->baudrate);
    usart_word_length_set(uart->periph, USART_WL_8BIT);
    usart_stop_bit_set(uart->periph, USART_STB_1BIT);
    usart_parity_config(uart->periph, USART_PM_NONE);
    usart_receive_config(uart->periph, USART_RECEIVE_ENABLE);
    usart_transmit_config(uart->periph, USART_TRANSMIT_ENABLE);
    usart_enable(uart->periph);

    gd32_dma_config(uart);
}

void gd32_uart_set_baudrate(void *handle, uint32_t baudrate)
{
    struct gd32_uart *uart = (struct gd32_uart *)handle;

    if (uart == NULL)
        return;

    uart->baudrate = baudrate;
    usart_baudrate_set(uart->periph, uart->baudrate);
}

int uart_init(void)
{
    int i;
    for (i = 0; i < sizeof(uart_obj) / sizeof(uart_obj[0]); i++)
    {
        uart_obj[i].tx_dma_element_pool = osPoolInit(NULL,
                                                     NULL,
                                                     TX_DMA_DATA_MAX_CNT,
                                                     sizeof(struct dma_element));
        uart_obj[i].tx_dma_data_pool = osPoolInit(NULL,
                                                  NULL,
                                                  TX_DMA_DATA_MAX_CNT,
                                                  TX_DMA_DATA_MAX_SIZE);

        uart_obj[i].rx_block_pool = osPoolInit(NULL,
                                               NULL,
                                               TX_DMA_DATA_MAX_CNT,
                                               sizeof(struct dma_element));
        uart_obj[i].rx_data_pool = osPoolInit(NULL,
                                              NULL,
                                              TX_DMA_DATA_MAX_CNT,
                                              gd32_uart_buf_size(&uart_obj[i]));
        gd32_uart_init(&uart_obj[i]);
    }

    return 0;
}

static int simple_strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

void *gd32_uart_get_handle(const char *name)
{
    int i;

    for (i = 0; i < sizeof(uart_obj) / sizeof(uart_obj[0]); i++)
    {
        if (simple_strcmp(uart_obj[i].device_name, name) == 0)
        {
            return &uart_obj[i];
        }
    }

    return NULL;
}

int gd32_uart_set_rx_indicate(void *handle, int (*rx_ind)(size_t size, void *userdata), void *userdata)
{
    struct gd32_uart *uart = (struct gd32_uart *)handle;

    if (uart == NULL)
        return -1;

    uart->rx_indicate = rx_ind;
    uart->userdata = userdata;

    return 0;
}

int gd32_uart_dma_send(void *handle, const uint8_t *buf, size_t size)
{
    struct gd32_uart *uart = (struct gd32_uart *)handle;
    struct dma_element *node;

    if ((uart == NULL) || (buf == NULL) || (size == 0))
        return -1;

    node = osPoolAlloc(uart->tx_dma_element_pool);
    if (node == NULL)
    {
        return -1;
    }

    node->buffer = osPoolAlloc(uart->tx_dma_data_pool);
    if (node->buffer == NULL)
    {
        osPoolFree(uart->tx_dma_element_pool, node);
        return -1;
    }

    memcpy(node->buffer, buf, size);
    node->size = size;
    node->next = NULL;
    node->prev = NULL;
    taskENTER_CRITICAL();
    DL_APPEND(uart->tx_dma_list, node);

    if (uart->tx_dma_state == 0) // stop
    {
        uart->tx_dma_state = 1; // running
        _uart_dma_transmit(uart, uart->tx_dma_list->buffer, uart->tx_dma_list->size);
    }
    taskEXIT_CRITICAL();
    return 0;
}

int gd32_uart_append_dma_send_list(void *handle, struct dma_element *element)
{
    struct gd32_uart *uart = (struct gd32_uart *)handle;

    if ((uart == NULL) || (element == NULL))
        return -1;

    taskENTER_CRITICAL();
    DL_APPEND(uart->tx_dma_list, element);

    if (uart->tx_dma_state == 0) // stop
    {
        uart->tx_dma_state = 1; // running
        _uart_dma_transmit(uart, uart->tx_dma_list->buffer, uart->tx_dma_list->size);
    }
    taskEXIT_CRITICAL();
    return 0;
}

struct dma_element *gd32_uart_alloc_dma_element(void *handle, size_t size)
{
    struct dma_element *element;
    struct gd32_uart *uart = (struct gd32_uart *)handle;

    if (size == 0 || size > TX_DMA_DATA_MAX_SIZE)
        return NULL;

    element = osPoolAlloc(uart->tx_dma_element_pool);
    if (element)
    {
        element->buffer = osPoolAlloc(uart->tx_dma_data_pool);
        if (element->buffer == NULL)
        {
            osPoolFree(uart->tx_dma_element_pool, element);
            element = NULL;
        }
        else
        {
            element->size = size;
            element->next = NULL;
            element->prev = NULL;
        }
    }

    return element;
}

#endif /* BSP_USING_SIMPLE_UART */

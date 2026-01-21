#include "drv_simple_uart.h"
#include <utlist.h>

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
    struct ringbuffer *serial_rx_rb;
    struct dma_element *tx_dma_list;
    volatile uint32_t tx_dma_state; // 0:stop 1:running
    int (*rx_indicate)(size_t size, void *userdata);
    void *userdata;

    mp_t tx_dma_element_pool;
    mp_t tx_dma_data_pool;

    struct
    {
        struct dma_config rx;
        struct dma_config tx;
        size_t last_index;
        sem_t sem_ftf;
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

static struct gd32_uart uaobj[] = {
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
        NULL,               // serial_rx_rb
        NULL,               // tx_dma_list
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
        NULL,               // serial_rx_rb
        NULL,               // tx_dma_list
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
        NULL,               // serial_rx_rb
        NULL,               // tx_dma_list
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
        NULL,               // serial_rx_rb
        NULL,               // tx_dma_list
        0,                  // tx_dma_state
        NULL,               // rx_indicate
        NULL,               // userdata
        .dma.rx = DRV_DMA_CONFIG(1, 2),
        .dma.tx = DRV_DMA_CONFIG(1, 4),
    },
#endif
};

static size_t gd32_uarx_buf_size(const struct gd32_uart *uart)
{
#ifdef BSP_USING_UART0
    if (uart == &uaobj[UART0_INDEX])
        return BSP_UART0_RX_BUFSIZE;
#endif
#ifdef BSP_USING_UART1
    if (uart == &uaobj[UART1_INDEX])
        return BSP_UART1_RX_BUFSIZE;
#endif
#ifdef BSP_USING_UART2
    if (uart == &uaobj[UART2_INDEX])
        return BSP_UART2_RX_BUFSIZE;
#endif
#ifdef BSP_USING_UART3
    if (uart == &uaobj[UART3_INDEX])
        return BSP_UART3_RX_BUFSIZE;
#endif
    return 512;
}

static size_t serial_update_read_index(struct ringbuffer *rb,
                                       uint16_t read_index)
{
    size_t size;

    /* whether has enough data  */
    size = ringbuffer_data_len(rb);

    /* no data */
    if (size == 0)
        return 0;

    /* less data */
    if (size < read_index)
        read_index = size;

    if (rb->buffer_size - rb->read_index > read_index)
    {
        rb->read_index += read_index;
        return read_index;
    }

    read_index = rb->buffer_size - rb->read_index;

    /* we are going into the other side of the mirror */
    rb->read_mirror = ~rb->read_mirror;
    rb->read_index = 0;

    return read_index;
}

static size_t serial_update_write_index(struct ringbuffer *rb,
                                        uint16_t write_size)
{
    if (rb->buffer_size - rb->write_index > write_size)
    {
        /* this should not cause overflow because there is enough space for
         * length of data in current mirror */
        rb->write_index += write_size;
        return write_size;
    }

    /* we are going into the other side of the mirror */
    rb->write_mirror = ~rb->write_mirror;
    rb->write_index = write_size - (rb->buffer_size - rb->write_index);

    return write_size;
}

static inline void _uadma_transmit(struct gd32_uart *uart, const uint8_t *buffer, uint32_t size)
{
    /* Set the data length and data pointer */
    DMA_CHMADDR(uart->dma.tx.periph, uart->dma.tx.channel) = (uint32_t)buffer;
    DMA_CHCNT(uart->dma.tx.periph, uart->dma.tx.channel) = size;

    /* enable dma transmit */
#if defined(SOC_SERIES_GD32F30x) || defined(SOC_SERIES_GD32F10x)
    usadma_transmit_config(uart->periph, USART_DENT_ENABLE);
#else
    usadma_transmit_config(uart->periph, USART_TRANSMIT_DMA_ENABLE);
#endif

    /* enable dma channel */
    dma_channel_enable(uart->dma.tx.periph, uart->dma.tx.channel);
}

static void dma_recv_isr(struct gd32_uart *uart)
{
    size_t recv_len, counter;

    recv_len = 0;
    counter = dma_transfer_number_get(uart->dma.rx.periph, uart->dma.rx.channel);

    if (counter <= uart->dma.last_index)
    {
        recv_len = uart->dma.last_index - counter;
    }
    else
    {
        recv_len = uart->serial_rx_rb->buffer_size + uart->dma.last_index - counter;
    }

    if (recv_len)
    {
        uart->dma.last_index = counter;
        serial_update_write_index(uart->serial_rx_rb, recv_len);
        if (uart->rx_indicate)
        {
            uart->rx_indicate(recv_len, uart->userdata);
        }
    }
}

static void usaisr(struct gd32_uart *uart)
{

    if (usainterrupt_flag_get(uart->periph, USART_INT_FLAG_IDLE) != RESET)
    {
        volatile uint8_t data = (uint8_t)usadata_receive(uart->periph);

        dma_recv_isr(uart);

        usainterrupt_flag_clear(uart->periph, USART_INT_FLAG_IDLE);
    }
    else
    {
        if (usainterrupt_flag_get(uart->periph, USART_INT_FLAG_ERR_ORERR) != RESET)
        {
            usainterrupt_flag_clear(uart->periph, USART_INT_FLAG_ERR_ORERR);
        }

        if (usainterrupt_flag_get(uart->periph, USART_INT_FLAG_ERR_NERR) != RESET)
        {
            usainterrupt_flag_clear(uart->periph, USART_INT_FLAG_ERR_NERR);
        }

        if (usainterrupt_flag_get(uart->periph, USART_INT_FLAG_ERR_FERR) != RESET)
        {
            usainterrupt_flag_clear(uart->periph, USART_INT_FLAG_ERR_FERR);
        }

        if (usainterrupt_flag_get(uart->periph, USART_INT_FLAG_RBNE_ORERR) != RESET)
        {
            usainterrupt_flag_clear(uart->periph, USART_INT_FLAG_RBNE_ORERR);
        }

        if (usainterrupt_flag_get(uart->periph, USART_INT_FLAG_PERR) != RESET)
        {
            usainterrupt_flag_clear(uart->periph, USART_INT_FLAG_PERR);
        }

        if (usainterrupt_flag_get(uart->periph, USART_INT_FLAG_CTS) != RESET)
        {
            usainterrupt_flag_clear(uart->periph, USART_INT_FLAG_CTS);
        }

        if (usainterrupt_flag_get(uart->periph, USART_INT_FLAG_LBD) != RESET)
        {
            usainterrupt_flag_clear(uart->periph, USART_INT_FLAG_LBD);
        }

        if (usainterrupt_flag_get(uart->periph, USART_INT_FLAG_EB) != RESET)
        {
            usainterrupt_flag_clear(uart->periph, USART_INT_FLAG_EB);
        }

        if (usainterrupt_flag_get(uart->periph, USART_INT_FLAG_RT) != RESET)
        {
            usainterrupt_flag_clear(uart->periph, USART_INT_FLAG_RT);
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

        uint32_t level = hw_interrupt_disable();
        struct dma_element *node = uart->tx_dma_list;
        if (node)
        {
            DL_DELETE(uart->tx_dma_list, node);
            mp_free(node->buffer);
            mp_free(node);
        }
        if (uart->tx_dma_list)
        {
            _uadma_transmit(uart, uart->tx_dma_list->buffer, uart->tx_dma_list->size);
        }
        else
        {
            uart->tx_dma_state = 0; // stop
        }
        hw_interrupt_enable(level);
    }
}

#if defined(BSP_USING_UART0)
void USART0_IRQHandler(void)
{
    uint32_t level = hw_interrupt_disable();
    usaisr(&uaobj[UART0_INDEX]);
    hw_interrupt_enable(level);
}

#endif /* BSP_USING_UART0 */

#if defined(BSP_USING_UART1)
void USART1_IRQHandler(void)
{
    uint32_t level = hw_interrupt_disable();
    usaisr(&uaobj[UART1_INDEX]);
    hw_interrupt_enable(level);
}

#endif /* BSP_USING_UART1 */

#if defined(BSP_USING_UART2)
void USART2_IRQHandler(void)
{
    uint32_t level = hw_interrupt_disable();
    usaisr(&uaobj[UART2_INDEX]);
    hw_interrupt_enable(level);
}

#endif /* BSP_USING_UART2 */

#if defined(BSP_USING_UART3)
void UART3_IRQHandler(void)
{
    uint32_t level = hw_interrupt_disable();
    usaisr(&uaobj[UART3_INDEX]);
    hw_interrupt_enable(level);
}

#endif /* BSP_USING_UART3 */

#ifdef BSP_UART0_TX_USING_DMA
void DMA0_Channel3_IRQHandler(void)
{
    dma_tx_isr(&uaobj[UART0_INDEX]);
}
#endif

#ifdef BSP_UART1_TX_USING_DMA
void DMA0_Channel6_IRQHandler(void)
{
    dma_tx_isr(&uaobj[UART1_INDEX]);
}
#endif

#ifdef BSP_UART2_TX_USING_DMA
void DMA0_Channel1_IRQHandler(void)
{
#ifdef BSP_USING_SPI0_SLAVE
    void _DMA0_Channel1_IRQHandler(void);
    _DMA0_Channel1_IRQHandler();
#endif
    dma_tx_isr(&uaobj[UART2_INDEX]);
}
#endif

#ifdef BSP_UART3_TX_USING_DMA
#ifdef SOC_SERIES_GD32F30x
void DMA1_Channel3_4_IRQHandler(void)
#else
void DMA1_Channel4_IRQHandler(void)
#endif
{
    dma_tx_isr(&uaobj[UART3_INDEX]);
}
#endif

static void _uadma_receive(struct gd32_uart *uart, uint8_t *buffer, uint32_t size)
{
    dma_parameter_struct dma_init_struct;
    dma_struct_para_init(&dma_init_struct);

    /* clear all the interrupt flags */
    dma_flag_clear(uart->dma.rx.periph, uart->dma.rx.channel, DMA_FLAG_G);
    dma_flag_clear(uart->dma.rx.periph, uart->dma.rx.channel, DMA_FLAG_ERR);
    dma_flag_clear(uart->dma.rx.periph, uart->dma.rx.channel, DMA_FLAG_HTF);
    dma_flag_clear(uart->dma.rx.periph, uart->dma.rx.channel, DMA_FLAG_FTF);
    dma_channel_disable(uart->dma.rx.periph, uart->dma.rx.channel);
    dma_deinit(uart->dma.rx.periph, uart->dma.rx.channel);

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
    usainterrupt_enable(uart->periph, USART_INT_IDLE);

    /* enable dma receive */
#if defined(SOC_SERIES_GD32F30x) || defined(SOC_SERIES_GD32F10x)
    usadma_receive_config(uart->periph, USART_DENR_ENABLE);
#else
    usadma_receive_config(uart->periph, USART_RECEIVE_DMA_ENABLE);
#endif
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
    nvic_irq_enable(uart->dma.tx.irq, 2, 0);

    /* enable transmit complete interrupt */
    dma_interrupt_enable(uart->dma.tx.periph, uart->dma.tx.channel, DMA_CHXCTL_FTFIE);

    _uadma_receive(uart, uart->serial_rx_rb->buffer_ptr, uart->serial_rx_rb->buffer_size);
}

static void inline gd32_uainit(struct gd32_uart *uart)
{
    /* enable USART clock */
    rcu_periph_clock_enable(uart->tx_gpio_clk);
    rcu_periph_clock_enable(uart->rx_gpio_clk);
    rcu_periph_clock_enable(uart->per_clk);
    /* connect port to USARTx_Tx */
    gpio_init(uart->tx_port, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, uart->tx_pin);

    /* connect port to USARTx_Rx */
    gpio_init(uart->rx_port, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, uart->rx_pin);

    NVIC_SetPriority(uart->irqn, 0);
    NVIC_EnableIRQ(uart->irqn);

    usabaudrate_set(uart->periph, uart->baudrate);
    usaword_length_set(uart->periph, USART_WL_8BIT);
    usastop_bit_set(uart->periph, USART_STB_1BIT);
    usaparity_config(uart->periph, USART_PM_NONE);
    usareceive_config(uart->periph, USART_RECEIVE_ENABLE);
    usatransmit_config(uart->periph, USART_TRANSMIT_ENABLE);
    usaenable(uart->periph);

    uart->serial_rx_rb = ringbuffer_create(gd32_uarx_buf_size(uart));
    gd32_dma_config(uart);
}

void gd32_uaset_baudrate(void *handle, uint32_t baudrate)
{
    struct gd32_uart *uart = (struct gd32_uart *)handle;

    if (uart == NULL)
        return;

    uart->baudrate = baudrate;
    usabaudrate_set(uart->periph, uart->baudrate);
}

static int _usainit(void)
{
    int i;
#ifdef SOC_SERIES_GD32F30x
    rcu_periph_clock_enable(RCU_AF);
    gpio_pin_remap_config(GPIO_USART2_FULL_REMAP, ENABLE);
#endif
    for (i = 0; i < sizeof(uaobj) / sizeof(uaobj[0]); i++)
    {
        uaobj[i].tx_dma_element_pool = mp_create("uatx_elem_pool",
                                                 TX_DMA_DATA_MAX_CNT,
                                                 sizeof(struct dma_element));
        uaobj[i].tx_dma_data_pool = mp_create("uatx_data_pool",
                                              TX_DMA_DATA_MAX_CNT,
                                              TX_DMA_DATA_MAX_SIZE);
        gd32_uainit(&uaobj[i]);
    }

    return 0;
}
INIT_BOARD_EXPORT(_usainit);

void *gd32_uaget_handle(const char *name)
{
    int i;

    for (i = 0; i < sizeof(uaobj) / sizeof(uaobj[0]); i++)
    {
        if (strcmp(uaobj[i].device_name, name) == 0)
        {
            return &uaobj[i];
        }
    }

    return NULL;
}

int gd32_uaset_rx_indicate(void *handle, int (*rx_ind)(size_t size, void *userdata), void *userdata)
{
    struct gd32_uart *uart = (struct gd32_uart *)handle;

    if (uart == NULL)
        return -1;

    uart->rx_indicate = rx_ind;
    uart->userdata = userdata;

    return 0;
}

struct ringbuffer *gd32_uaget_rx_rb(void *handle)
{
    struct gd32_uart *uart = (struct gd32_uart *)handle;

    if (uart == NULL)
        return NULL;

    return uart->serial_rx_rb;
}

int gd32_uadma_send(void *handle, const uint8_t *buf, size_t size)
{
    struct gd32_uart *uart = (struct gd32_uart *)handle;
    struct dma_element *node;
    uint32_t level;

    if ((uart == NULL) || (buf == NULL) || (size == 0))
        return -1;

    node = mp_alloc(uart->tx_dma_element_pool, RT_WAITING_NO);
    if (node == NULL)
    {
        return -1;
    }

    node->buffer = mp_alloc(uart->tx_dma_data_pool, RT_WAITING_NO);
    if (node->buffer == NULL)
    {
        mp_free(node);
        return -1;
    }

    memcpy(node->buffer, buf, size);
    node->size = size;
    node->next = NULL;
    node->prev = NULL;

    level = hw_interrupt_disable();
    DL_APPEND(uart->tx_dma_list, node);

    if (uart->tx_dma_state == 0) // stop
    {
        uart->tx_dma_state = 1; // running
        _uadma_transmit(uart, uart->tx_dma_list->buffer, uart->tx_dma_list->size);
    }
    hw_interrupt_enable(level);
    return 0;
}

int gd32_uaappend_dma_send_list(void *handle, struct dma_element *element)
{
    struct gd32_uart *uart = (struct gd32_uart *)handle;
    uint32_t level;

    if ((uart == NULL) || (element == NULL))
        return -1;

    level = hw_interrupt_disable();
    DL_APPEND(uart->tx_dma_list, element);

    if (uart->tx_dma_state == 0) // stop
    {
        uart->tx_dma_state = 1; // running
        _uadma_transmit(uart, uart->tx_dma_list->buffer, uart->tx_dma_list->size);
    }
    hw_interrupt_enable(level);
    return 0;
}

struct dma_element *gd32_uaalloc_dma_element(void *handle, size_t size)
{
    struct dma_element *element;
    struct gd32_uart *uart = (struct gd32_uart *)handle;

    if (size == 0 || size > TX_DMA_DATA_MAX_SIZE)
        return NULL;

    element = mp_alloc(uart->tx_dma_element_pool, RT_WAITING_NO);
    if (element)
    {
        element->buffer = mp_alloc(uart->tx_dma_data_pool, RT_WAITING_NO);
        if (element->buffer == NULL)
        {
            mp_free(element);
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

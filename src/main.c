#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <timers.h>
#include <semphr.h>
#include <stdio.h>
#include "config.h"
#include "partition.h"
#include "gd32c10x.h"
#include "drv_simple_uart.h"
#include "freertos_mpool.h"
#include "xlink_upgrade.h"
#include "xlink_control.h"
#include "xlink_port_freertos.h"
#include "onchip_flash_port.h"

static void live_led_task(void *parameters) __attribute__((noreturn));

static void xlinkTask(void *parameters) __attribute__((noreturn));

void entry(void)
{
    static StaticTask_t xlinkTaskTCB;
    static StackType_t xlinkTaskStack[1024];
    (void)xTaskCreateStatic(xlinkTask,
                            "xlink",
                            1024,
                            NULL,
                            configMAX_PRIORITIES - 2U,
                            &(xlinkTaskStack[0]),
                            &(xlinkTaskTCB));

    static StaticTask_t live_led_TCB;
    static StackType_t live_led_Stack[configMINIMAL_STACK_SIZE];

    (void)xTaskCreateStatic(live_led_task,
                            "live_led",
                            configMINIMAL_STACK_SIZE,
                            NULL,
                            configMAX_PRIORITIES - 3U,
                            &(live_led_Stack[0]),
                            &(live_led_TCB));

    /* Start the scheduler. */
    vTaskStartScheduler();

    for (;;)
        ;

    return;
}

static int uart_rx_ind(size_t size, void *userdata)
{
    SemaphoreHandle_t *semaphore = (SemaphoreHandle_t *)userdata;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(*semaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    return 0;
}

static xlink_frame_t *xlink_frame_send_alloc(void *transport_handle, uint16_t needed)
{
    return (xlink_frame_t *)gd32_uart_alloc_dma_element(transport_handle, needed);
}

static int xlink_uart_send(void *transport_handle, xlink_frame_t *frame)
{
    return gd32_uart_append_dma_send_list(transport_handle, (struct dma_element *)frame);
}
static xlink_context_p xlink_ctx = NULL;
static void xlinkTask(void *parameters)
{
    int upgrade_init(xlink_context_p context);
    /* Unused parameters. */
    (void)parameters;
    uart_init();
    void *uart_handle = gd32_uart_get_handle("uart1");
    SemaphoreHandle_t uart_rx_semaphore = xSemaphoreCreateCounting(0xffffffff, 0);
    if (uart_handle == NULL)
    {
        for (;;)
            vTaskDelay(WINT_MAX);
    }
    static xlink_port_api_t xlink_port = {
        .malloc_fn = xlink_freertos_malloc,
        .free_fn = xlink_freertos_free,
        .mutex_create_fn = xlink_freertos_mutex_create,
        .mutex_delete_fn = xlink_freertos_mutex_delete,
        .mutex_lock_fn = xlink_freertos_mutex_lock,
        .mutex_unlock_fn = xlink_freertos_mutex_unlock,
        .transport_send_fn = xlink_uart_send,
        .frame_send_alloc_fn = xlink_frame_send_alloc,
    };
    xlink_ctx = xlink_context_create(&xlink_port, uart_handle);
    upgrade_init(xlink_ctx);
    gd32_uart_set_rx_indicate(uart_handle, uart_rx_ind, &uart_rx_semaphore);

    for (;;)
    {
        xSemaphoreTake(uart_rx_semaphore, portMAX_DELAY);
        for (;;)
        {
            struct dma_element *rx_block = get_rx_block(uart_handle);
            if (rx_block == NULL)
            {
                break;
            }
            uint8_t *data = rx_block->buffer;
            size_t size = rx_block->size;
            for (size_t i = 0; i < size; i++)
            {
                xlink_process_rx(xlink_ctx, data[i]);
            }
            // release rx_block
            free_rx_block(uart_handle, rx_block);
        }
    }
}

static void live_led_task(void *parameters)
{
    /* Unused parameters. */
    (void)parameters;
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_bit_set(GPIOB, GPIO_PIN_7); // Set PB7 high
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_7);
    int led_state = 1;

    for (;;)
    {
        /* Example Task Code */
        led_state = !led_state;
        if (led_state)
        {
            gpio_bit_set(GPIOB, GPIO_PIN_7); // Turn LED on
        }
        else
        {
            gpio_bit_reset(GPIOB, GPIO_PIN_7); // Turn LED off
        }
        xlink_control_led_state_send(xlink_ctx, led_state);
        vTaskDelay(pdMS_TO_TICKS(100)); /* delay 1000 ms */
    }
}

#if (configCHECK_FOR_STACK_OVERFLOW > 0)

void vApplicationStackOverflowHook(TaskHandle_t xTask,
                                   char *pcTaskName)
{
    /* Check pcTaskName for the name of the offending task,
     * or pxCurrentTCB if pcTaskName has itself been corrupted. */
    (void)xTask;
    (void)pcTaskName;
}

#endif /* #if ( configCHECK_FOR_STACK_OVERFLOW > 0 ) */
/*-----------------------------------------------------------*/

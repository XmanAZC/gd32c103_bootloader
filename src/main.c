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
#include "onchip_flash_port.h"

static void exampleTask(void *parameters) __attribute__((noreturn));

static void xlinkTask(void *parameters);

static uint32_t start_address;
static uint32_t size_bytes;
static uint32_t chunk_size;
static uint32_t check_crc32;

static int GetFirmwareInfo_cb(uint8_t comp_id,
                              uint8_t msg_id,
                              const uint8_t *payload,
                              uint8_t payload_len,
                              void *user_data)
{
#define BOOTLOADER_VERSION 0x00010001
#define BOOTLOADER_COMMIT_HASH 0x12345678
#define BOOTLOADER_COMPILE_TIMESTAMP 0x644B5A00
    void Reset_Handler(void);
    xlink_upgrade_get_firmware_info_t *msg = (xlink_upgrade_get_firmware_info_t *)payload;
    xlink_partition_type_t partition_type;
    uint32_t version, size_bytes, commit_hash, compile_timestamp;
    switch (msg->required_partition)
    {
    case XLINK_PARTITION_TYPE_BOOTLOADER:
        version = BOOTLOADER_VERSION;
        size_bytes = 0;
        commit_hash = BOOTLOADER_COMMIT_HASH;
        compile_timestamp = BOOTLOADER_COMPILE_TIMESTAMP;
        break;
    case XLINK_PARTITION_TYPE_APP_A:
        partition_type = XLINK_PARTITION_TYPE_APP_A;
        AppInfo_p app_info = (AppInfo_p)PARTITION_ADDRESS_APP_A_INFO;
        version = app_info->version;
        size_bytes = app_info->size_bytes;
        commit_hash = app_info->commit_hash;
        compile_timestamp = app_info->compile_timestamp;
        break;
    case XLINK_PARTITION_TYPE_APP_B:
        partition_type = XLINK_PARTITION_TYPE_APP_B;
        app_info = (AppInfo_p)PARTITION_ADDRESS_APP_B_INFO;
        version = app_info->version;
        size_bytes = app_info->size_bytes;
        commit_hash = app_info->commit_hash;
        compile_timestamp = app_info->compile_timestamp;
        break;
    default:
        return -1;
        break;
    }
    xlink_upgrade_firmware_info_send((xlink_context_p)user_data,
                                     partition_type,
                                     version,
                                     size_bytes,
                                     commit_hash,
                                     compile_timestamp,
                                     (size_t)Reset_Handler);
    return 0;
}

static int StartFirmwareUpgrade_cb(uint8_t comp_id,
                                   uint8_t msg_id,
                                   const uint8_t *payload,
                                   uint8_t payload_len,
                                   void *user_data)
{
    xlink_upgrade_start_firmware_upgrade_t *msg = (xlink_upgrade_start_firmware_upgrade_t *)payload;
    start_address = msg->start_address;
    size_bytes = msg->size_bytes;
    chunk_size = msg->chunk_size;
    check_crc32 = XLINK_INIT_CRC16;
    fmc_erase_pages(start_address, size_to_pages(size_bytes));
    xlink_upgrade_start_firmware_upgrade_response_send((xlink_context_p)user_data,
                                                       true);
    return 0;
}

static int FirmwareChunk_cb(uint8_t comp_id,
                            uint8_t msg_id,
                            const uint8_t *payload,
                            uint8_t payload_len,
                            void *user_data)
{
    xlink_upgrade_firmware_chunk_t *msg = (xlink_upgrade_firmware_chunk_t *)payload;
    size_t write_address = start_address + msg->offset * chunk_size;
    size_t write_size = msg->data_len;
    if (write_address + write_size > start_address + size_bytes)
    {
        xlink_upgrade_firmware_chunk_response_send((xlink_context_p)user_data,
                                                   msg->offset,
                                                   false);
        return -1;
    }
    fmc_program_word(write_address, msg->data, write_size);
    check_crc32 = xlink_crc16_with_init(msg->data, write_size, check_crc32);
    xlink_upgrade_firmware_chunk_response_send((xlink_context_p)user_data,
                                               msg->offset,
                                               true);
    return 0;
}

static int FinalizeFirmwareUpgrade_cb(uint8_t comp_id,
                                      uint8_t msg_id,
                                      const uint8_t *payload,
                                      uint8_t payload_len,
                                      void *user_data)
{
    xlink_upgrade_finalize_firmware_upgrade_t *msg = (xlink_upgrade_finalize_firmware_upgrade_t *)payload;
    bool success = (msg->expected_crc32 == check_crc32);
    xlink_upgrade_finalize_firmware_upgrade_response_send((xlink_context_p)user_data,
                                                          success);
    return 0;
}

int upgrade_init(xlink_context_p context)
{

    xlink_register_msg_handler(context,
                               XLINK_COMP_ID_UPGRADE,
                               XLINK_UPGRADE_MSG_ID_GET_FIRMWARE_INFO,
                               GetFirmwareInfo_cb,
                               NULL);

    xlink_register_msg_handler(context,
                               XLINK_COMP_ID_UPGRADE,
                               XLINK_UPGRADE_MSG_ID_START_FIRMWARE_UPGRADE,
                               StartFirmwareUpgrade_cb,
                               NULL);
    xlink_register_msg_handler(context,
                               XLINK_COMP_ID_UPGRADE,
                               XLINK_UPGRADE_MSG_ID_FIRMWARE_CHUNK,
                               FirmwareChunk_cb,
                               NULL);
    xlink_register_msg_handler(context,
                               XLINK_COMP_ID_UPGRADE,
                               XLINK_UPGRADE_MSG_ID_FINALIZE_FIRMWARE_UPGRADE,
                               FinalizeFirmwareUpgrade_cb,
                               NULL);

    return 0;
}

void entry(void)
{
    static StaticTask_t exampleTaskTCB;
    static StackType_t exampleTaskStack[configMINIMAL_STACK_SIZE];

    (void)xTaskCreateStatic(exampleTask,
                            "example",
                            configMINIMAL_STACK_SIZE,
                            NULL,
                            configMAX_PRIORITIES - 1U,
                            &(exampleTaskStack[0]),
                            &(exampleTaskTCB));

    /* Start the scheduler. */
    vTaskStartScheduler();

    for (;;)
        ;

    return;
}

static int uart_rx_ind(size_t size, void *userdata)
{
    SemaphoreHandle_t semaphore = (SemaphoreHandle_t)userdata;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(semaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    return 0;
}

static void xlinkTask(void *parameters)
{
    /* Unused parameters. */
    (void)parameters;
    uart_init();
    void *uart_handle = gd32_uart_get_handle("uart1");
    SemaphoreHandle_t uart_rx_semaphore = xSemaphoreCreateCounting(100, 0);
    if (uart_handle == NULL)
    {
        return;
    }
    xlink_context_p xlink_ctx = xlink_context_create(uart_handle);
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

static void exampleTask(void *parameters)
{
    static StaticTask_t xlinkTaskTCB;
    static StackType_t xlinkTaskStack[1024];

    /* Unused parameters. */
    (void)parameters;
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_bit_set(GPIOB, GPIO_PIN_7); // Set PB7 high
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_7);
    int led_state = 1;

    (void)xTaskCreateStatic(xlinkTask,
                            "xlink",
                            1024,
                            NULL,
                            configMAX_PRIORITIES - 2U,
                            &(xlinkTaskStack[0]),
                            &(xlinkTaskTCB));
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
        vTaskDelay(pdMS_TO_TICKS(1000)); /* delay 100 ms */
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

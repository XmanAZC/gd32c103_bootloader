#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <timers.h>
#include <semphr.h>
#include <stdio.h>
#include "config.h"
#include "partition.h"
#include "gd32c10x.h"

static void exampleTask(void *parameters) __attribute__((noreturn));

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

static void exampleTask(void *parameters)
{
    /* Unused parameters. */
    (void)parameters;
    BootFromInfo_p boot_from_info = (BootFromInfo_p)PARTITION_ADDRESS_BOOTFROM;
    jump_to_app(boot_from_info->activeApp);
    jump_to_app(!boot_from_info->activeApp);
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
        vTaskDelay(pdMS_TO_TICKS(100)); /* delay 100 ms */
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

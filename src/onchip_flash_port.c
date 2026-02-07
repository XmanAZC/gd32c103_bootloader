#include "onchip_flash_port.h"
#include "config.h"
#include "gd32c10x.h"

#if defined(SOC_SERIES_GD32C11x) || defined(SOC_SERIES_GD32C10x)
static const uint32_t FMC_FLAGS = FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGAERR | FMC_FLAG_PGERR;
#elif defined(SOC_SERIES_GD32F10x) || defined(SOC_SERIES_GD32F30x)
static const uint32_t FMC_FLAGS = FMC_FLAG_BANK0_END | FMC_FLAG_BANK0_WPERR | FMC_FLAG_BANK0_PGERR;
#endif

void fmc_erase_pages(uint32_t page_address, uint32_t page_num)
{
    uint32_t EraseCounter;
    fmc_state_enum state;

    asm volatile("cpsid i"); /* close interrupt */

    /* unlock the flash program/erase controller */
    fmc_unlock();

    /* clear all pending flags */
    fmc_flag_clear(FMC_FLAGS);

    /* erase the flash pages */
    for (EraseCounter = 0; EraseCounter < page_num; EraseCounter++)
    {
        state = fmc_page_erase(page_address + (PAGE_SIZE * EraseCounter));
        if (FMC_READY != state)
        {
            break;
        }
        fmc_flag_clear(FMC_FLAGS);
    }

    /* lock the main FMC after the erase operation */
    fmc_lock();

    asm volatile("cpsie i"); /* open interrupt */
}

int fmc_erase_pages_check(uint32_t page_address, uint32_t page_num)
{
    uint32_t i;

    uint32_t *ptrd = (uint32_t *)page_address;

    /* check flash whether has been erased */
    for (i = 0; i < page_num * (PAGE_SIZE >> 2); i++)
    {
        if (0xFFFFFFFF != (*ptrd))
        {
            return -1;
        }
        else
        {
            ptrd++;
        }
    }
    return 0;
}

uint32_t fmc_program_data(uint32_t address, void *data, uint32_t size)
{
    uint32_t i;

    asm volatile("cpsid i"); /* close interrupt */

    /* unlock the flash program/erase controller */
    fmc_unlock();

    /* program flash */
    for (i = 0; i < size / 4; i++)
    {
        fmc_word_program(address, *(uint32_t *)data);
        data = (void *)((uint8_t *)data + 4);
        address += 4;
        fmc_flag_clear(FMC_FLAGS);
    }

    /* lock the main FMC after the program operation */
    fmc_lock();

    asm volatile("cpsie i"); /* open interrupt */

    return address;
}

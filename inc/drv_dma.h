/*
 * Copyright (c) 2006-2024, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date         Author      Notes
 * 2024-03-19   Evlers      first implementation
 */

#ifndef _DRV_DMA_H_
#define _DRV_DMA_H_

#define DRV_DMA_CONFIG(dmax, chx) {         \
    .periph = DMA##dmax,                    \
    .channel = DMA_CH##chx,                 \
    .rcu = RCU_DMA##dmax,                   \
    .irq = DMA##dmax##_Channel##chx##_IRQn, \
}

struct dma_config
{
    uint32_t periph;
    rcu_periph_enum rcu;
    dma_channel_enum channel;
    IRQn_Type irq;
};

#endif /* _DRV_DMA_H_ */

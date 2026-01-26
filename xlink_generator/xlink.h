#ifndef XLINK_H
#define XLINK_H

#include <FreeRTOS.h>
#include <semphr.h>
#include <stddef.h>
#include <drv_simple_uart.h>

#define XLINK_SOF 0xA5u
#define XLINK_MAX_PAYLOAD 250u
#define XLINK_LENGTH_OF_HEADER 4u
#define XLINK_LENGTH_OF_CRC 2u

#define xlink_packed(declare) declare __attribute__((packed))

typedef xlink_packed(struct xlink_message_def {
    uint8_t sof;     // fixed to XLINK_SOF
    uint8_t len;     // length of payload
    uint8_t comp_id; // component ID
    uint8_t msg_id;  // message ID
    uint8_t payload[XLINK_MAX_PAYLOAD];
    uint16_t crc; // CRC16 over LEN, MSGID, PAYLOAD
}) xlink_message_t,
    *xlink_message_p;

typedef struct xlink_message_list_element_def
{
    xlink_message_p msg;
    struct xlink_message_list_element_def *next;
} xlink_message_list_element_t,
    *xlink_message_list_element_p;

#ifndef XLINK_CRC_16
#define XLINK_INIT_CRC16 0xFFFF

static const uint16_t crc_table[256] = {
    0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF,
    0x8C48, 0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7,
    0x1081, 0x0108, 0x3393, 0x221A, 0x56A5, 0x472C, 0x75B7, 0x643E,
    0x9CC9, 0x8D40, 0xBFDB, 0xAE52, 0xDAED, 0xCB64, 0xF9FF, 0xE876,
    0x2102, 0x308B, 0x0210, 0x1399, 0x6726, 0x76AF, 0x4434, 0x55BD,
    0xAD4A, 0xBCC3, 0x8E58, 0x9FD1, 0xEB6E, 0xFAE7, 0xC87C, 0xD9F5,
    0x3183, 0x200A, 0x1291, 0x0318, 0x77A7, 0x662E, 0x54B5, 0x453C,
    0xBDCB, 0xAC42, 0x9ED9, 0x8F50, 0xFBED, 0xEA64, 0xD8FF, 0xC976,
    0x4204, 0x538D, 0x6116, 0x709F, 0x0420, 0x15A9, 0x2732, 0x36BB,
    0xCE4C, 0xDFC5, 0xED5E, 0xFCD7, 0x8868, 0x99E1, 0xAB7A, 0xBAF3,
    0x5285, 0x430C, 0x7197, 0x601E, 0x14A1, 0x0528, 0x37B3, 0x263A,
    0xDECD, 0xCF44, 0xFDDF, 0xEC56, 0x98E9, 0x8960, 0xBBFB, 0xAA72,
    0x6306, 0x728F, 0x4014, 0x519D, 0x2522, 0x34AB, 0x0630, 0x17B9,
    0xEF4E, 0xFEC7, 0xCC5C, 0xDDD5, 0xA96A, 0xB8E3, 0x8A78, 0x9BF1,
    0x7387, 0x620E, 0x5095, 0x411C, 0x35A3, 0x242A, 0x16B1, 0x0738,
    0xFFCF, 0xEE46, 0xDCDD, 0xCD54, 0xB9EB, 0xA862, 0x9AF9, 0x8B70,
    0x8408, 0x9581, 0xA71A, 0xB693, 0xC22C, 0xD3A5, 0xE13E, 0xF0B7,
    0x0840, 0x19C9, 0x2B52, 0x3ADB, 0x4E64, 0x5FED, 0x6D76, 0x7CFF,
    0x9489, 0x8500, 0xB79B, 0xA612, 0xD2AD, 0xC324, 0xF1BF, 0xE036,
    0x18C1, 0x0948, 0x3BD3, 0x2A5A, 0x5EE5, 0x4F6C, 0x7DF7, 0x6C7E,
    0xA50A, 0xB483, 0x8618, 0x9791, 0xE32E, 0xF2A7, 0xC03C, 0xD1B5,
    0x2942, 0x38CB, 0x0A50, 0x1BD9, 0x6F66, 0x7EEF, 0x4C74, 0x5DFD,
    0xB58B, 0xA402, 0x9699, 0x8710, 0xF3AF, 0xE226, 0xD0BD, 0xC134,
    0x39C3, 0x284A, 0x1AD1, 0x0B58, 0x7FE7, 0x6E6E, 0x5CF5, 0x4D7C,
    0xC60C, 0xD785, 0xE51E, 0xF497, 0x8028, 0x91A1, 0xA33A, 0xB2B3,
    0x4A44, 0x5BCD, 0x6956, 0x78DF, 0x0C60, 0x1DE9, 0x2F72, 0x3EFB,
    0xD68D, 0xC704, 0xF59F, 0xE416, 0x90A9, 0x8120, 0xB3BB, 0xA232,
    0x5AC5, 0x4B4C, 0x79D7, 0x685E, 0x1CE1, 0x0D68, 0x3FF3, 0x2E7A,
    0xE70E, 0xF687, 0xC41C, 0xD595, 0xA12A, 0xB0A3, 0x8238, 0x93B1,
    0x6B46, 0x7ACF, 0x4854, 0x59DD, 0x2D62, 0x3CEB, 0x0E70, 0x1FF9,
    0xF78F, 0xE606, 0xD49D, 0xC514, 0xB1AB, 0xA022, 0x92B9, 0x8330,
    0x7BC7, 0x6A4E, 0x58D5, 0x495C, 0x3DE3, 0x2C6A, 0x1EF1, 0x0F78};

static inline uint16_t xlink_crc16(const uint8_t *pBuffer, uint16_t length)
{
    uint16_t crcTmp = XLINK_INIT_CRC16;
    while (length--)
    {
        crcTmp = (crcTmp >> 8) ^ crc_table[(*pBuffer++ ^ (crcTmp & 0xFF)) & 0xFF];
    }
    return crcTmp;
}

static inline uint16_t xlink_crc16_with_init(const uint8_t *pBuffer, uint16_t length, uint16_t init)
{
    while (length--)
    {
        init = (init >> 8) ^ crc_table[(*pBuffer++ ^ (init & 0xFF)) & 0xFF];
    }
    return init;
}
#endif // XLINK_CRC_16

typedef int (*xlink_msg_handler_t)(uint8_t comp_id,
                                   uint8_t msg_id,
                                   const uint8_t *payload,
                                   uint8_t payload_len,
                                   void *user_data);

typedef struct xlink_message_handler_element_def
{
    uint8_t msg_id;
    xlink_msg_handler_t handler;
    void *user_data;
    struct xlink_message_handler_element_def *next;
} xlink_msg_id_handler_element_t, *xlink_message_handler_element_p;

typedef struct xlink_comp_id_handler_element_def
{
    uint8_t comp_id;
    xlink_message_handler_element_p handlers_list;
    xlink_message_handler_element_p *handlers_list_pos;
    struct xlink_comp_id_handler_element_def *next;
} xlink_comp_id_handler_element_t, *xlink_comp_id_handler_element_p;

enum xlink_msg_rx_state
{
    XLINK_MSG_RX_WAIT_MAGIC,
    XLINK_MSG_RX_WAIT_ID_LENGTH,
    XLINK_MSG_RX_WAIT_PAYLOAD,
    XLINK_MSG_RX_WAIT_CHECKSUM
};

typedef int (*xlink_transport_send_t)(const uint8_t *data, uint16_t len);

typedef struct xlink_context_def
{
    SemaphoreHandle_t global_mutex;
    xlink_comp_id_handler_element_p comp_id_handler_map;
    xlink_comp_id_handler_element_p *comp_id_handler_map_pos;

    SemaphoreHandle_t rx_mutex;
    enum xlink_msg_rx_state rx_msg_state;
    xlink_message_t rx_msg;
    uint16_t rx_msg_crc;
    uint16_t rx_msg_pos;
    uint16_t expected_len;

    void *gd32_serial_handle;

} xlink_context_t,
    *xlink_context_p;

static inline xlink_context_p xlink_context_create(void *_gd32_serial_handle)
{
    xlink_context_p context = (xlink_context_p)pvPortMalloc(sizeof(xlink_context_t));
    if (context == NULL)
    {
        return NULL;
    }

    context->comp_id_handler_map = NULL;
    context->comp_id_handler_map_pos = &context->comp_id_handler_map;
    context->rx_msg_pos = 0;
    context->rx_msg_crc = 0;
    context->rx_msg_state = XLINK_MSG_RX_WAIT_MAGIC;

    context->global_mutex = xSemaphoreCreateMutex();
    if (context->global_mutex == NULL)
    {
        vPortFree(context);
        return NULL;
    }
    context->rx_mutex = xSemaphoreCreateMutex();
    if (context->rx_mutex == NULL)
    {
        vSemaphoreDelete(context->global_mutex);
        vPortFree(context);
        return NULL;
    }

    context->gd32_serial_handle = _gd32_serial_handle;

    return context;
}

static inline int xlink_register_msg_handler(xlink_context_p context,
                                             uint8_t comp_id,
                                             uint8_t msg_id,
                                             xlink_msg_handler_t handler,
                                             void *user_data)
{
    if (xSemaphoreTake(context->global_mutex, portMAX_DELAY) != pdTRUE)
    {
        return -1;
    }

    xlink_comp_id_handler_element_p comp_id_element = context->comp_id_handler_map;
    while (comp_id_element != NULL)
    {
        if (comp_id_element->comp_id == comp_id)
        {
            break;
        }
        comp_id_element = comp_id_element->next;
    }
    if (comp_id_element == NULL)
    {
        comp_id_element = (xlink_comp_id_handler_element_p)pvPortMalloc(sizeof(xlink_comp_id_handler_element_t));
        if (comp_id_element == NULL)
        {
            xSemaphoreGive(context->global_mutex);
            return -1;
        }
        comp_id_element->comp_id = comp_id;
        comp_id_element->handlers_list = NULL;
        comp_id_element->handlers_list_pos = &comp_id_element->handlers_list;
        comp_id_element->next = NULL;
        *context->comp_id_handler_map_pos = comp_id_element;
        context->comp_id_handler_map_pos = &comp_id_element->next;
    }

    xlink_message_handler_element_p msg_id_pos = comp_id_element->handlers_list;
    while (msg_id_pos != NULL)
    {
        if (msg_id_pos->msg_id == msg_id &&
            msg_id_pos->handler == handler &&
            msg_id_pos->user_data == user_data)
        {
            xSemaphoreGive(context->global_mutex);
            return -1; // Handler already registered
        }
        msg_id_pos = msg_id_pos->next;
    }

    xlink_message_handler_element_p new_handler_element = (xlink_message_handler_element_p)pvPortMalloc(sizeof(xlink_msg_id_handler_element_t));
    if (new_handler_element == NULL)
    {
        xSemaphoreGive(context->global_mutex);
        return -1;
    }
    new_handler_element->msg_id = msg_id;
    new_handler_element->handler = handler;
    new_handler_element->user_data = user_data;
    new_handler_element->next = NULL;
    *comp_id_element->handlers_list_pos = new_handler_element;
    comp_id_element->handlers_list_pos = &new_handler_element->next;
    xSemaphoreGive(context->global_mutex);
    return 0;
}

static inline int xlink_unregister_msg_handler(xlink_context_p context,
                                               uint8_t comp_id,
                                               uint8_t msg_id,
                                               xlink_msg_handler_t handler,
                                               void *user_data)
{
    if (xSemaphoreTake(context->global_mutex, portMAX_DELAY) != pdTRUE)
    {
        return -1;
    }

    xlink_comp_id_handler_element_p comp_id_element = context->comp_id_handler_map;
    while (comp_id_element != NULL)
    {
        if (comp_id_element->comp_id == comp_id)
        {
            break;
        }
        comp_id_element = comp_id_element->next;
    }
    if (comp_id_element == NULL)
    {
        xSemaphoreGive(context->global_mutex);
        return -1; // Component ID not found
    }

    xlink_message_handler_element_p *msg_id_pos = &comp_id_element->handlers_list;
    xlink_message_handler_element_p current = comp_id_element->handlers_list;
    while (current != NULL)
    {
        if (current->msg_id == msg_id &&
            current->handler == handler &&
            current->user_data == user_data)
        {
            *msg_id_pos = current->next;
            if (comp_id_element->handlers_list_pos == &current->next)
            {
                comp_id_element->handlers_list_pos = msg_id_pos;
            }
            vPortFree(current);
            xSemaphoreGive(context->global_mutex);
            return 0; // Successfully unregistered
        }
        msg_id_pos = &current->next;
        current = current->next;
    }

    xSemaphoreGive(context->global_mutex);
    return -1; // Handler not found
}

static inline int xlink_process_rx(xlink_context_p context,
                                   uint8_t data)
{
    switch (context->rx_msg_state)
    {
    case XLINK_MSG_RX_WAIT_MAGIC:
        if (data == XLINK_SOF)
        {
            context->rx_msg.sof = data;
            context->expected_len = 3; // LEN + COMP_ID + MSG_ID
            context->rx_msg_pos = 0;
            context->rx_msg_state = XLINK_MSG_RX_WAIT_ID_LENGTH;
            context->rx_msg_crc = XLINK_INIT_CRC16;
        }
        break;

    case XLINK_MSG_RX_WAIT_ID_LENGTH:
    {
        ((uint8_t *)(&(context->rx_msg.len)))[context->rx_msg_pos] = data;
        context->rx_msg_crc = xlink_crc16_with_init(&data, 1, context->rx_msg_crc);
        context->rx_msg_pos++;
        if (context->rx_msg_pos == context->expected_len)
        {
            context->expected_len += context->rx_msg.len + 2; // Add payload length + CRC
            context->rx_msg_state = XLINK_MSG_RX_WAIT_PAYLOAD;
            context->rx_msg_pos = 0;
        }
    }
    break;

    case XLINK_MSG_RX_WAIT_PAYLOAD:
    case XLINK_MSG_RX_WAIT_CHECKSUM:
    {
        context->rx_msg.payload[context->rx_msg_pos++] = data;
        if (context->rx_msg_pos >= context->expected_len)
        {
            context->rx_msg_state = XLINK_MSG_RX_WAIT_MAGIC;
            uint16_t received_crc = (uint16_t)(context->rx_msg.payload[context->rx_msg_pos - 2]) |
                                    ((uint16_t)(context->rx_msg.payload[context->rx_msg_pos - 1]) << 8);
            if (received_crc == context->rx_msg_crc)
            {
                // Valid message received
                // call handler
                xlink_comp_id_handler_element_p comp_id_element = context->comp_id_handler_map;
                while (comp_id_element)
                {
                    if (comp_id_element->comp_id == context->rx_msg.comp_id)
                    {
                        xlink_message_handler_element_p handler_element = comp_id_element->handlers_list;
                        while (handler_element)
                        {
                            if (handler_element->msg_id == context->rx_msg.msg_id)
                            {
                                handler_element->handler(context->rx_msg.comp_id,
                                                         context->rx_msg.msg_id,
                                                         context->rx_msg.payload,
                                                         context->rx_msg.len,
                                                         handler_element->user_data);
                            }
                            handler_element = handler_element->next;
                        }
                        break;
                    }
                    comp_id_element = comp_id_element->next;
                }
                return 0;
            }
            return -2; // CRC error
        }
        else if (context->rx_msg_pos <= context->expected_len - sizeof(uint16_t))
        {
            context->rx_msg_crc = xlink_crc16_with_init(&data, 1, context->rx_msg_crc);
        }
    }
    break;
    }
    return -1;
}

void _memcpy_and_crc16(void *dst, const void *src, size_t count)
{
    uint16_t crc = XLINK_INIT_CRC16;
    uint8_t *dst_ptr = (uint8_t *)dst;
    const uint8_t *src_ptr = (const uint8_t *)src;

    while (count--)
    {
        *dst_ptr = *src_ptr;
        crc = (crc >> 8) ^ crc_table[(*src_ptr ^ (crc & 0xFF)) & 0xFF];
        dst_ptr++;
        src_ptr++;
    }
    *dst_ptr++ = (uint8_t)(crc & 0xFF);
    *dst_ptr = (uint8_t)((crc >> 8) & 0xFF);
}

static inline int xlink_send(xlink_context_p context,
                             uint8_t comp_id,
                             uint8_t msg_id,
                             const uint8_t *payload,
                             uint8_t payload_len)
{
    struct dma_element *dma_node = gd32_uart_alloc_dma_element(context->gd32_serial_handle,
                                                               payload_len +
                                                                   XLINK_LENGTH_OF_HEADER +
                                                                   XLINK_LENGTH_OF_CRC);
    if (dma_node == NULL)
    {
        return -1;
    }
    xlink_message_p msg = (xlink_message_p)(dma_node->buffer);
    if (msg == NULL)
    {
        return -1;
    }

    msg->sof = XLINK_SOF;
    msg->len = payload_len;
    msg->comp_id = comp_id;
    msg->msg_id = msg_id;
    _memcpy_and_crc16(msg->payload, payload, payload_len);
    gd32_uart_append_dma_send_list(context->gd32_serial_handle, dma_node);

    return 0;
}

#endif // XLINK_H

#ifndef XLINK_UPGRADE_H
#define XLINK_UPGRADE_H

#include "xlink.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define XLINK_COMP_ID_UPGRADE 1

#define XLINK_UPGRADE_MSG_ID_GET_FIRMWARE_INFO 0
#define XLINK_UPGRADE_MSG_ID_FIRMWARE_INFO 1
#define XLINK_UPGRADE_MSG_ID_START_FIRMWARE_UPGRADE 2
#define XLINK_UPGRADE_MSG_ID_START_FIRMWARE_UPGRADE_RESPONSE 3
#define XLINK_UPGRADE_MSG_ID_FIRMWARE_CHUNK 4
#define XLINK_UPGRADE_MSG_ID_FIRMWARE_CHUNK_RESPONSE 5
#define XLINK_UPGRADE_MSG_ID_FINALIZE_FIRMWARE_UPGRADE 6
#define XLINK_UPGRADE_MSG_ID_FINALIZE_FIRMWARE_UPGRADE_RESPONSE 7

typedef uint8_t xlink_partition_type_t;
#define XLINK_PARTITION_TYPE_BOOTLOADER 0
#define XLINK_PARTITION_TYPE_APP_A 1
#define XLINK_PARTITION_TYPE_APP_B 2

typedef xlink_packed(struct xlink_upgrade_get_firmware_info_t_def
{
    xlink_partition_type_t required_partition;
}) xlink_upgrade_get_firmware_info_t;

static inline int xlink_upgrade_get_firmware_info_send(xlink_context_p context, xlink_partition_type_t required_partition)
{
    xlink_upgrade_get_firmware_info_t msg;
    msg.required_partition = required_partition;
    return xlink_send(context, XLINK_COMP_ID_UPGRADE, XLINK_UPGRADE_MSG_ID_GET_FIRMWARE_INFO, (const uint8_t *)&msg, (uint8_t)sizeof(msg));
}

typedef xlink_packed(struct xlink_upgrade_firmware_info_t_def
{
    xlink_partition_type_t partition_type;
    uint32_t version;
    uint32_t size_bytes;
    uint32_t commit_hash;
    uint32_t compile_timestamp;
    uint32_t current_base_address;
}) xlink_upgrade_firmware_info_t;

static inline int xlink_upgrade_firmware_info_send(xlink_context_p context, xlink_partition_type_t partition_type, uint32_t version, uint32_t size_bytes, uint32_t commit_hash, uint32_t compile_timestamp, uint32_t current_base_address)
{
    xlink_upgrade_firmware_info_t msg;
    msg.partition_type = partition_type;
    msg.version = version;
    msg.size_bytes = size_bytes;
    msg.commit_hash = commit_hash;
    msg.compile_timestamp = compile_timestamp;
    msg.current_base_address = current_base_address;
    return xlink_send(context, XLINK_COMP_ID_UPGRADE, XLINK_UPGRADE_MSG_ID_FIRMWARE_INFO, (const uint8_t *)&msg, (uint8_t)sizeof(msg));
}

typedef xlink_packed(struct xlink_upgrade_start_firmware_upgrade_t_def
{
    uint32_t start_address;
    uint32_t size_bytes;
    uint32_t chunk_size;
}) xlink_upgrade_start_firmware_upgrade_t;

static inline int xlink_upgrade_start_firmware_upgrade_send(xlink_context_p context, uint32_t start_address, uint32_t size_bytes, uint32_t chunk_size)
{
    xlink_upgrade_start_firmware_upgrade_t msg;
    msg.start_address = start_address;
    msg.size_bytes = size_bytes;
    msg.chunk_size = chunk_size;
    return xlink_send(context, XLINK_COMP_ID_UPGRADE, XLINK_UPGRADE_MSG_ID_START_FIRMWARE_UPGRADE, (const uint8_t *)&msg, (uint8_t)sizeof(msg));
}

typedef xlink_packed(struct xlink_upgrade_start_firmware_upgrade_response_t_def
{
    bool accepted;
}) xlink_upgrade_start_firmware_upgrade_response_t;

static inline int xlink_upgrade_start_firmware_upgrade_response_send(xlink_context_p context, bool accepted)
{
    xlink_upgrade_start_firmware_upgrade_response_t msg;
    msg.accepted = accepted;
    return xlink_send(context, XLINK_COMP_ID_UPGRADE, XLINK_UPGRADE_MSG_ID_START_FIRMWARE_UPGRADE_RESPONSE, (const uint8_t *)&msg, (uint8_t)sizeof(msg));
}

#define XLINK_UPGRADE_FIRMWARE_CHUNK_DATA_MAX_LEN (XLINK_MAX_PAYLOAD - 5u)

typedef xlink_packed(struct xlink_upgrade_firmware_chunk_t_def
{
    uint32_t offset;
    uint8_t data_len;
    uint8_t data[XLINK_UPGRADE_FIRMWARE_CHUNK_DATA_MAX_LEN];
}) xlink_upgrade_firmware_chunk_t;

static inline int xlink_upgrade_firmware_chunk_send(xlink_context_p context, uint32_t offset, const uint8_t *data, uint8_t data_len)
{
    xlink_upgrade_firmware_chunk_t msg;
    if (data_len > 0u && data == NULL)
    {
        return -1;
    }
    if (data_len > XLINK_UPGRADE_FIRMWARE_CHUNK_DATA_MAX_LEN)
    {
        return -1;
    }
    msg.offset = offset;
    msg.data_len = data_len;
    memcpy(msg.data, data, data_len);
    return xlink_send(context, XLINK_COMP_ID_UPGRADE, XLINK_UPGRADE_MSG_ID_FIRMWARE_CHUNK, (const uint8_t *)&msg, (uint8_t)(5u + msg.data_len));
}

typedef xlink_packed(struct xlink_upgrade_firmware_chunk_response_t_def
{
    uint32_t offset;
    bool accepted;
}) xlink_upgrade_firmware_chunk_response_t;

static inline int xlink_upgrade_firmware_chunk_response_send(xlink_context_p context, uint32_t offset, bool accepted)
{
    xlink_upgrade_firmware_chunk_response_t msg;
    msg.offset = offset;
    msg.accepted = accepted;
    return xlink_send(context, XLINK_COMP_ID_UPGRADE, XLINK_UPGRADE_MSG_ID_FIRMWARE_CHUNK_RESPONSE, (const uint8_t *)&msg, (uint8_t)sizeof(msg));
}

typedef xlink_packed(struct xlink_upgrade_finalize_firmware_upgrade_t_def
{
    uint32_t expected_crc32;
}) xlink_upgrade_finalize_firmware_upgrade_t;

static inline int xlink_upgrade_finalize_firmware_upgrade_send(xlink_context_p context, uint32_t expected_crc32)
{
    xlink_upgrade_finalize_firmware_upgrade_t msg;
    msg.expected_crc32 = expected_crc32;
    return xlink_send(context, XLINK_COMP_ID_UPGRADE, XLINK_UPGRADE_MSG_ID_FINALIZE_FIRMWARE_UPGRADE, (const uint8_t *)&msg, (uint8_t)sizeof(msg));
}

typedef xlink_packed(struct xlink_upgrade_finalize_firmware_upgrade_response_t_def
{
    bool success;
}) xlink_upgrade_finalize_firmware_upgrade_response_t;

static inline int xlink_upgrade_finalize_firmware_upgrade_response_send(xlink_context_p context, bool success)
{
    xlink_upgrade_finalize_firmware_upgrade_response_t msg;
    msg.success = success;
    return xlink_send(context, XLINK_COMP_ID_UPGRADE, XLINK_UPGRADE_MSG_ID_FINALIZE_FIRMWARE_UPGRADE_RESPONSE, (const uint8_t *)&msg, (uint8_t)sizeof(msg));
}

#endif // XLINK_UPGRADE_H

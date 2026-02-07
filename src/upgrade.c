#include "xlink_upgrade.h"
#include "partition.h"
#include "gd32c10x.h"
#include "onchip_flash_port.h"

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
    extern uint32_t __gVectors[];
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
                                     (size_t)__gVectors);
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
    fmc_program_data(write_address, msg->data, write_size);
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
                               context);

    xlink_register_msg_handler(context,
                               XLINK_COMP_ID_UPGRADE,
                               XLINK_UPGRADE_MSG_ID_START_FIRMWARE_UPGRADE,
                               StartFirmwareUpgrade_cb,
                               context);
    xlink_register_msg_handler(context,
                               XLINK_COMP_ID_UPGRADE,
                               XLINK_UPGRADE_MSG_ID_FIRMWARE_CHUNK,
                               FirmwareChunk_cb,
                               context);
    xlink_register_msg_handler(context,
                               XLINK_COMP_ID_UPGRADE,
                               XLINK_UPGRADE_MSG_ID_FINALIZE_FIRMWARE_UPGRADE,
                               FinalizeFirmwareUpgrade_cb,
                               context);

    return 0;
}

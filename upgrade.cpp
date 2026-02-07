#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <string>
#include <fcntl.h>
#include <termios.h>
#include <thread>
#include <chrono>
#include <vector>
#include <signal.h>

#include "xlink_generator/xlink.h"
#include "xlink_generator/xlink_port_posix.h"
#include "xlink_generator/xlink_port_stdlib.h"
#include "xlink_generator/xlink_upgrade.h"
#include "xlink_generator/xlink_control.h"
#include "inc/partition.h"

using namespace std;

static int get_mcu_firmware_version(xlink_context_p ctx, xlink_partition_type_t partition_type, xlink_upgrade_firmware_info_t *out_info);

static void _close(int sig)
{
    (void)sig;
    printf("\nCtrl+c\n");
    exit(0);
}

static xlink_frame_t *xlink_frame_send_alloc(void *transport_handle, uint16_t needed)
{
    (void)transport_handle;
    xlink_frame_t *frame = (xlink_frame_t *)malloc(sizeof(xlink_frame_t));
    if (frame == NULL)
    {
        return NULL;
    }
    frame->buffer = (uint8_t *)malloc(needed);
    if (frame->buffer == NULL)
    {
        free(frame);
        return NULL;
    }
    frame->size = needed;
    frame->next = NULL;
    frame->prev = NULL;
    return frame;
}

static int xlink_transport_send(void *transport_handle, xlink_frame_t *frame)
{
    ssize_t send_size = write((int)(size_t)transport_handle, frame->buffer, frame->size);
    int ret = send_size == (ssize_t)frame->size ? 0 : -1;
    free(frame->buffer);
    free(frame);
    return ret;
}

static void usage(void)
{
    printf(
        "Usage: -d /path/to/device -f /path/to/file\n"
        "-h, --help             display this help and exit\n"
        "-s, --show             show device information\n"
        "-d, --device           device file path\n"
        "-f, --file             file path\n");
}

static void serial_set_param(int fd, int baudrate)
{
    struct termios param;
    tcgetattr(fd, &param);

    cfsetospeed(&param, baudrate);
    cfsetispeed(&param, baudrate);

    param.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    param.c_oflag &= ~OPOST;
    param.c_cflag |= (CLOCAL | CREAD | CS8);
    param.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    param.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    param.c_cc[VMIN] = 1;
    param.c_cc[VTIME] = 255;

    tcsetattr(fd, TCSANOW, &param);
}

class upgrade_partition
{
public:
    upgrade_partition(xlink_partition_type_t type, xlink_context_p context, int fd)
        : partition_type(type), ctx(context)
    {
        switch (partition_type)
        {
        case XLINK_PARTITION_TYPE_BOOTLOADER:
            start_address = PARTITION_ADDRESS_BOOTLOADER;
            size_bytes = PARTITION_SIZE_BOOTLOADER;
            partition_name = "BOOTLOADER";
            break;
        case XLINK_PARTITION_TYPE_APP_A:
            start_address = PARTITION_ADDRESS_APP_A_INFO;
            size_bytes = PARTITION_SIZE_APP_A + PARTITION_SIZE_APP_A_INFO;
            partition_name = "APP_A";
            break;
        case XLINK_PARTITION_TYPE_APP_B:
            start_address = PARTITION_ADDRESS_APP_B_INFO;
            size_bytes = PARTITION_SIZE_APP_B + PARTITION_SIZE_APP_B_INFO;
            partition_name = "APP_B";
            break;
        default:
            start_address = 0;
            size_bytes = 0;
            break;
        }
        int ret = lseek(fd, start_address - PARTITION_ADDRESS_BOOTLOADER, SEEK_SET);
        if (ret < 0)
        {
            printf("lseek to partition %s failed\n", partition_name.c_str());
            return;
        }
        firmware_data.resize(size_bytes);
        ssize_t read_bytes = read(fd, firmware_data.data(), size_bytes);
        if (read_bytes != (ssize_t)size_bytes)
        {
            printf("read partition %s failed\n", partition_name.c_str());
            firmware_data.clear();
            return;
        }
    }

    upgrade_partition(uint32_t _start_address, uint32_t _size_bytes, string _partition_name, xlink_context_p context, const uint8_t *data, size_t data_len)
        : start_address(_start_address), size_bytes(_size_bytes), partition_name(_partition_name), ctx(context)
    {
        if (data_len > size_bytes)
        {
            printf("Invalid data length for partition %s: %zu\n", partition_name.c_str(), data_len);
            return;
        }
        firmware_data.resize(size_bytes);
        memset(firmware_data.data(), 0xFF, size_bytes);
        memcpy(firmware_data.data(), data, data_len);
    }

    int perform_upgrade()
    {
        printf("Starting firmware upgrade for partition %s...\n", partition_name.c_str());
        int ret = send_start_upgrade();
        if (ret != 0)
        {
            return -1;
        }
        size_t offset = 0;
        while (offset * chunk_size < (size_t)size_bytes)
        {
            size_t chunk_len = ((size_t)size_bytes - offset * chunk_size) > chunk_size ? chunk_size : ((size_t)size_bytes - offset * chunk_size);
            ret = send_firmware_chunk((uint32_t)offset, &firmware_data[offset * chunk_size], (uint8_t)chunk_len);
            if (ret != 0)
            {
                return -1;
            }
            printf("\rProgress: %.2f%%", (float)(offset * chunk_size + chunk_len) * 100.0f / (float)size_bytes);
            fflush(stdout);
            offset++;
        }
        printf("\n");
        ret = send_finalize_upgrade(crc16);
        if (ret != 0)
        {
            return -1;
        }
        return 0;
    }

private:
    xlink_partition_type_t partition_type;
    string partition_name;
    uint32_t start_address;
    uint32_t size_bytes;
    xlink_context_p ctx;
    vector<uint8_t> firmware_data;

    const size_t chunk_size = 200;
    uint16_t crc16 = 0;

    int send_start_upgrade()
    {
        crc16 = XLINK_INIT_CRC16;
        int ret = -1;
        xlink_msg_handler_t handler_handle = xlink_register_msg_handler(ctx, XLINK_COMP_ID_UPGRADE, XLINK_UPGRADE_MSG_ID_START_FIRMWARE_UPGRADE_RESPONSE, [](uint8_t comp_id, uint8_t msg_id, const uint8_t *payload, uint8_t payload_len, void *user_data) -> int
                                                                        {
            (void)comp_id;
            (void)msg_id;
            if (payload_len != sizeof(xlink_upgrade_start_firmware_upgrade_response_t))
            {
                printf("Invalid start firmware upgrade response length: %u\n", payload_len);
                return -1;
            }
            const xlink_upgrade_start_firmware_upgrade_response_t *response = (const xlink_upgrade_start_firmware_upgrade_response_t *)payload;
            if (!response->accepted)
            {
                printf("Firmware upgrade request was rejected by the device\n");
                return -1;
            }
            printf("Firmware upgrade request accepted by the device\n");
            *(int *)user_data = 0;
            return 0; }, &ret);
        int wait_time = 500; // 500 * 10ms = 5s
        if (xlink_upgrade_start_firmware_upgrade_send(ctx, start_address, size_bytes, chunk_size) < 0)
        {
            printf("Failed to start firmware upgrade for partition %s\n", partition_name.c_str());
            goto __exit;
        }
        while (ret != 0 && wait_time-- > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (ret != 0)
        {
            printf("Timeout waiting for start firmware upgrade response for partition %s\n", partition_name.c_str());
            goto __exit;
        }
    __exit:
        xlink_unregister_msg_handler(ctx, XLINK_COMP_ID_UPGRADE, XLINK_UPGRADE_MSG_ID_START_FIRMWARE_UPGRADE_RESPONSE, handler_handle, &ret);
        return ret;
    }

    int send_firmware_chunk(uint32_t offset, const uint8_t *data, uint8_t data_len)
    {
        int ret = -1;
        xlink_msg_handler_t handler_handle = xlink_register_msg_handler(ctx, XLINK_COMP_ID_UPGRADE, XLINK_UPGRADE_MSG_ID_FIRMWARE_CHUNK_RESPONSE, [](uint8_t comp_id, uint8_t msg_id, const uint8_t *payload, uint8_t payload_len, void *user_data) -> int
                                                                        {
            (void)comp_id;
            (void)msg_id;
            if (payload_len != sizeof(xlink_upgrade_firmware_chunk_response_t))
            {
                printf("Invalid firmware chunk response length: %u\n", payload_len);
                return -1;
            }
            const xlink_upgrade_firmware_chunk_response_t *response = (const xlink_upgrade_firmware_chunk_response_t *)payload;
            if (!response->accepted)
            {
                printf("Firmware chunk at offset %u was rejected by the device\n", response->offset);
                return -1;
            }
            *(int *)user_data = 0;
            return 0; }, &ret);
        int wait_time = 500; // 500 * 10ms = 5s
        if (xlink_upgrade_firmware_chunk_send(ctx, offset, data, data_len) != 0)
        {
            printf("Failed to send firmware chunk at offset %u for partition %s\n", offset, partition_name.c_str());
            goto __exit;
        }
        while (ret != 0 && wait_time-- > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (ret != 0)
        {
            printf("Timeout waiting for firmware chunk response at offset %u for partition %s\n", offset, partition_name.c_str());
            goto __exit;
        }
    __exit:
        xlink_unregister_msg_handler(ctx, XLINK_COMP_ID_UPGRADE, XLINK_UPGRADE_MSG_ID_FIRMWARE_CHUNK_RESPONSE, handler_handle, &ret);
        if (ret == 0)
        {
            crc16 = xlink_crc16_with_init(data, data_len, crc16);
        }
        return ret;
    }

    int send_finalize_upgrade(uint32_t expected_crc32)
    {
        int ret = -1;
        xlink_msg_handler_t handler_handle = xlink_register_msg_handler(ctx, XLINK_COMP_ID_UPGRADE, XLINK_UPGRADE_MSG_ID_FINALIZE_FIRMWARE_UPGRADE_RESPONSE, [](uint8_t comp_id, uint8_t msg_id, const uint8_t *payload, uint8_t payload_len, void *user_data) -> int
                                                                        {
            (void)comp_id;
            (void)msg_id;
            if (payload_len != sizeof(xlink_upgrade_finalize_firmware_upgrade_response_t))
            {
                printf("Invalid finalize firmware upgrade response length: %u\n", payload_len);
                return -1;
            }
            const xlink_upgrade_finalize_firmware_upgrade_response_t *response = (const xlink_upgrade_finalize_firmware_upgrade_response_t *)payload;
            if (!response->success)
            {
                printf("Finalize firmware upgrade was rejected by the device\n");
                return -1;
            }
            printf("Firmware upgrade finalized successfully by the device\n");
            *(int *)user_data = 0;
            return 0; }, &ret);
        int wait_time = 500; // 500 * 10ms = 5s
        if (xlink_upgrade_finalize_firmware_upgrade_send(ctx, expected_crc32) != 0)
        {
            printf("Failed to finalize firmware upgrade for partition %s\n", partition_name.c_str());
            goto __exit;
        }
        while (ret != 0 && wait_time-- > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (ret != 0)
        {
            printf("Timeout waiting for finalize firmware upgrade response for partition %s\n", partition_name.c_str());
            goto __exit;
        }
    __exit:
        xlink_unregister_msg_handler(ctx, XLINK_COMP_ID_UPGRADE, XLINK_UPGRADE_MSG_ID_FINALIZE_FIRMWARE_UPGRADE_RESPONSE, handler_handle, &ret);
        return ret;
    }
};

int main(int argc, char *const *argv)
{
    static xlink_port_api_t tx_port = {
        .malloc_fn = xlink_stdlib_malloc,
        .free_fn = xlink_stdlib_free,
        .mutex_create_fn = xlink_posix_mutex_create,
        .mutex_delete_fn = xlink_posix_mutex_delete,
        .mutex_lock_fn = xlink_posix_mutex_lock,
        .mutex_unlock_fn = xlink_posix_mutex_unlock,
        .transport_send_fn = xlink_transport_send,
        .frame_send_alloc_fn = xlink_frame_send_alloc,
    };

    static const char short_options[] = "hd:f:s";
    static struct option long_options[] = {
        {"help", 0, 0, 'h'},
        {"device", 1, 0, 'd'},
        {"file", 1, 0, 'f'},
        {"show", 0, 0, 's'},
        {0, 0, 0, 0}};

    int c;
    int option_index = 0;
    string *device_path = nullptr;
    string *file_path = nullptr;
    bool is_show_info = false;
    int ret = 0;
    BootFromInfo_t boot_from_info;
    int firmware_fd;
    std::thread *rx_thread;
    xlink_partition_type_t target_partition;
    upgrade_partition *app_partition;
    upgrade_partition *bootfrom_partition;

    while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1)
    {
        switch (c)
        {
        case 'h':
            usage();
            return 0;
            break;
        case 'd':
            device_path = new string(optarg);
            break;
        case 'f':
            file_path = new string(optarg);
            break;
        case 's':
            is_show_info = true;
            break;
        default:
            usage();
            return -1;
            break;
        }
    }

    if (!is_show_info && (device_path == nullptr || file_path == nullptr))
    {
        usage();
        return -1;
    }

    int serial_fd = open(device_path->c_str(), O_RDWR | O_NOCTTY);
    if (serial_fd < 0)
    {
        printf("open %s failed\n", device_path->c_str());
        return 1;
    }
    serial_set_param(serial_fd, B115200);

    xlink_context_p ctx = xlink_context_create(&tx_port, (void *)(size_t)serial_fd);
    if (ctx == NULL)
    {
        printf("xlink context create failed\n");
        goto __close_serial;
    }
    signal(SIGINT, _close);

    rx_thread = new thread([&]()
                           {
        uint8_t rx_buffer[256];
        while (true)
        {
            ssize_t read_bytes = read(serial_fd, rx_buffer, sizeof(rx_buffer));
            if (read_bytes > 0)
            {
                for (ssize_t i = 0; i < read_bytes; i++)
                {
                    xlink_process_rx(ctx, rx_buffer[i]);
                }
            }
        } });
    rx_thread->detach();

    xlink_upgrade_firmware_info_t app_a_info;
    xlink_upgrade_firmware_info_t app_b_info;
    xlink_upgrade_firmware_info_t bootloader_info;
    ret |= get_mcu_firmware_version(ctx, XLINK_PARTITION_TYPE_BOOTLOADER, &bootloader_info);
    ret |= get_mcu_firmware_version(ctx, XLINK_PARTITION_TYPE_APP_A, &app_a_info);
    ret |= get_mcu_firmware_version(ctx, XLINK_PARTITION_TYPE_APP_B, &app_b_info);
    if (ret != 0)
    {
        printf("Failed to get firmware version\n");
        goto __free_ctx;
    }
    if (is_show_info)
    {
        goto __free_ctx;
    }
    target_partition = app_a_info.current_base_address == PARTITION_ADDRESS_APP_A
                           ? XLINK_PARTITION_TYPE_APP_B
                           : XLINK_PARTITION_TYPE_APP_A;
    firmware_fd = open(file_path->c_str(), O_RDONLY);
    if (firmware_fd < 0)
    {
        printf("open %s failed\n", file_path->c_str());
        goto __free_ctx;
    }
    app_partition = new upgrade_partition(target_partition, ctx, firmware_fd);
    ret = app_partition->perform_upgrade();
    if (ret != 0)
    {
        printf("Firmware upgrade failed\n");
        goto __close_frimware_fd;
    }
    delete app_partition;
    boot_from_info.magicNumber = PARTITION_MAGIC_NUMBER;
    boot_from_info.activeApp = target_partition == XLINK_PARTITION_TYPE_APP_A ? ACTIVE_APP_A : ACTIVE_APP_B;
    memset(boot_from_info.reserved, 0, sizeof(boot_from_info.reserved));
    boot_from_info.checksum = 0; // Assume checksum is calculated elsewhere
    bootfrom_partition = new upgrade_partition(
        PARTITION_ADDRESS_BOOTFROM,
        PARTITION_SIZE_BOOTFROM,
        "BOOTFROM",
        ctx,
        (const uint8_t *)&boot_from_info,
        sizeof(BootFromInfo_t));
    ret = bootfrom_partition->perform_upgrade();
    if (ret != 0)
    {
        printf("BootFrom partition upgrade failed\n");
        goto __close_frimware_fd;
    }

    printf("Firmware upgrade completed successfully\n");

__close_frimware_fd:
    close(firmware_fd);
__free_ctx:
    xlink_context_delete(ctx);
__close_serial:
    close(serial_fd);
    return 0;
}

static int get_mcu_firmware_version(xlink_context_p ctx, xlink_partition_type_t partition_type, xlink_upgrade_firmware_info_t *out_info)
{
    static xlink_msg_handler_t handler_handle = nullptr;
    int timeout = 100; // 100ms
    out_info->current_base_address = 0;
    handler_handle = xlink_register_msg_handler(ctx, XLINK_COMP_ID_UPGRADE, XLINK_UPGRADE_MSG_ID_FIRMWARE_INFO, [](uint8_t comp_id, uint8_t msg_id, const uint8_t *payload, uint8_t payload_len, void *user_data) -> int
                                                {
                                     (void)comp_id;
                                     (void)msg_id;
                                     string partition_str[] = {"BOOTLOADER", "APP_A", "APP_B"};
                                     if (payload_len != sizeof(xlink_upgrade_firmware_info_t))
                                     {
                                        printf("Invalid firmware info length: %u\n", payload_len);
                                        return -1;
                                     }
                                     const xlink_upgrade_firmware_info_t *info = (const xlink_upgrade_firmware_info_t *)payload;
                                        printf("\n==============================\n");
                                        printf("Partition Type: %s\n",info->partition_type < sizeof(partition_str)/sizeof(partition_str[0]) ? partition_str[info->partition_type].c_str() : "UNKNOWN");
                                        printf("Firmware Version: %u\n", info->version);
                                        printf("Firmware Size: %u bytes\n", info->size_bytes);
                                        printf("Commit Hash: 0x%08X\n", info->commit_hash);
                                        printf("Compile Timestamp: %u\n", info->compile_timestamp);
                                        printf("Current Base Address: 0x%08X\n", info->current_base_address);
                                        printf("==============================\n");
                                        if (user_data != nullptr)
                                        {
                                            xlink_upgrade_firmware_info_t *out = (xlink_upgrade_firmware_info_t *)user_data;
                                            memcpy(out, info, sizeof(xlink_upgrade_firmware_info_t));
                                        }
                                     return 0; }, out_info);
    if (handler_handle == nullptr)
    {
        return -1;
    }
    xlink_upgrade_get_firmware_info_send(ctx, partition_type);
    while (out_info->current_base_address == 0 && timeout > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        timeout--;
    }
    if (out_info->current_base_address == 0)
    {
        printf("Get firmware info timeout\n");
    }
    if (handler_handle)
    {
        xlink_unregister_msg_handler(ctx, XLINK_COMP_ID_UPGRADE, XLINK_UPGRADE_MSG_ID_FIRMWARE_INFO, handler_handle, out_info);
    }
    return out_info->current_base_address == 0 ? -1 : 0;
}

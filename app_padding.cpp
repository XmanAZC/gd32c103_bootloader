#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <vector>
#include "inc/partition.h"

using namespace std;

static void usage(void)
{
    printf(
        "Usage: bootloader-bin-path appa-bin-path appb-bin-path [params-bin-path]\n"
        "-s, --show             show bin file info\n"
        "-p, --params           params bin file path\n");
}

static void _close(int sig)
{
    (void)sig;
    printf("\nCtrl+c\n");
    exit(0);
}

struct padding_def
{
    string path;
    int fd;
    uint32_t size;
    vector<uint8_t> data;
};

int main(int argc, char *const *argv)
{
    static const char short_options[] = "hs:o:p:l:a:b:";
    struct tm tm;
    static struct option long_options[] = {
        {"help", 0, 0, 'h'},
        {"show", 1, 0, 's'},
        {"output", 1, 0, 'o'},
        {"params", 1, 0, 'p'},
        {"loader", 1, 0, 'l'},
        {"appa", 1, 0, 'a'},
        {"appb", 1, 0, 'b'},
        {0, 0, 0, 0}};

    int c;
    int option_index = 0;

    padding_def pads[] = {
        {.path = "", .size = PARTITION_SIZE_BOOTLOADER},
        {.path = "", .size = PARTITION_SIZE_BOOTFROM},
        {.path = "", .size = PARTITION_SIZE_APP_A_INFO},
        {.path = "", .size = PARTITION_SIZE_APP_A},
        {.path = "", .size = PARTITION_SIZE_APP_B_INFO},
        {.path = "", .size = PARTITION_SIZE_APP_B},
        {.path = "", .size = PARTITION_SIZE_PARAMS}};

    signal(SIGINT, _close);
    string output_file = "bootloader_full.bin";

    while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1)
    {
        switch (c)
        {
        case 'h':
            usage();
            return 0;
            break;
        case 's':

            break;
        case 'l':
            pads[0].path = optarg; // bootloader
            break;
        case 'a':
            pads[3].path = optarg; // appa
            break;
        case 'b':
            pads[5].path = optarg; // appb
            break;
        case 'o':
            output_file = optarg;
            break;
        case 'p':
            pads[6].path = optarg; // params
            break;
        default:
            break;
        }
    }

    if (pads[0].path.empty() || pads[3].path.empty() || pads[5].path.empty())
    {
        usage();
        return -1;
    }

    for (size_t i = 0; i < sizeof(pads) / sizeof(padding_def); i++)
    {
        pads[i].data.resize(pads[i].size, 0xFF);
        if (pads[i].path.empty())
        {
            continue;
        }
        pads[i].fd = open(pads[i].path.c_str(), O_RDONLY);
        if (pads[i].fd < 0)
        {
            printf("open %s failed\n", pads[i].path.c_str());
            return -1;
        }
        struct stat st;
        if (fstat(pads[i].fd, &st) < 0)
        {
            printf("fstat %s failed\n", pads[i].path.c_str());
            close(pads[i].fd);
            return -1;
        }
        if ((uint32_t)st.st_size > pads[i].size)
        {
            printf("%s size %ld exceed partition size %u\n", pads[i].path.c_str(), st.st_size, pads[i].size);
            close(pads[i].fd);
            return -1;
        }

        ssize_t read_bytes = read(pads[i].fd, pads[i].data.data(), st.st_size);
        if (read_bytes < 0 || (uint32_t)read_bytes != (uint32_t)st.st_size)
        {
            printf("read %s failed\n", pads[i].path.c_str());
            close(pads[i].fd);
            return -1;
        }
        close(pads[i].fd);
        printf("load %s size %ld bytes\n", pads[i].path.c_str(), read_bytes);
    }
    BootFromInfo_p bootfrom_info = (BootFromInfo_p)pads[1].data.data();
    bootfrom_info->magicNumber = PARTITION_MAGIC_NUMBER;
    bootfrom_info->activeApp = ACTIVE_APP_A;
    bootfrom_info->checksum = 0;
    // simple checksum: sum of all bytes
    uint32_t checksum = 0;
    for (size_t i = 0; i < pads[1].size - sizeof(bootfrom_info->checksum); i++)
    {
        checksum += pads[1].data[i];
    }
    bootfrom_info->checksum = checksum;

    AppInfo_p appa_info = (AppInfo_p)pads[2].data.data();
    appa_info->magicNumber = PARTITION_MAGIC_NUMBER;
    appa_info->version = 1;
    appa_info->size_bytes = pads[3].size;
    appa_info->commit_hash = 0;
    appa_info->app_checksum = crc32_calculate(pads[3].data.data(), pads[3].size, 0);
    time_t now = time(NULL);
    localtime_r(&now, &tm);
    appa_info->compile_timestamp = (uint32_t)mktime(&tm);

    AppInfo_p appb_info = (AppInfo_p)pads[4].data.data();
    appb_info->magicNumber = PARTITION_MAGIC_NUMBER;
    appb_info->version = 1;
    appb_info->size_bytes = pads[5].size;
    appb_info->commit_hash = 0;
    appb_info->app_checksum = crc32_calculate(pads[5].data.data(), pads[5].size, 0);
    appb_info->compile_timestamp = appa_info->compile_timestamp;

    int out_fd = open(output_file.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
    if (out_fd < 0)
    {
        printf("create %s failed\n", output_file.c_str());
        return -1;
    }
    for (size_t i = 0; i < sizeof(pads) / sizeof(padding_def); i++)
    {
        ssize_t write_bytes = write(out_fd, pads[i].data.data(), pads[i].size);
        if (write_bytes < 0 || (uint32_t)write_bytes != pads[i].size)
        {
            printf("write %s failed\n", output_file.c_str());
            close(out_fd);
            return -1;
        }
    }
    close(out_fd);
    printf("create %s success\n", output_file.c_str());

    return 0;
}

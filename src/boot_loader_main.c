#include "config.h"
#include "partition.h"

void main(void)
{
    BootFromInfo_p boot_from_info = (BootFromInfo_p)PARTITION_ADDRESS_BOOTFROM;
    jump_to_app(boot_from_info->activeApp);
    jump_to_app(!boot_from_info->activeApp);
    for (;;)
        ;
    return;
}

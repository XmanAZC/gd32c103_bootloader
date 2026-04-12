# GD32C103 AB分区
该项目是一个模板项目，实现了AB区升级的功能。

分区表详见：[分区表](./partition.md "分区表")

## 编译
```bash
git clone https://github.com/XmanAZC/gd32c103_ab.git
cd gd32c103_ab
git submodule update --init --recursive
```
下载交叉编译工具。
```bash
sudo apt update
sudo apt install build-essential gcc g++ cmake wget ninja-build usbutils
wget https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2
mkdir -p ~/software
tar xf gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2 -C ~/software/
```
配置交叉编译工具路径，设置环境变量`GCC_ARM_NONE_EABI_TOOLCHAIN_PATH`，示例如下：
```bash
export GCC_ARM_NONE_EABI_TOOLCHAIN_PATH=$HOME/software/gcc-arm-none-eabi-10.3-2021.10/bin
```
编译项目
```
./build.sh
```

使用jlink下载`build/gd32c103_ab.bin`到MCU中。

获取版本版本信息。
```bash
debian@phil:~/work/gd32c103_ab$ sudo build/upgrade -d /dev/ttyACM1 -s

==============================
Partition Type: BOOTLOADER
Firmware Version: 65537
Firmware Size: 0 bytes
Commit Hash: 0x12345678
Compile Timestamp: 305419896
Current Base Address: 0x08005000
==============================

==============================
Partition Type: APP_A
Firmware Version: 1
Firmware Size: 49152 bytes
Commit Hash: 0x00000000
Compile Timestamp: 1774586776
Current Base Address: 0x08005000
==============================

==============================
Partition Type: APP_B
Firmware Version: 1
Firmware Size: 49152 bytes
Commit Hash: 0x00000000
Compile Timestamp: 1774586776
Current Base Address: 0x08005000
==============================
debian@phil:~/work/gd32c103_ab$ 
```
升级MCU
```bash
debian@phil:~/work/gd32c103_ab$ sudo build/upgrade -d /dev/ttyACM1 -f build/gd32c103_ab.bin 
[sudo] password for debian: 

==============================
Partition Type: BOOTLOADER
Firmware Version: 65537
Firmware Size: 0 bytes
Commit Hash: 0x12345678
Compile Timestamp: 305419896
Current Base Address: 0x08005000
==============================

==============================
Partition Type: APP_A
Firmware Version: 1
Firmware Size: 49152 bytes
Commit Hash: 0x00000000
Compile Timestamp: 1774586776
Current Base Address: 0x08005000
==============================

==============================
Partition Type: APP_B
Firmware Version: 1
Firmware Size: 49152 bytes
Commit Hash: 0x00000000
Compile Timestamp: 1774586776
Current Base Address: 0x08005000
==============================
Starting firmware upgrade for partition APP_B...
Firmware upgrade request accepted by the device
Progress: 100.00%
Firmware upgrade finalized successfully by the device
Starting firmware upgrade for partition BOOTFROM...
Firmware upgrade request accepted by the device
Progress: 100.00%
Firmware upgrade finalized successfully by the device
Firmware upgrade completed successfully
Reset the device to boot into the new firmware
debian@phil:~/work/gd32c103_ab$ 
```
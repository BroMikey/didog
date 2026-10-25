# hello：在 Linux 上用 xmake 编译 STM32F103C8

本文以 `example/1-1 LED闪烁`（老师提供的 Keil MDK 工程）为参考，把工程迁移到 **Linux + xmake + GNU Arm 工具链**。

原来的 `xmake.lua` 用的是 `mdk.console` 和 `microlib`，这两者都只适用于 Windows 下的 Keil MDK（armcc/armclang），在 Linux 上必须换成 `gnu-rm`。

---

## 1. 芯片、库和工程结构

### 1.1 从 Keil 工程读出的关键参数

来源：`example/1-1 LED闪烁/Project.uvprojx`、`stm32f10x.h`、`system_stm32f10x.c`、`Project.sct`

| 项目 | 值 | 依据 |
|---|---|---|
| 芯片型号 | **STM32F103C8**（常见板子为 STM32F103C8T6） | `<Device>STM32F103C8</Device>` |
| 内核 | **Cortex-M3** | `CPUTYPE("Cortex-M3")` |
| Flash | **64 KB @ 0x08000000** | `IROM(0x08000000,0x10000)` |
| RAM | **20 KB @ 0x20000000** | `IRAM(0x20000000,0x5000)` |
| 外部晶振 | **8 MHz**（HSE） | `stm32f10x.h` 的 `HSE_VALUE` |
| 系统时钟 | **72 MHz** | `system_stm32f10x.c` 的 `SYSCLK_FREQ_72MHz` |
| 外设库 | **STM32F10x Standard Peripheral Library V3.5.0**（标准库/StdPeriph） | `Library/` |
| 启动文件 | **startup_stm32f10x_md.s**（中容量） | `Start/` |
| 预处理宏 | `USE_STDPERIPH_DRIVER`；GCC 下还必须加 `STM32F10X_MD` | `<Define>` + `stm32f10x.h` |
| 头文件路径 | `Start`、`Library`、`User`、`System` | `<IncludePath>` |
| 编译工具 | ARMCC V5.06 update 5 | `<pCCUsed>` |
| 输出 | `.axf` + `.hex` | `<CreateHexFile>1</CreateHexFile>` |
| 分散加载 | Flash 0x08000000 大小 0x10000；RAM 0x20000000 大小 0x5000 | `Project.sct` |

> `Project.uvprojx` 里的 `CLOCK(12000000)` 是 Keil 调试/仿真用的 XTAL 设置，不是代码里的 HSE。代码实际按 `HSE_VALUE = 8000000` 配置；如果板子晶振不是 8 MHz，请同步修改 `HSE_VALUE`。

> **结论：** 芯片是 STM32F103C8（Cortex-M3，64KB Flash，20KB RAM），使用的是 **STM32F10x 标准外设库**，不是 HAL 库。
>
> `Project.uvprojx` 的 `<Define>` 里只写了 `USE_STDPERIPH_DRIVER`，但没有显式写 `STM32F10X_MD`。GCC 编译时必须在 `xmake.lua` 里补上，否则 `stm32f10x.h` 会报：
>
> ```text
> #error "Please select first the target STM32F10x device used in your application"
> ```

### 1.2 老师工程各目录的作用

| 目录 | 内容 | Linux 下是否保留 |
|---|---|---|
| `Start/` | `core_cm3.c/h`、`stm32f10x.h`、`system_stm32f10x.c/h`、各种 Keil 启动文件 | 保留，但启动文件要换成 GCC 版 |
| `Library/` | STM32F10x 标准外设库全部 `.c/.h` | 直接用 |
| `User/` | `main.c`、`stm32f10x_conf.h`、`stm32f10x_it.c/h` | 直接用 |
| `System/` | `Delay.c/h`（SysTick 延时） | 直接用 |
| `Objects/`、`Listings/`、`DebugConfig/` | Keil 编译产物和调试配置 | 不用 |
| `Project.uvprojx`、`Project.uvoptx` | Keil 工程文件 | 不用，由 `xmake.lua` 代替 |
| `Project.sct` | Keil 分散加载脚本 | 换成 GCC 链接脚本 `.ld` |
| `keilkill.bat` | 清理 Keil 产物 | 用 `xmake clean` 代替 |

### 1.3 不能直接用的文件

`Start/startup_stm32f10x_md.s` 是 **ARMCC/armasm 语法**（`AREA`、`EXPORT`、`ENDFUNC`），GNU `as` 无法编译，会报一堆 `bad instruction`。

Linux 下需要一份 **GNU 语法** 的同名启动文件，例如：

- STM32F10x 标准库 V3.5.0：
  `Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/startup/gcc_ride7/startup_stm32f10x_md.s`
- 或 STM32CubeF1：
  `Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/gcc/startup_stm32f103xb.s`

把它复制到工程里并命名为：

```text
Start/startup_stm32f10x_md_gcc.s
```

`.s` 文件里应该有 `.syntax unified`、`.thumb`、`.section .isr_vector`、`Reset_Handler` 等 GNU 语法；如果看到 `AREA`、`EXPORT`，那就是 Keil 版本，不能用。

---

## 2. 需要安装的工具及替代关系

### 2.1 工具对照表

| 老师 Windows 工具 | 作用 | Linux 替代 | 安装方式 |
|---|---|---|---|
| Keil MDK / ARMCC V5.06 | 编译、汇编、链接 | `arm-none-eabi-gcc`（xmake 的 `gnu-rm` 工具链） | `pacman -S arm-none-eabi-gcc` |
| Keil uVision 工程（`.uvprojx`） | 工程配置 | `xmake.lua` | 已安装 xmake |
| ARMCC 启动文件 `startup_*.s` | 中断向量表、复位入口 | GNU 语法的 `startup_*_gcc.s` | 从标准库 `gcc_ride7/` 复制 |
| `fromelf` / CreateHexFile | `.axf` 转 `.hex` | `arm-none-eabi-objcopy` | 随 `arm-none-eabi-binutils` |
| `Project.sct` 分散加载 | 指定 Flash/RAM 地址 | GCC 链接脚本 `.ld` | 手写，见第 3.3 节 |
| **FlyMcu** | 串口 ISP 烧录 | **stm32flash**、**stm32loader**、**STM32CubeProgrammer** | AUR / pip / ST 官网 |
| **CH340 驱动** | USB 转串口 | Linux 内核自带 `ch341` | 无需安装 |
| **串口助手 V1.1** | 串口调试 | **picocom**、**minicom** | `pacman -S picocom minicom` |
| ULINK / J-Link | 调试、下载 | **OpenOCD**、**stlink**、**probe-rs** | `pacman -S openocd stlink probe-rs` |
| `keilkill.bat` | 清理编译产物 | `xmake clean --all` | 已安装 xmake |
| `microlib` | ARMCC 精简 C 库 | **newlib-nano**（`--specs=nano.specs`） | `gnu-rm` 工具链自动添加 |

### 2.2 Arch Linux 安装命令

```bash
# 交叉编译器（gcc 会依赖 binutils/newlib，一起装更稳妥）
sudo pacman -S arm-none-eabi-gcc arm-none-eabi-binutils arm-none-eabi-newlib

# 串口调试
sudo pacman -S picocom minicom

# 可选：用 ST-Link / J-Link / DAP-Link 下载调试
sudo pacman -S openocd stlink probe-rs

# 串口权限：把自己加入 uucp 组，重新登录后生效
sudo usermod -aG uucp "$USER"
```

验证：

```bash
arm-none-eabi-gcc --version
xmake show -l toolchains | grep gnu-rm
```

### 2.3 FlyMcu 的 Linux 替代

老师用的 FlyMcu 走的是 **STM32 片内 System Bootloader（串口 ISP）**，不是 ST-Link。Linux 下可以用：

```bash
# 方式一：stm32flash（Arch 官方仓库没有，走 AUR）
yay -S stm32flash
# 或从源码编译：https://github.com/stm32flash/stm32flash

# 方式二：stm32loader（Python，安装简单）
pip install stm32loader

# 方式三：ST 官方 STM32CubeProgrammer（提供 Linux 版）
# 从 ST 官网下载 .deb/.rpm，或解压后运行 bin/STM32CubeProgrammer
```

使用串口 ISP 的步骤和 FlyMcu 一样：

1. 板子 **BOOT0 = 1**，按一下复位，进入 System Bootloader；
2. 用 `/dev/ttyUSB0`（CH340）连接，115200 8N1；
3. 烧录 `.hex` 或 `.bin` 到 `0x08000000`；
4. 烧录完成后 **BOOT0 = 0**，再复位，运行用户程序。

### 2.4 CH340 驱动

Linux 内核已经自带 `ch341` 模块，**不需要安装 Windows 驱动**。

```bash
# 确认模块
lsmod | grep ch341
sudo modprobe ch341

# 插上板子后查看设备
dmesg | tail
ls -l /dev/ttyUSB*

# 如果提示权限不足，确认当前用户在 uucp 组
groups
```

### 2.5 串口助手替代

```bash
# picocom（推荐，轻量）
picocom -b 115200 /dev/ttyUSB0
# 退出：Ctrl-A 然后 Ctrl-X

# minicom
minicom -D /dev/ttyUSB0 -b 115200

# 也可以直接用 screen（需安装 screen）
screen /dev/ttyUSB0 115200
# 退出：Ctrl-A 然后 K
```

图形界面的串口工具（可选）：`cutecom`、`gtkterm`、`moserial`，部分在 AUR。

### 2.6 可选：让 xmake 自动下载 gnu-rm

除了用 `pacman` 安装系统工具链，也可以让 xmake 从 xmake-repo 自动拉取 **gnu-rm 工具链包**（`package("gnu-rm")` 的 `set_kind` 是 `"toolchain"`）。这样不需要 `sudo`，也能锁定工具链版本：

```lua
add_requires("gnu-rm 2024.12")
set_toolchains("@gnu-rm")
```

`@gnu-rm` 的写法会在安装好的 `gnu-rm` 包里查找 `arm-none-eabi-gcc`，然后绑定到内置的 `gnu-rm` 工具链。

此时配置命令不需要再写 `--toolchain=gnu-rm`：

```bash
xmake f -p cross -a cortex-m3 -c
xmake -y
```

> 第一次配置会下载约 100 MB 左右的工具链，耗时取决于网络。若本机已经用 `pacman` 装了 `arm-none-eabi-gcc`，用系统工具链更省事。

---

## 3. Linux 工程配置与 xmake.lua

### 3.1 复制老师工程里的必要文件

在 `hello/` 目录下执行：

```bash
cd hello

cp -r "example/1-1 LED闪烁/User"    .
cp -r "example/1-1 LED闪烁/Start"   .
cp -r "example/1-1 LED闪烁/Library" .
cp -r "example/1-1 LED闪烁/System"  .
```

复制后目录大致是：

```text
hello/
├── xmake.lua
├── src/                    # 原来的 hello world（可删，或按 3.4 节二选一）
├── User/
│   ├── main.c
│   ├── stm32f10x_conf.h
│   ├── stm32f10x_it.c
│   └── stm32f10x_it.h
├── Start/
│   ├── core_cm3.c / core_cm3.h
│   ├── stm32f10x.h
│   ├── system_stm32f10x.c / system_stm32f10x.h
│   ├── startup_stm32f10x_md.s          # ARMCC 语法，GCC 不用
│   └── startup_stm32f10x_md_gcc.s      # 需要自己放入，见 3.2
├── Library/                # STM32F10x 标准外设库
├── System/
│   ├── Delay.c
│   └── Delay.h
└── linker/
    └── stm32f103c8.ld      # 需要自己创建，见 3.3
```

创建链接脚本目录：

```bash
mkdir -p linker
```

### 3.2 放入 GCC 启动文件

把第 1.3 节提到的 `startup_stm32f10x_md.s`（`gcc_ride7` 目录里的那份）复制为：

```text
Start/startup_stm32f10x_md_gcc.s
```

> 注意：不要使用 `example/1-1 LED闪烁/Start/startup_stm32f10x_md.s`，那是 ARMCC 语法。
>
> 如果手头没有标准库压缩包，也可以到 STM32CubeF1 的 `Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/gcc/` 里找 `startup_stm32f103xb.s`，改名为 `Start/startup_stm32f10x_md_gcc.s` 即可（STM32F103C8 属于中容量，和 xB 启动文件兼容）。

### 3.3 创建 GCC 链接脚本

新建 `linker/stm32f103c8.ld`，内容和老师的 `Project.sct` 对应：

- Flash：`0x08000000`，长度 `64K`
- RAM：`0x20000000`，长度 `20K`
- `_estack` 放在 RAM 末尾
- 定义 `_sidata/_sdata/_edata/_sbss/_ebss`，GCC 启动文件会用到

```ld
/* STM32F103C8: 64KB Flash @ 0x08000000, 20KB RAM @ 0x20000000 */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > FLASH

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > FLASH

    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > RAM

    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + 0x200;   /* heap */
        . = . + 0x400;   /* 预留给栈，栈顶仍是 _estack */
        . = ALIGN(8);
    } > RAM
}
```

### 3.4 重写 `xmake.lua`

用下面的内容替换 `hello/xmake.lua`。原来的 `mdk.console`、`set_runtimes("microlib")` 全部删掉：

```lua
-- 可选：不安装系统工具链，而是让 xmake 自动下载 gnu-rm
-- add_requires("gnu-rm 2024.12")
-- set_toolchains("@gnu-rm")

add_rules("mode.debug", "mode.release")

target("hello")
    -- 使用 GNU Arm 规则：输出 .elf，并关闭自动 flag 检测
    add_rules("gnu-rm.binary")

    -- 应用代码：二选一
    -- 方案 A：使用老师工程结构（User/main.c）
    add_files("User/*.c")
    -- 方案 B：保留原来的 src/main.c
    -- add_files("src/*.c", "User/stm32f10x_it.c")

    -- 系统文件、外设库、延时
    add_files("Start/core_cm3.c", "Start/system_stm32f10x.c")
    add_files("Library/*.c")
    add_files("System/*.c")

    -- GCC 语法的启动文件（不能用 Keil 的 startup_stm32f10x_md.s）
    add_files("Start/startup_stm32f10x_md_gcc.s")

    -- 头文件路径，对应 Keil 的 IncludePath
    add_includedirs("Start", "Library", "User", "System")

    -- 对应 Keil 的 Define：
    --   STM32F10X_MD          中容量（STM32F103C8，64KB Flash）
    --   USE_STDPERIPH_DRIVER  启用标准外设库
    add_defines(
        "STM32F10X_MD",
        "USE_STDPERIPH_DRIVER",
        "HSE_VALUE=8000000"
    )

    -- Cortex-M3 编译参数
    add_cxflags(
        "-mcpu=cortex-m3",
        "-mthumb",
        "-ffunction-sections",
        "-fdata-sections",
        {force = true}
    )
    add_asflags("-mcpu=cortex-m3", "-mthumb", {force = true})

    -- 链接参数：
    --   -T                     指定 GCC 链接脚本
    --   --gc-sections          删除未用代码/数据
    --   -Map                   生成 map 文件
    -- 注意：gnu-rm 工具链已自动加 --specs=nano.specs / --specs=nosys.specs
    add_ldflags(
        "-T", path.join(os.projectdir(), "linker/stm32f103c8.ld"),
        "-Wl,--gc-sections",
        "-Wl,-Map=hello.map",
        {force = true}
    )

    -- 编译完成后自动生成 .hex 和 .bin（替代 Keil 的 CreateHexFile/fromelf）
    after_build(function (target)
        local elf = target:targetfile()
        local dir = target:targetdir()
        local hex = path.join(dir, target:name() .. ".hex")
        local bin = path.join(dir, target:name() .. ".bin")
        os.execv("arm-none-eabi-objcopy", {"-O", "ihex", elf, hex})
        os.execv("arm-none-eabi-objcopy", {"-O", "binary", elf, bin})
        os.execv("arm-none-eabi-size", {elf})
    end)
```

> **关于 `src/main.c` 和 `User/main.c`**
>
> 一个工程只能有一个 `main`。老师工程里的 `User/main.c` 目前是空的，需要把 `shili.txt` 里的 LED 闪烁代码填进去，或者直接使用你原来的 `src/main.c`。如果使用 `src/main.c`，请把上面的 `add_files("User/*.c")` 换成：
>
> ```lua
> add_files("src/*.c", "User/stm32f10x_it.c")
> ```
>
> 否则会出现 `multiple definition of 'main'`。

一个可用的 `User/main.c` 示例（PC13 LED 闪烁，基于 `shili.txt`）：

```c
#include "stm32f10x.h"
#include "Delay.h"

int main(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    while (1)
    {
        GPIO_ResetBits(GPIOC, GPIO_Pin_13);
        Delay_ms(500);
        GPIO_SetBits(GPIOC, GPIO_Pin_13);
        Delay_ms(500);
    }
}
```

> 原来的 `src/main.c` 里的 `printf("hello world!\n")` 在 STM32 上默认不会输出到任何地方（`nosys` 的 `_write` 是空实现）。如果要用串口打印，需要自己重定向 `_write` 到 USART。

### 3.5 编译

**方式 A：使用系统安装的 `arm-none-eabi-gcc`**

```bash
cd hello

# 清掉旧的 Linux 主机配置，重新配置为交叉编译
# --cross=arm-none-eabi- 告诉 xmake 去 /usr/bin 找 arm-none-eabi-gcc
xmake f -p cross -a cortex-m3 --toolchain=gnu-rm --cross=arm-none-eabi- -c

# 编译
xmake -y
```

> 如果 `arm-none-eabi-gcc` 不在 `/usr/bin`（例如手动解压到 `~/tools/gcc-arm-none-eabi`），把 `--cross=arm-none-eabi-` 换成：
>
> ```bash
> xmake f -p cross -a cortex-m3 --toolchain=gnu-rm \
>     --sdk=$HOME/tools/gcc-arm-none-eabi -c
> ```
>
> 或者用 `--bin=$HOME/tools/gcc-arm-none-eabi/bin` 指定 bin 目录。

**方式 B：使用 `add_requires("gnu-rm")` 自动下载的工具链**

先在 `xmake.lua` 顶部取消这两行的注释：

```lua
add_requires("gnu-rm 2024.12")
set_toolchains("@gnu-rm")
```

然后：

```bash
xmake f -p cross -a cortex-m3 -c
xmake -y
```

参数说明：

| 参数 | 含义 |
|---|---|
| `-p cross` | 交叉编译平台 |
| `-a cortex-m3` | CPU 架构，STM32F103C8 是 Cortex-M3 |
| `--toolchain=gnu-rm` | 使用 `arm-none-eabi-gcc` |
| `--cross=arm-none-eabi-` | 交叉工具前缀；系统安装的 `arm-none-eabi-gcc` 在 `/usr/bin` 时必须指定 |
| `-c` | 重新配置（清理旧的 platform/toolchain 缓存） |

成功后会生成：

```text
build/cross/cortex-m3/release/hello.elf
build/cross/cortex-m3/release/hello.hex
build/cross/cortex-m3/release/hello.bin
```

其中：

- `.elf`：带调试信息的完整固件，OpenOCD/GDB 用；
- `.hex`：带地址信息，FlyMcu/stm32flash 用；
- `.bin`：纯二进制，烧录时必须指定起始地址 `0x08000000`。

清理：

```bash
xmake clean          # 清理编译产物
xmake clean --all    # 连同配置一起清理
```

---

## 4. 烧录

### 4.1 串口 ISP（FlyMcu 的等价方式）

以 `stm32flash` 为例：

```bash
# BOOT0=1，复位进入 System Bootloader
stm32flash -b 115200 -w build/cross/cortex-m3/release/hello.hex -v -g 0x08000000 /dev/ttyUSB0

# 烧录完成后 BOOT0=0，再复位
```

`stm32loader`：

```bash
stm32loader -p /dev/ttyUSB0 -b 115200 -e -w -v build/cross/cortex-m3/release/hello.bin
```

### 4.2 ST-Link（如果板子有 SWD 接口）

```bash
# stlink 工具
st-flash write build/cross/cortex-m3/release/hello.bin 0x08000000
st-flash reset

# 或 OpenOCD
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
    -c "program build/cross/cortex-m3/release/hello.elf verify reset exit"
```

---

## 5. 常见问题

| 现象 | 原因与解决 |
|---|---|
| `error: target(hello): toolchain not found!` | 没装 `arm-none-eabi-gcc`；或没加 `--toolchain=gnu-rm --cross=arm-none-eabi-` |
| `arm-none-eabi-gcc: command not found` | 安装 `arm-none-eabi-gcc`，确认在 `PATH` 中 |
| `#error "Please select first the target STM32F10x device..."` | 缺少 `STM32F10X_MD`，检查 `add_defines` |
| `undefined reference to '_estack'` / `_sdata` | 链接脚本没传进去，检查 `-T linker/stm32f103c8.ld` |
| `multiple definition of 'main'` | `src/main.c` 和 `User/main.c` 同时被编译，二选一 |
| `bad instruction` / `unexpected token` 在 `startup_*.s` | 用了 ARMCC 启动文件，换成 GNU 版 `startup_*_gcc.s` |
| `region RAM overflowed` | 芯片型号或链接脚本 RAM 大小不对；检查是否误用了大容量型号的脚本 |
| `undefined reference to '_write'` / `_sbrk` | 用了 `printf`/`malloc`；`gnu-rm` 已带 `nosys.specs`，一般不会出现；自定义串口输出时实现 `_write` |
| Keil 能编译、GCC 报错 | 旧标准库对 GCC 的兼容问题，先确认 `core_cm3.c` 是带 `__GNUC__` 分支的那份；必要时降低优化或加 `-Wno-...` |

---

## 6. 一句话总结

| 项目 | Keil（老师） | Linux（本工程） |
|---|---|---|
| 编译器 | ARMCC V5.06 | `arm-none-eabi-gcc` + `gnu-rm` |
| 工程文件 | `Project.uvprojx` | `xmake.lua` |
| 启动文件 | `startup_stm32f10x_md.s`（armasm） | `startup_stm32f10x_md_gcc.s`（GNU as） |
| 链接脚本 | `Project.sct` | `linker/stm32f103c8.ld` |
| 精简 C 库 | `microlib` | `newlib-nano`（自动） |
| 生成 hex | Keil CreateHexFile / fromelf | `arm-none-eabi-objcopy` |
| 烧录 | FlyMcu | `stm32flash` / `stm32loader` / STM32CubeProgrammer |
| 串口助手 | 串口助手 V1.1 | `picocom` / `minicom` |
| CH340 驱动 | Windows 驱动 | 内核 `ch341`，无需安装 |

按第 3 节配置完成后，执行：

```bash
# 系统工具链
xmake f -p cross -a cortex-m3 --toolchain=gnu-rm --cross=arm-none-eabi- -c
xmake -y

# 或者使用 xmake 自动下载的 gnu-rm
# xmake f -p cross -a cortex-m3 -c
# xmake -y
```

即可得到 `hello.elf`、`hello.hex`、`hello.bin`。

---

## 7. 从零创建新项目

### 7.1 启动文件从哪里来

启动文件必须和芯片型号、Flash 容量、中断向量表匹配。STM32F103C8 属于**中容量（Medium Density）**，常用来源：

| 来源 | 路径 | 说明 |
|---|---|---|
| STM32F10x StdPeriph V3.5.0 | `Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/startup/gcc_ride7/startup_stm32f10x_md.s` | 和老师工程最匹配 |
| STM32CubeF1 | `Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/gcc/startup_stm32f103xb.s` | Cube 命名，xB 与 x8 共用中容量向量表 |
| STM32CubeMX | 生成 Makefile 工程时自动带 `startup_stm32f103xb.s` | 最省事，见 7.2 |
| `stm32-devel` addon | `cmsis-device-f1` 包里的 `Source/Templates/gcc/startup_stm32f103xb.s` | 由 addon 自动加入，见 7.3 |

> 老师给的 `example/1-1 LED闪烁/Start/` 里只有 ARMCC 版本（`AREA`/`EXPORT` 语法），**没有 GCC 版本**。GCC 版需要从标准库压缩包的 `gcc_ride7/` 目录拿，或者用 CubeMX/CubeF1 生成。

选择规则：

- STM32F103C8/CB → `startup_stm32f10x_md.s`（StdPeriph）或 `startup_stm32f103xb.s`（Cube/CMSIS）
- STM32F103RB/RC/RD/RE → 对应中/大容量启动文件
- **不要**把 Keil 的 `startup_stm32f10x_md.s`（`AREA/EXPORT` 语法）直接给 GCC 用
- 不确定时，优先用 CubeMX 为具体型号生成的启动文件

### 7.2 链接脚本从哪里来

三种常用方式：

1. **STM32CubeMX 生成（推荐新手）**
   - 新建 STM32F103C8 工程 → Project Manager → Toolchain/IDE 选 `Makefile`
   - 生成目录里会有 `STM32F103C8Tx_FLASH.ld`，可以直接用
   - 链接脚本只描述 Flash/RAM 布局，和 HAL/StdPeriph 无关，可以复用

2. **从官方例程改**
   - STM32CubeF1 的 `Projects/.../Templates/.../*.ld`
   - StdPeriph V3.5.0 的 `Project/STM32F10x_StdPeriph_Template/TrueSTUDIO/` 里的 `.ld`
   - 把 `MEMORY` 里的 `LENGTH` 改成 64K/20K 即可

3. **照着 Keil `.sct` 手写**
   - 老师的 `Project.sct` 已经给出地址和大小：
     - Flash：`0x08000000`，`0x10000`（64K）
     - RAM：`0x20000000`，`0x5000`（20K）
   - 本 README 第 3.3 节的 `linker/stm32f103c8.ld` 就是按这个写好的，可以直接复用

链接脚本必须提供启动文件用到的符号：

- `_estack`：栈顶
- `_sidata`、`_sdata`、`_edata`：`.data` 的加载/运行地址
- `_sbss`、`_ebss`：`.bss` 起止
- `ENTRY(Reset_Handler)` 和 `KEEP(*(.isr_vector))`

换芯片时，通常只需要改 `MEMORY` 里的 Flash/RAM 大小。

### 7.3 不用每次手写 `xmake.lua`

有三种复用方式。

#### 方式 A：官方 `stm32-devel` addon（最省事）

这个 addon 提供 STM32 的 toolchain、build rule 和项目模板，启动文件、链接脚本、`gnu-rm` 工具链都会自动处理。

```bash
# 安装 addon（只需一次）
xmake addon -y --install stm32-devel

# 用模板创建项目
xmake create -l c -t stm32.blink demo
cd demo

# bluepill = STM32F103C8
xmake f --board=bluepill -c
xmake
```

生成的 `xmake.lua` 只有几行：

```lua
add_rules("mode.debug", "mode.release")

add_addons("stm32-devel")
includes("@addon/stm32-devel/board")

target("demo")
    add_rules("@addon/stm32-devel/app")
    add_files("src/*.c")
    add_includedirs("boards/$(board)")
```

addon 会自动完成：

- 通过 `add_requires("gnu-rm")` 下载 ARM 工具链
- 下载 `cmsis-core`、`cmsis-device-f1`，提供 `stm32f1xx.h`、`SystemInit`、`startup_stm32f103xb.s`
- 根据 `board` 自动生成链接脚本（Flash/RAM 大小、堆栈）
- 生成 `.elf` / `.bin` / `.hex`，并打印 size
- `xmake install` 通过 OpenOCD 烧录；`xmake run` 烧录后打开串口监视器

支持的板子：`bluepill`、`blackpill`、`f407ve`。常用配置：

```bash
xmake f --board=bluepill --flash_size=64K --ram_size=20K -c
xmake f --probe=cmsis-dap -c   # 使用 DAPLink
```

> 注意 1：`stm32-devel` 用的是 **CMSIS 设备支持**（`stm32f1xx.h`），不是老师工程里的 **StdPeriph**（`stm32f10x.h`）。如果课程代码必须用标准库，请走第 3 节的手写方案，不要混用两套头文件。
>
> 注意 2：`xmake install` 走 SWD（ST-Link/DAPLink），不是 FlyMcu 的串口 ISP。如果只有 CH340，先用 `xmake` 生成 `.hex/.bin`，再按第 4.1 节用 `stm32flash` 烧录。
>
> 注意 3：addon 的 bluepill 模板默认使用内部 HSI 8 MHz（见 `boards/bluepill/board.h` 的 `HSI_HZ`）；老师工程是 HSE 8 MHz 经 PLL 到 72 MHz。需要 72 MHz 时要自己配置 PLL 或替换 `system_stm32f1xx.c` 的时钟配置。
>
> 模板会生成 `xmake-addons.lock`，用于锁定 addon 版本，建议一起提交到 git。

#### 方式 B：做一个全局模板

把第 3 节的工程整理成模板，放到：

```text
~/.xmake/templates/c/stm32-stdperiph/
├── xmake.lua
├── src/main.c
├── Start/
├── Library/
├── System/
└── linker/stm32f103c8.ld
```

以后每次：

```bash
xmake create -l c -t stm32-stdperiph myproj
cd myproj
xmake f -p cross -a cortex-m3 --toolchain=gnu-rm --cross=arm-none-eabi- -c
xmake
```

模板里的 `${TARGET_NAME}` 会被替换成项目名（`xmake.lua` 和 `.c/.h` 等源文件都会处理）。

`xmake create --list -l c` 里能看到 `stm32-stdperiph`。

#### 方式 C：直接复制工程目录

最直接：

```bash
cp -r hello myproj
cd myproj
rm -rf build .xmake
# 按需修改 xmake.lua 里的 target 名
xmake f -p cross -a cortex-m3 --toolchain=gnu-rm --cross=arm-none-eabi- -c
xmake
```

### 7.4 怎么选

| 场景 | 建议 |
|---|---|
| 新项目，不限定库 | `stm32-devel` addon + CMSIS |
| 必须沿用老师的 StdPeriph 代码 | 第 3 节手写方案，或做成全局模板 |
| 同一芯片要做很多练习 | 全局模板 `~/.xmake/templates/c/...` |
| 只想快速复制 | 直接复制现有工程目录 |

一句话：**启动文件和链接脚本优先用 CubeMX/CubeF1 官方生成；`xmake.lua` 用 `stm32-devel` addon 或自己的全局模板复用，不需要每次从零写。**

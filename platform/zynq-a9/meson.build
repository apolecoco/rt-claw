# SPDX-License-Identifier: MIT
# Zynq-A9 QEMU platform: FreeRTOS kernel + Xilinx BSP + rt-claw → ELF

bsp_root = project_root / 'vendor' / 'bsp' / 'xilinx'
freertos_root = project_root / 'vendor' / 'os' / 'freertos'
platform_dir = project_root / 'platform' / 'zynq-a9'

# ---- Xilinx BSP ----

bsp_incs = include_directories(
    '../../vendor/bsp/xilinx',
    '../../vendor/bsp/xilinx/standalone/common',
    '../../vendor/bsp/xilinx/standalone/cortexa9',
    '../../vendor/bsp/xilinx/drivers/scugic',
    '../../vendor/bsp/xilinx/drivers/scutimer',
)

bsp_src = files(
    '../../vendor/bsp/xilinx/standalone/common/xil_assert.c',
    '../../vendor/bsp/xilinx/standalone/common/xil_printf.c',
    '../../vendor/bsp/xilinx/standalone/common/xil_exception.c',
    '../../vendor/bsp/xilinx/standalone/cortexa9/xil_cache.c',
    '../../vendor/bsp/xilinx/standalone/cortexa9/xil_mmu.c',
    '../../vendor/bsp/xilinx/drivers/scugic/xscugic.c',
    '../../vendor/bsp/xilinx/drivers/scugic/xscugic_hw.c',
    '../../vendor/bsp/xilinx/drivers/scugic/xscugic_g.c',
    '../../vendor/bsp/xilinx/drivers/scugic/xscugic_sinit.c',
    '../../vendor/bsp/xilinx/drivers/scugic/xscugic_intr.c',
    '../../vendor/bsp/xilinx/drivers/scutimer/xscutimer.c',
    '../../vendor/bsp/xilinx/drivers/scutimer/xscutimer_g.c',
    '../../vendor/bsp/xilinx/drivers/scutimer/xscutimer_sinit.c',
)

bsp_asm = []

bsp_lib = static_library('xilinx_bsp',
    bsp_src, bsp_asm,
    include_directories: bsp_incs,
    c_args: ['-DARMR5', '-w'],
)

# ---- FreeRTOS Kernel (ARM_CA9 port) ----

freertos_incs = include_directories(
    '../../vendor/os/freertos/include',
    '../../vendor/os/freertos/portable/GCC/ARM_CA9',
    '.',
)

freertos_src = files(
    '../../vendor/os/freertos/tasks.c',
    '../../vendor/os/freertos/queue.c',
    '../../vendor/os/freertos/list.c',
    '../../vendor/os/freertos/timers.c',
    '../../vendor/os/freertos/event_groups.c',
    '../../vendor/os/freertos/stream_buffer.c',
    '../../vendor/os/freertos/portable/GCC/ARM_CA9/port.c',
    '../../vendor/os/freertos/portable/GCC/ARM_CA9/portASM.S',
    '../../vendor/os/freertos/portable/MemMang/heap_4.c',
)

freertos_lib = static_library('freertos',
    freertos_src,
    include_directories: [freertos_incs, bsp_incs],
    c_args: ['-w'],
)

# ---- Platform sources ----

platform_src = files(
    'startup.S',
    'FreeRTOS_asm_vectors.S',
    'main.c',
    'syscalls.c',
    'boards/qemu/board.c',
)

# ---- Link everything into rtclaw.elf ----

rtclaw_elf = executable('rtclaw',
    platform_src,
    include_directories: [
        freertos_incs,
        bsp_incs,
        claw_inc,
        claw_root_inc,
    ],
    c_args: platform_defs + ['-DCLAW_PLATFORM_FREERTOS'],
    link_with: [rtclaw_lib, osal_lib, cjson_lib, bsp_lib, freertos_lib],
    link_args: [
        '-T', platform_dir / 'link.ld',
        '-Wl,--gc-sections',
        '-nostartfiles',
        '-Xlinker', '--defsym=_HEAP_SIZE=0x80000',
        '-Xlinker', '--defsym=_STACK_SIZE=0x4000',
    ],
    name_suffix: 'elf',
)

# ---- Unit test ELF ----

test_src = files(
    'startup.S',
    'FreeRTOS_asm_vectors.S',
    'test_main.c',
    'syscalls.c',
)

test_incs = include_directories(
    '../..',
)

rtclaw_test_elf = executable('rtclaw_test',
    test_src,
    include_directories: [
        freertos_incs,
        bsp_incs,
        claw_inc,
        claw_root_inc,
        test_incs,
    ],
    c_args: platform_defs + ['-DCLAW_PLATFORM_FREERTOS'],
    link_with: [unittest_lib, rtclaw_lib, osal_lib, cjson_lib,
                bsp_lib, freertos_lib],
    link_args: [
        '-T', platform_dir / 'link.ld',
        '-Wl,--gc-sections',
        '-nostartfiles',
        '-Xlinker', '--defsym=_HEAP_SIZE=0x80000',
        '-Xlinker', '--defsym=_STACK_SIZE=0x4000',
    ],
    name_suffix: 'elf',
)

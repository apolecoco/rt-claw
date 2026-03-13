project('rt-claw', 'c',
    version: '0.1.0',
    default_options: [
        'c_std=gnu99',
        'warning_level=2',
    ],
)

osal_backend = get_option('osal')
project_root = meson.project_source_root()

# Platform-specific include directories and compile definitions
platform_defs = []
platform_incs = []

if osal_backend == 'rtthread'
    platform_defs += ['-D_POSIX_SOURCE', '-DCLAW_PLATFORM_RTTHREAD']
    rtt = project_root / 'vendor' / 'rt-thread'
    platform_incs += include_directories(
        'vendor/rt-thread/include',
        'vendor/rt-thread/libcpu/arm/cortex-a',
        'vendor/rt-thread/components/legacy',
        'vendor/rt-thread/components/finsh',
        'vendor/rt-thread/components/net/lwip/port',
        'vendor/rt-thread/components/net/lwip/port/netif',
        'platform/vexpress-a9-qemu/drivers',
        'vendor/rt-thread/components/libc/compilers/common/include',
        'vendor/rt-thread/components/libc/compilers/newlib',
        'vendor/rt-thread/components/libc/posix/io/poll',
        'vendor/rt-thread/components/libc/posix/io/stdio',
        'vendor/rt-thread/components/libc/posix/ipc',
        'vendor/rt-thread/components/drivers/include',
        'vendor/rt-thread/components/net/sal/include',
        'vendor/rt-thread/components/net/sal/include/socket',
        'vendor/rt-thread/components/net/sal/include/socket/sys_socket',
        'vendor/rt-thread/components/net/netdev/include',
        'vendor/rt-thread/components/net/lwip/lwip-2.1.2/src/include',
        'vendor/rt-thread/components/net/lwip/lwip-2.1.2/src/include/netif',
        'platform/vexpress-a9-qemu',
        is_system: true,
    )
elif osal_backend == 'freertos'
    # ESP-IDF defines and include paths are provided by the auto-generated
    # cross.ini (scripts/gen-esp32c3-cross.py).  No platform_incs needed.
    platform_defs += ['-DCLAW_PLATFORM_ESP_IDF']
endif

# AI config overrides (pass via -Dai_api_key=... on meson setup)
_ai_key = get_option('ai_api_key')
if _ai_key != ''
    platform_defs += ['-DCONFIG_CLAW_AI_API_KEY="' + _ai_key + '"']
endif
_ai_url = get_option('ai_api_url')
if _ai_url != ''
    platform_defs += ['-DCONFIG_CLAW_AI_API_URL="' + _ai_url + '"']
endif
_ai_model = get_option('ai_model')
if _ai_model != ''
    platform_defs += ['-DCONFIG_CLAW_AI_MODEL="' + _ai_model + '"']
endif

subdir('vendor/cjson')
subdir('osal')
subdir('src')

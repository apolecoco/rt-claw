osal_inc = include_directories('include')

osal_backend = get_option('osal')

if osal_backend == 'rtthread'
    osal_src = files(
        'rtthread/claw_os_rtthread.c',
        'rtthread/claw_net_rtthread.c',
    )
    osal_lib = static_library('osal', osal_src,
        include_directories: [osal_inc] + platform_incs,
        c_args: platform_defs,
    )
    osal_dep = declare_dependency(
        link_with: osal_lib,
        include_directories: osal_inc,
    )
elif osal_backend == 'freertos'
    osal_src = files(
        'freertos/claw_os_freertos.c',
        'freertos/claw_net_freertos.c',
    )
    osal_lib = static_library('osal', osal_src,
        include_directories: [osal_inc] + platform_incs,
        c_args: platform_defs,
    )
    osal_dep = declare_dependency(
        link_with: osal_lib,
        include_directories: osal_inc,
    )
else
    # osal='none': OSAL compiled by platform build system.
    osal_dep = declare_dependency(
        include_directories: osal_inc,
    )
endif

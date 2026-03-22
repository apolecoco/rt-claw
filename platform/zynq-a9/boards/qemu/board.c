/* SPDX-License-Identifier: MIT */

#include "platform/board.h"

void board_early_init(void)
{
    /* No board-specific init needed for Zynq QEMU */
}

const shell_cmd_t *board_platform_commands(int *count)
{
    *count = 0;
    return NULL;
}

/* SPDX-License-Identifier: MIT */
/* Minimal bspconfig.h for Zynq-7000 QEMU standalone BSP */

#ifndef BSPCONFIG_H
#define BSPCONFIG_H

#ifndef STDIN_BASEADDRESS
#define STDIN_BASEADDRESS  0xE0000000
#endif
#ifndef STDOUT_BASEADDRESS
#define STDOUT_BASEADDRESS 0xE0000000
#endif
#define EL1_NONSECURE      0

#endif /* BSPCONFIG_H */

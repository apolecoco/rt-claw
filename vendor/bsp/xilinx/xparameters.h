/* SPDX-License-Identifier: MIT */
/*
 * xparameters.h - Hardware parameters for Zynq-7000 SoC (QEMU xilinx-zynq-a9)
 *
 * Auto-generated BSP parameters for QEMU emulated Zynq-7000 PS.
 * Values derived from the Zynq-7000 Technical Reference Manual (UG585)
 * and adjusted for QEMU emulation where noted.
 */

#ifndef XPARAMETERS_H
#define XPARAMETERS_H

#include "xparameters_ps.h"

/* ------------------------------------------------------------------ */
/*  Platform                                                          */
/* ------------------------------------------------------------------ */
#define PLATFORM_ZYNQ

/* ------------------------------------------------------------------ */
/*  CPU - Cortex-A9 #0                                                */
/* ------------------------------------------------------------------ */
/*
 * Real silicon runs at 667 MHz; QEMU does not emulate clock speed
 * faithfully.  100 MHz matches the Xilinx FreeRTOS demo defaults.
 */
#define XPAR_CPU_CORTEXA9_0_CPU_CLK_FREQ_HZ    100000000U
#define XPAR_CPU_CORTEXA9_CORE_CLOCK_FREQ_HZ   XPAR_CPU_CORTEXA9_0_CPU_CLK_FREQ_HZ
#define XPAR_CPU_ID                             0U

/* ------------------------------------------------------------------ */
/*  GIC (SCU Generic Interrupt Controller)                            */
/* ------------------------------------------------------------------ */
#define XPAR_SCUGIC_NUM_INSTANCES               1U
#define XPAR_XSCUGIC_NUM_INSTANCES              XPAR_SCUGIC_NUM_INSTANCES

#define XPAR_SCUGIC_SINGLE_DEVICE_ID            0U
#define XPAR_SCUGIC_0_DEVICE_ID                 0U

/* CPU interface register base (MPCore private memory region) */
#define XPAR_PS7_SCUGIC_0_BASEADDR              0xF8F00100U
#define XPAR_SCUGIC_0_CPU_BASEADDR              XPAR_PS7_SCUGIC_0_BASEADDR

/* Distributor register base */
#define XPAR_PS7_SCUGIC_0_DIST_BASEADDR         0xF8F01000U
#define XPAR_SCUGIC_0_DIST_BASEADDR             XPAR_PS7_SCUGIC_0_DIST_BASEADDR

/* ------------------------------------------------------------------ */
/*  SCU Private Timer                                                 */
/* ------------------------------------------------------------------ */
#define XPAR_SCUTIMER_NUM_INSTANCES             1U
#define XPAR_XSCUTIMER_NUM_INSTANCES            XPAR_SCUTIMER_NUM_INSTANCES

#define XPAR_SCUTIMER_DEVICE_ID                 0U
#define XPAR_XSCUTIMER_0_DEVICE_ID              XPAR_SCUTIMER_DEVICE_ID

#define XPAR_PS7_SCUTIMER_0_BASEADDR            0xF8F00600U
#define XPAR_XSCUTIMER_0_BASEADDR               XPAR_PS7_SCUTIMER_0_BASEADDR

/* Private timer PPI = IRQ 29 */
#define XPAR_SCUTIMER_INTR                      29U

/* ------------------------------------------------------------------ */
/*  UART                                                              */
/* ------------------------------------------------------------------ */
#define XPAR_XUARTPS_NUM_INSTANCES              2U

/* UART 0 */
#define XPAR_PS7_UART_0_BASEADDR                0xE0000000U
#define XPAR_PS7_UART_0_HIGHADDR                0xE0000FFFU
#define XPAR_PS7_UART_0_UART_CLK_FREQ_HZ        50000000U
#define XPAR_PS7_UART_0_INTR                    59U
#define XPAR_PS7_UART_0_DEVICE_ID               0U

/* UART 1 */
#define XPAR_PS7_UART_1_BASEADDR                0xE0001000U
#define XPAR_PS7_UART_1_HIGHADDR                0xE0001FFFU
#define XPAR_PS7_UART_1_UART_CLK_FREQ_HZ        50000000U
#define XPAR_PS7_UART_1_INTR                    82U
#define XPAR_PS7_UART_1_DEVICE_ID               1U

/* Stdin / stdout default to UART 1 (QEMU console) */
#ifndef STDIN_BASEADDRESS
#define STDIN_BASEADDRESS                       XPAR_PS7_UART_1_BASEADDR
#endif
#ifndef STDOUT_BASEADDRESS
#define STDOUT_BASEADDRESS                      XPAR_PS7_UART_1_BASEADDR
#endif

/* ------------------------------------------------------------------ */
/*  GEM (Cadence Gigabit Ethernet MAC)                                */
/* ------------------------------------------------------------------ */
/* GEM 0 */
#define XPAR_PS7_ETHERNET_0_BASEADDR            0xE000B000U
#define XPAR_PS7_ETHERNET_0_HIGHADDR            0xE000BFFFU
#define XPAR_PS7_ETHERNET_0_INTR                54U
#define XPAR_PS7_ETHERNET_0_DEVICE_ID           0U

/* GEM 1 */
#define XPAR_PS7_ETHERNET_1_BASEADDR            0xE000C000U
#define XPAR_PS7_ETHERNET_1_HIGHADDR            0xE000CFFFU
#define XPAR_PS7_ETHERNET_1_INTR                77U
#define XPAR_PS7_ETHERNET_1_DEVICE_ID           1U

/* ------------------------------------------------------------------ */
/*  Triple Timer Counter (TTC)                                        */
/* ------------------------------------------------------------------ */
#define XPAR_PS7_TTC_0_BASEADDR                 0xF8001000U
#define XPAR_PS7_TTC_1_BASEADDR                 0xF8002000U

/* TTC 0 counter IRQs: 42, 43, 44 */
#define XPAR_PS7_TTC_0_TTC_CLK_FREQ_HZ         100000000U
#define XPAR_PS7_TTC_0_INTR                     42U

/* ------------------------------------------------------------------ */
/*  DDR Memory                                                        */
/* ------------------------------------------------------------------ */
#define XPAR_PS7_DDR_0_S_AXI_BASEADDR           0x00100000U
#define XPAR_PS7_DDR_0_S_AXI_HIGHADDR           0x3FFFFFFFU

/* Canonical aliases used by some BSP code */
#define XPAR_PS7_DDR_0_BASEADDRESS              XPAR_PS7_DDR_0_S_AXI_BASEADDR
#define XPAR_PS7_DDR_0_HIGHADDRESS              XPAR_PS7_DDR_0_S_AXI_HIGHADDR

/* ------------------------------------------------------------------ */
/*  Miscellaneous PS peripherals (base addresses only)                */
/* ------------------------------------------------------------------ */
#define XPAR_PS7_SLCR_0_S_AXI_BASEADDR          0xF8000000U
#define XPAR_PS7_QSPI_0_BASEADDR               0xE000D000U
#define XPAR_PS7_GPIO_0_BASEADDR                0xE000A000U
#define XPAR_PS7_I2C_0_BASEADDR                 0xE0004000U
#define XPAR_PS7_I2C_1_BASEADDR                 0xE0005000U
#define XPAR_PS7_SPI_0_BASEADDR                 0xE0006000U
#define XPAR_PS7_SPI_1_BASEADDR                 0xE0007000U
#define XPAR_PS7_USB_0_BASEADDR                 0xE0002000U
#define XPAR_PS7_SD_0_BASEADDR                  0xE0100000U
#define XPAR_PS7_SD_1_BASEADDR                  0xE0101000U

/* ------------------------------------------------------------------ */
/*  SCU Watchdog Timer                                                */
/* ------------------------------------------------------------------ */
#define XPAR_PS7_SCUWDT_0_BASEADDR              0xF8F00620U
#define XPAR_PS7_SCUWDT_0_INTR                  30U

/* ------------------------------------------------------------------ */
/*  L2 Cache Controller (PL310)                                       */
/* ------------------------------------------------------------------ */
#define XPS_L2CC_BASEADDR                       0xF8F02000U

/* ------------------------------------------------------------------ */
/*  GIC max interrupt inputs                                          */
/* ------------------------------------------------------------------ */
#define XSCUGIC_MAX_NUM_INTR_INPUTS             95U

#endif /* XPARAMETERS_H */

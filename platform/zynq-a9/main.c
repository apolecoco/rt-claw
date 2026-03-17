/*
 * Copyright (c) 2026, Chao Liu <chao.liu.zevorn@gmail.com>
 * SPDX-License-Identifier: MIT
 *
 * Platform entry for Zynq-A9 QEMU / FreeRTOS.
 */

#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "xscugic.h"
#include "xscutimer.h"
#include "xil_cache.h"
#include "xparameters.h"

/* claw_init() may not be linked yet; forward declare */
extern int claw_init(void);

/*
 * GIC instance — extern so FreeRTOS port tick config can reference it.
 * The timer is module-local.
 */
XScuGic xInterruptController;
static XScuTimer xTimer;

#define CLAW_MAIN_STACK_SIZE    8192
#define CLAW_MAIN_PRIORITY      2

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */
static void claw_main_task(void *pvParameters);
static void init_platform(void);

/* ------------------------------------------------------------------ */
/* main — platform entry point                                        */
/* ------------------------------------------------------------------ */
int main(void)
{
    init_platform();

    printf("rt-claw: Zynq-A9 QEMU (FreeRTOS) - Real-Time Claw\n");

    xTaskCreate(claw_main_task,
                "claw_main",
                CLAW_MAIN_STACK_SIZE,
                NULL,
                CLAW_MAIN_PRIORITY,
                NULL);

    vTaskStartScheduler();

    /* Should never reach here */
    for (;;) {
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* Main task — runs claw_init then idles                              */
/* ------------------------------------------------------------------ */
static void claw_main_task(void *pvParameters)
{
    (void)pvParameters;

    claw_init();

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ------------------------------------------------------------------ */
/* Platform init — enable caches                                      */
/* ------------------------------------------------------------------ */
static void init_platform(void)
{
    /*
     * Skip L1/L2 cache init on QEMU — the PL310 controller is not
     * fully emulated and can cause hangs.  QEMU runs fast enough
     * without caches for development.
     */
}

/* ================================================================== */
/* FreeRTOS hooks                                                     */
/* ================================================================== */

void vApplicationMallocFailedHook(void)
{
    printf("FATAL: FreeRTOS malloc failed!\n");
    configASSERT(0);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask,
                                   char *pcTaskName)
{
    (void)xTask;
    printf("FATAL: stack overflow in task '%s'\n", pcTaskName);
    configASSERT(0);
}

void vApplicationIdleHook(void)
{
    /* Nothing to do */
}

void vAssertCalled(const char *pcFile, unsigned long ulLine)
{
    printf("ASSERT: %s:%lu\n", pcFile, ulLine);
    for (;;) {
    }
}

#if (configSUPPORT_STATIC_ALLOCATION == 1)

static StaticTask_t xIdleTaskTCB;
static StackType_t  uxIdleTaskStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer   = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB;
static StackType_t  uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer   = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize   = configTIMER_TASK_STACK_DEPTH;
}

#endif /* configSUPPORT_STATIC_ALLOCATION */

/* ================================================================== */
/* Tick timer configuration (Zynq private timer via GIC)              */
/* ================================================================== */

/*
 * The private timer runs at half the CPU frequency on Zynq.
 * QEMU may or may not model this accurately, but the ratio is correct.
 */
#define TIMER_CLOCK_HZ \
    (XPAR_CPU_CORTEXA9_0_CPU_CLK_FREQ_HZ / 2)

#define TIMER_DEVICE_ID     XPAR_SCUTIMER_DEVICE_ID
#define TIMER_IRQ_ID        XPAR_SCUTIMER_INTR

/* Called by the FreeRTOS Cortex-A9 port layer */
void vConfigureTickInterrupt(void)
{
    XScuGic_Config *pxGICConfig;
    XScuTimer_Config *pxTimerConfig;
    BaseType_t xStatus;

    /* ---- GIC ---- */
    pxGICConfig = XScuGic_LookupConfig(XPAR_SCUGIC_SINGLE_DEVICE_ID);
    configASSERT(pxGICConfig != NULL);

    xStatus = XScuGic_CfgInitialize(&xInterruptController,
                                     pxGICConfig,
                                     pxGICConfig->CpuBaseAddress);
    configASSERT(xStatus == XST_SUCCESS);
    (void)xStatus;

    /*
     * Install the GIC interrupt handler that the FreeRTOS Cortex-A port
     * calls from the IRQ wrapper (FreeRTOS_IRQ_Handler).
     */
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT,
        (Xil_ExceptionHandler)XScuGic_InterruptHandler,
        &xInterruptController);

    /* Connect the tick handler to the private timer interrupt */
    xStatus = XScuGic_Connect(&xInterruptController,
                               TIMER_IRQ_ID,
                               (Xil_ExceptionHandler)FreeRTOS_Tick_Handler,
                               (void *)&xTimer);
    configASSERT(xStatus == XST_SUCCESS);

    /* ---- Private timer ---- */
    pxTimerConfig = XScuTimer_LookupConfig(TIMER_DEVICE_ID);
    configASSERT(pxTimerConfig != NULL);

    xStatus = XScuTimer_CfgInitialize(&xTimer,
                                       pxTimerConfig,
                                       pxTimerConfig->BaseAddr);
    configASSERT(xStatus == XST_SUCCESS);

    XScuTimer_EnableAutoReload(&xTimer);
    XScuTimer_SetPrescaler(&xTimer, 0);

    XScuTimer_LoadTimer(&xTimer,
                        (TIMER_CLOCK_HZ / configTICK_RATE_HZ) - 1);

    /* Enable the timer interrupt in the GIC, then start */
    XScuGic_SetPriorityTriggerType(&xInterruptController,
                                    TIMER_IRQ_ID,
                                    0xA0,   /* priority */
                                    0x03);  /* rising edge */

    XScuGic_Enable(&xInterruptController, TIMER_IRQ_ID);
    XScuTimer_EnableInterrupt(&xTimer);
    XScuTimer_Start(&xTimer);

    /* Enable IRQ exceptions in the ARM core */
    Xil_ExceptionEnable();
}

/* Called by the FreeRTOS port after servicing the tick interrupt */
void vClearTickInterrupt(void)
{
    XScuTimer_ClearInterruptStatus(&xTimer);
}

/* ================================================================== */
/* GIC interrupt dispatch                                             */
/* ================================================================== */

/*
 * Called from the FreeRTOS IRQ handler (portASM.S) on Cortex-A ports
 * that define configINTERRUPT_CONTROLLER_CPU_INTERFACE_OFFSET.  This
 * reads the GIC acknowledge register, dispatches to the registered
 * handler, and writes the end-of-interrupt register.
 */
void vApplicationFPUSafeIRQHandler(uint32_t ulICCIAR)
{
    (void)ulICCIAR;
    XScuGic_InterruptHandler(&xInterruptController);
}

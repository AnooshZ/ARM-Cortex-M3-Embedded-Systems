#define osObjectsPublic
#include "osObjects.h"
#include "cmsis_os.h"
#include <stdint.h>
#define SOLUTION

osThreadId t_low, t_medium, t_high;
osThreadId t_main;

/* Boolean variable representing resource R1 */
volatile uint8_t R1_busy = 0;

/* Simple workload */
void delay(void)
{
    volatile uint32_t i;

    for (i = 0; i < 100000U; i++) {
    }
}

/* Low-priority task C */
void taskC(void const *argument)
{
    R1_busy = 1;

    for (;;) {
        delay();

        if (osSignalWait(0x01, 0).status == osEventSignal) {
            R1_busy = 0;
            osSignalSet(t_high, 0x02);
            break;
        }
    }

    for (;;) {
        osDelay(osWaitForever);
    }
}

/* Medium-priority task B */
void taskB(void const *argument)
{
    for (;;) {
        delay();
    }
}

/* High-priority task A */
void taskA(void const *argument)
{
    /* Request the resource held by task C */

#ifdef SOLUTION
    osThreadSetPriority(t_low, osPriorityHigh);
#endif

    osSignalSet(t_low, 0x01);
    osSignalWait(0x02, osWaitForever);

#ifdef SOLUTION
    osThreadSetPriority(t_low, osPriorityBelowNormal);
#endif

    for (;;) {
        osDelay(osWaitForever);
    }
}

/* Thread priorities */
osThreadDef(taskA, osPriorityHigh, 1, 0);
osThreadDef(taskB, osPriorityNormal, 1, 0);
osThreadDef(taskC, osPriorityBelowNormal, 1, 0);

int main(void)
{
    osKernelInitialize();

    t_main = osThreadGetId();
    osThreadSetPriority(t_main, osPriorityHigh);

    /* Start low-priority task first */
    t_low = osThreadCreate(osThread(taskC), NULL);
    osDelay(5);

    /* Then medium-priority task */
    t_medium = osThreadCreate(osThread(taskB), NULL);
    osDelay(5);

    /* Finally high-priority task */
    t_high = osThreadCreate(osThread(taskA), NULL);

    osThreadTerminate(t_main);
    osKernelStart();

    for (;;) {
    }
}
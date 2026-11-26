
#include "cmsis_os.h"
#include "LPC17xx.h"
#include "Board_LED.h"
#include <stdint.h>

/* Periods and computation times in milliseconds */
#define PERIOD_A 40000U
#define PERIOD_B 40000U
#define PERIOD_C 20000U

#define COMP_A   20000U
#define COMP_B   10000U
#define COMP_C    5000U

/* RTX tick interval = 10 ms */
#define TICK_MS 10U

#define SIG_RELEASE 0x01

#define LED_A 0
#define LED_B 1
#define LED_C 2

/* Thread IDs */
osThreadId tid_A;
osThreadId tid_B;
osThreadId tid_C;

/* Debug counters */
volatile uint32_t count_a = 0;
volatile uint32_t count_b = 0;
volatile uint32_t count_c = 0;

/*
 * The currently executing RMS task.
 * 0 = none, 1 = A, 2 = B, 3 = C
 */
volatile int running_task = 0;

/* Track active tasks for preemption detection */
volatile int active_A = 0;
volatile int active_B = 0;
volatile int active_C = 0;

/* LED display */
void LED_Show(int id)
{
    LED_SetOut(0);

    if (id == 1)
        LED_On(LED_A);
    else if (id == 2)
        LED_On(LED_B);
    else if (id == 3)
        LED_On(LED_C);
}

/* Initialize hardware Timer0 at 1 MHz */
void Clock_Init(void)
{
    uint32_t div;

    SystemCoreClockUpdate();

    LPC_SC->PCONP |= (1U << 1);

    switch ((LPC_SC->PCLKSEL0 >> 2) & 3U) {
        case 0: div = 4; break;
        case 1: div = 1; break;
        case 2: div = 2; break;
        default: div = 8; break;
    }

    LPC_TIM0->TCR = 2;
    LPC_TIM0->PR =
        (SystemCoreClock / div) / 1000000U - 1U;
    LPC_TIM0->TC = 0;
    LPC_TIM0->TCR = 1;
}

/*
 * Return the highest-priority active process.
 */
int CurrentTask(void)
{
    if (active_C) return 3;
    if (active_B) return 2;
    if (active_A) return 1;
    return 0;
}

/*
 * Custom computation delay.
 *
 * Counts elapsed microseconds only while this
 * process is the highest-priority active task.
 *
 * Higher-priority processes can preempt lower ones.
 */
void delay(unsigned int ms, int id)
{
    uint32_t required = (uint32_t)ms * 1000U;
    uint32_t elapsed = 0;
    uint32_t previous;
    uint32_t now;
    int current;

    if (id == 1) active_A = 1;
    if (id == 2) active_B = 1;
    if (id == 3) active_C = 1;

    running_task = id;
    LED_Show(id);

    previous = LPC_TIM0->TC;

    while (elapsed < required) {

        now = LPC_TIM0->TC;
        current = CurrentTask();

        if (current == id) {
            /*
             * Count CPU time while this task
             * is the highest-priority active task.
             */
            elapsed += (uint32_t)(now - previous);

            if (running_task != id) {
                running_task = id;
                LED_Show(id);
            }
        }

        previous = now;
    }

    if (id == 1) active_A = 0;
    if (id == 2) active_B = 0;
    if (id == 3) active_C = 0;

    running_task = CurrentTask();
    LED_Show(running_task);
}

/* Process A: lowest priority */
void ProcessA(void const *argument)
{
    for (;;) {
        osSignalWait(SIG_RELEASE, osWaitForever);

        delay(COMP_A, 1);
        count_a++;
    }
}

/* Process B: medium priority */
void ProcessB(void const *argument)
{
    for (;;) {
        osSignalWait(SIG_RELEASE, osWaitForever);

        delay(COMP_B, 2);
        count_b++;
    }
}

/* Process C: highest priority */
void ProcessC(void const *argument)
{
    for (;;) {
        osSignalWait(SIG_RELEASE, osWaitForever);

        delay(COMP_C, 3);
        count_c++;
    }
}

/* Fixed RMS priorities */
osThreadDef(ProcessA, osPriorityBelowNormal, 1, 0);
osThreadDef(ProcessB, osPriorityNormal, 1, 0);
osThreadDef(ProcessC, osPriorityAboveNormal, 1, 0);

/*
 * Periodic release callback.
 *
 * Timer 0 releases C every 20 seconds.
 * Timer 1 releases A and B every 40 seconds.
 */
void callback(void const *param)
{
    if ((uint32_t)param == 0U) {
        osSignalSet(tid_C, SIG_RELEASE);
    }
    else {
        osSignalSet(tid_B, SIG_RELEASE);
        osSignalSet(tid_A, SIG_RELEASE);
    }
}

osTimerDef(timer0_handle, callback);
osTimerDef(timer1_handle, callback);

int main(void)
{
    osTimerId timer_0;
    osTimerId timer_1;

    osKernelInitialize();

    LED_Initialize();
    Clock_Init();

    /* Create RMS processes */
    tid_A = osThreadCreate(osThread(ProcessA), NULL);
    tid_B = osThreadCreate(osThread(ProcessB), NULL);
    tid_C = osThreadCreate(osThread(ProcessC), NULL);

    /* Create periodic virtual timers */
    timer_0 = osTimerCreate(
        osTimer(timer0_handle),
        osTimerPeriodic,
        (void *)0
    );

    timer_1 = osTimerCreate(
        osTimer(timer1_handle),
        osTimerPeriodic,
        (void *)1
    );

    osKernelStart();

    /* Convert milliseconds into RTX ticks */
    osTimerStart(timer_0, PERIOD_C);
    osTimerStart(timer_1, PERIOD_A);

    /* Release all three processes initially */
    osSignalSet(tid_A, SIG_RELEASE);
    osSignalSet(tid_B, SIG_RELEASE);
    osSignalSet(tid_C, SIG_RELEASE);

    for (;;) {
        osDelay(osWaitForever);
    }
}
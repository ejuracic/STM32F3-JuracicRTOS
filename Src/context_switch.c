#include <stdint.h>
#include <stddef.h>
#include "context_switch.h"

// ARM System Control Block registers
#define SCB_ICSR  (*(volatile uint32_t *)0xE000ED04UL)
#define SCB_SHPR3 (*(volatile uint32_t *)0xE000ED20UL)

// PendSV pending bit
#define PENDSV_SET (1UL << 28)

// The assembly below uses the saved_sp field at offset 8
_Static_assert(sizeof(void *) == 4U,
               "Context switching requires 32-bit pointers");

_Static_assert(offsetof(TaskContext, saved_sp) == 8U,
               "TaskContext saved_sp offset changed");

// Pointers accessed directly by the assembly handlers
TaskContext * volatile g_switch_current_task = 0;
TaskContext * volatile g_switch_next_task = 0;

// The two registered contexts
static TaskContext *g_switch_first_task = 0;
static TaskContext *g_switch_second_task = 0;

// Number of completed PendSV switches
volatile uint32_t g_switch_count = 0U;


// Indicates that the first PSP-based thread has started
volatile uint32_t g_switch_started = 0U;

// Indicates that periodic preemption has been requested
static volatile uint32_t g_preemption_enabled = 0U;


// Name: context_switch_configure
// Description: Initializes the two-thread switching system
//              and sets PendSV to the lowest priority.
// Parameters: first - Initial thread context.
//             second - Second thread context.
// Returns: None
void context_switch_configure(
    TaskContext *first,
    TaskContext *second
)
{
    g_switch_first_task = first;
    g_switch_second_task = second;

    g_switch_current_task = first;
    g_switch_next_task = second;
    g_switch_count = 0U;
    g_switch_started = 0U;
    g_preemption_enabled = 0U;

    // PendSV priority = 255, the lowest priority
    SCB_SHPR3 =
        (SCB_SHPR3 & ~(0xFFUL << 16)) |
        (0xFFUL << 16);
}

// Name: context_switch_yield
// Description: Selects the other thread and requests
//              the PendSV exception.
// Parameters: None
// Returns: None
void context_switch_yield(void)
{
    if (g_switch_current_task == g_switch_first_task)
    {
        g_switch_next_task = g_switch_second_task;
    }
    else
    {
        g_switch_next_task = g_switch_first_task;
    }

    // Request PendSV
    SCB_ICSR = PENDSV_SET;

    // Ensure the write completes before continuing
    __asm volatile (
        "dsb\n"
        "isb\n"
        ::: "memory"
    );
}


// Name: context_switch_enable_preemption
// Description: Enables timer-based scheduling requests.
//              Switching begins after SVC starts Thread A.
// Parameters: None
// Returns: None
void context_switch_enable_preemption(void)
{
    g_preemption_enabled = 1U;
}

// Name: context_switch_on_systick
// Description: Requests the next PendSV context switch
//              after a SysTick interrupt.
// Parameters: None
// Returns: None
void context_switch_on_systick(void)
{
    // Do nothing in the normal cooperative application
    if (g_preemption_enabled == 0U)
    {
        return;
    }

    // Never switch before the first PSP thread has started
    if (g_switch_started == 0U)
    {
        return;
    }

    // Select the other thread and pend PendSV
    context_switch_yield();
}


// Name: context_switch_get_count
// Description: Returns the completed context-switch count.
// Parameters: None
// Returns: Number of completed PendSV switches.
uint32_t context_switch_get_count(void)
{
    return g_switch_count;
}

// Name: context_switch_start
// Description: Triggers SVC to launch the initial thread.
// Parameters: None
// Returns: Does not return during normal operation.
void context_switch_start(void)
{
    __asm volatile ("svc 0");

    // Execution should never return here
    for (;;)
    {
        __asm volatile ("wfi");
    }
}

// Name: SVC_Handler
// Description: Restores the first thread's prepared context
//              and enters Thread mode using PSP.
// Parameters: None
// Returns: By hardware exception return.
__attribute__((naked))
void SVC_Handler(void)
{
    __asm volatile (
        "ldr r0, =g_switch_current_task\n"
        "ldr r0, [r0]\n"
        "ldr r0, [r0, #8]\n"

        // Restore the prepared R4-R11 values
        "ldmia r0!, {r4-r11}\n"

        
        // PSP now points at the hardware exception frame
        "msr psp, r0\n"

        // Mark the first thread as started
        "ldr r1, =g_switch_started\n"
        "movs r2, #1\n"
        "str r2, [r1]\n"

        // Return to Thread mode using a basic PSP frame
        "ldr lr, =0xFFFFFFFD\n"
        "bx lr\n"
    );
}

// Name: PendSV_Handler
// Description: Saves the outgoing thread's integer context,
//              selects the next thread, and restores its context.
// Parameters: None
// Returns: By hardware exception return.
__attribute__((naked))
void PendSV_Handler(void)
{
    __asm volatile (
        // Read the outgoing thread's PSP
        "mrs r0, psp\n"

        // Save R4-R11 on the outgoing thread's stack
        "stmdb r0!, {r4-r11}\n"

        // Store the outgoing thread's new saved_sp
        "ldr r1, =g_switch_current_task\n"
        "ldr r2, [r1]\n"
        "str r0, [r2, #8]\n"

        // Change the current thread to the selected thread
        "ldr r3, =g_switch_next_task\n"
        "ldr r2, [r3]\n"
        "str r2, [r1]\n"

        // Load the incoming thread's saved_sp
        "ldr r0, [r2, #8]\n"

        // Restore the incoming thread's R4-R11
        "ldmia r0!, {r4-r11}\n"

        // Update PSP for hardware exception return
        "msr psp, r0\n"

        // Record one completed switch
        "ldr r3, =g_switch_count\n"
        "ldr r2, [r3]\n"
        "adds r2, r2, #1\n"
        "str r2, [r3]\n"

        // Hardware restores R0-R3, R12, LR, PC, xPSR
        "bx lr\n"
    );
}

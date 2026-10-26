#include <stdint.h>
#include <stddef.h>
#include "context_switch.h"
#include "systick.h"

// ARM System Control Block registers
#define SCB_ICSR  (*(volatile uint32_t *)0xE000ED04UL)
#define SCB_SHPR3 (*(volatile uint32_t *)0xE000ED20UL)

#define PENDSV_SET (1UL << 28)

#define USER_THREAD_COUNT 2U
#define TOTAL_THREAD_COUNT 3U
#define IDLE_THREAD_INDEX 2U

// Validate assumptions used by our ARM assembly
_Static_assert(
    sizeof(void *) == 4U,
    "Context switching requires 32-bit pointers"
);

_Static_assert(
    offsetof(TaskContext, saved_sp) == 8U,
    "TaskContext.saved_sp must remain at offset 8"
);

// Kernel thread control block
typedef struct
{
    TaskContext *context;
    volatile ContextThreadState state;
    uint32_t wake_ms;
} KernelThread;

// Thread table and current thread index
static KernelThread g_threads[TOTAL_THREAD_COUNT];
static volatile uint32_t g_current_thread = 0U;

// Current task context used by SVC and PendSV
TaskContext * volatile g_switch_current_task = 0;

// Scheduling control and diagnostics
volatile uint32_t g_switch_count = 0U;
volatile uint32_t g_switch_started = 0U;

static volatile uint32_t g_preemption_enabled = 0U;

// Name: context_switch_configure
// Description: Initializes two user threads and one idle
//              thread, including their starting states.
// Parameters: first - Initial thread context.
//             second - Second user thread context.
//             idle - Idle thread context.
// Returns: None
void context_switch_configure(
    TaskContext *first,
    TaskContext *second,
    TaskContext *idle
)
{
    // Thread A begins as the initial running thread
    g_threads[0].context = first;
    g_threads[0].state = CONTEXT_THREAD_RUNNING;
    g_threads[0].wake_ms = 0U;

    // Thread B begins ready to execute
    g_threads[1].context = second;
    g_threads[1].state = CONTEXT_THREAD_READY;
    g_threads[1].wake_ms = 0U;

    // Idle thread is available when no user thread is ready
    g_threads[2].context = idle;
    g_threads[2].state = CONTEXT_THREAD_READY;
    g_threads[2].wake_ms = 0U;

    g_current_thread = 0U;
    g_switch_current_task = first;

    g_switch_count = 0U;
    g_switch_started = 0U;
    g_preemption_enabled = 0U;

    // Configure PendSV with the lowest priority
    SCB_SHPR3 =
        (SCB_SHPR3 & ~(0xFFUL << 16)) |
        (0xFFUL << 16);
}

// Name: context_switch_enable_preemption
// Description: Arms automatic scheduling through SysTick.
// Parameters: None
// Returns: None
void context_switch_enable_preemption(void)
{
    g_preemption_enabled = 1U;
}

// Name: context_switch_on_systick
// Description: Requests PendSV while the thread kernel is active.
// Parameters: None
// Returns: None
void context_switch_on_systick(void)
{
    if ((g_preemption_enabled != 0U) &&
        (g_switch_started != 0U))
    {
        SCB_ICSR = PENDSV_SET;
    }
}

// Name: context_switch_yield
// Description: Requests a voluntary context-switch decision.
// Parameters: None
// Returns: None
void context_switch_yield(void)
{
    if (g_switch_started != 0U)
    {
        SCB_ICSR = PENDSV_SET;

        __asm volatile (
            "dsb\n"
            "isb\n"
            ::: "memory"
        );
    }
}

// Name: context_switch_sleep
// Description: Places the currently running user thread
//              into SLEEPING state until its deadline.
// Parameters: milliseconds - Sleep duration in milliseconds.
// Returns: None
void context_switch_sleep(uint32_t milliseconds)
{
    if ((milliseconds == 0U) ||
        (milliseconds > INT32_MAX) ||
        (g_switch_started == 0U))
    {
        return;
    }

    uint32_t previous_mask;

    // Disable interrupts while changing shared thread state
    __asm volatile (
        "mrs %0, primask\n"
        "cpsid i\n"
        : "=r" (previous_mask)
        :
        : "memory"
    );

    if (g_current_thread < USER_THREAD_COUNT)
    {
        KernelThread *thread =
            &g_threads[g_current_thread];

        // Calculate the future wake-up deadline
        thread->wake_ms =
            systick_get_milliseconds() + milliseconds;

        // Block this thread until its deadline
        thread->state = CONTEXT_THREAD_SLEEPING;

        // Request a scheduling decision
        SCB_ICSR = PENDSV_SET;
    }

    // Restore the original interrupt state
    __asm volatile (
        "msr primask, %0\n"
        "dsb\n"
        "isb\n"
        :
        : "r" (previous_mask)
        : "memory"
    );
}

// Name: context_switch_get_count
// Description: Returns the number of completed thread changes.
// Parameters: None
// Returns: Completed context-switch count.
uint32_t context_switch_get_count(void)
{
    return g_switch_count;
}

// Name: context_switch_get_thread_state
// Description: Retrieves the state of a registered thread.
// Parameters: index - Thread index.
// Returns: Current state or INVALID.
ContextThreadState context_switch_get_thread_state(
    uint32_t index
)
{
    if (index >= TOTAL_THREAD_COUNT)
    {
        return CONTEXT_THREAD_INVALID;
    }

    return g_threads[index].state;
}

// Name: context_switch_select_next
// Description: Wakes eligible threads and selects the next
//              runnable thread in round-robin order.
// Parameters: None
// Returns: Pointer to the selected task context.
// This function is called from PendSV with interrupts masked.
__attribute__((used, noinline))
TaskContext *context_switch_select_next(void)
{
    uint32_t now = systick_get_milliseconds();

    // Wake threads whose sleeping deadlines have expired
    for (uint32_t i = 0U; i < USER_THREAD_COUNT; i++)
    {
        if ((g_threads[i].state ==
             CONTEXT_THREAD_SLEEPING) &&
            ((int32_t)(now - g_threads[i].wake_ms) >= 0))
        {
            g_threads[i].state = CONTEXT_THREAD_READY;
        }
    }

    // Determine where round-robin selection should begin
    uint32_t first =
        (g_current_thread < USER_THREAD_COUNT)
        ? ((g_current_thread + 1U) % USER_THREAD_COUNT)
        : 0U;

    // Default to the idle thread if nobody else can run
    uint32_t next = IDLE_THREAD_INDEX;

    // Search for a ready user thread
    for (uint32_t offset = 0U;
         offset < USER_THREAD_COUNT;
         offset++)
    {
        uint32_t index =
            (first + offset) % USER_THREAD_COUNT;

        ContextThreadState state = g_threads[index].state;

        if ((state == CONTEXT_THREAD_READY) ||
            (state == CONTEXT_THREAD_RUNNING))
        {
            next = index;
            break;
        }
    }

    // Only record a switch when changing actual threads
    if (next != g_current_thread)
    {
        // Return the previous thread to READY if appropriate
        if (g_threads[g_current_thread].state ==
            CONTEXT_THREAD_RUNNING)
        {
            g_threads[g_current_thread].state =
                CONTEXT_THREAD_READY;
        }

        // Activate the selected thread
        g_threads[next].state = CONTEXT_THREAD_RUNNING;

        g_current_thread = next;
        g_switch_current_task = g_threads[next].context;

        g_switch_count++;
    }

    return g_switch_current_task;
}

// Name: context_switch_start
// Description: Starts the initial thread through SVC.
// Parameters: None
// Returns: Does not return during normal operation.
void context_switch_start(void)
{
    __asm volatile ("svc 0");

    for (;;)
    {
        __asm volatile ("wfi");
    }
}

// Name: SVC_Handler
// Description: Restores the initial thread and starts
//              execution in Thread mode using PSP.
// Parameters: None
// Returns: Through hardware exception return.
__attribute__((naked))
void SVC_Handler(void)
{
    __asm volatile (
        "ldr r0, =g_switch_current_task\n"
        "ldr r0, [r0]\n"
        "ldr r0, [r0, #8]\n"

        // Restore R4-R11
        "ldmia r0!, {r4-r11}\n"

        // PSP points to the hardware exception frame
        "msr psp, r0\n"

        // The first PSP thread is now ready to execute
        "ldr r1, =g_switch_started\n"
        "movs r2, #1\n"
        "str r2, [r1]\n"

        // Return to privileged Thread mode using PSP
        "ldr lr, =0xFFFFFFFD\n"
        "bx lr\n"
    );
}

// Name: PendSV_Handler
// Description: Preserves the outgoing execution context,
//              selects a runnable thread, and restores it.
// Parameters: None
// Returns: Through hardware exception return.
__attribute__((naked))
void PendSV_Handler(void)
{
    __asm volatile (
        // Keep the context operation atomic
        "cpsid i\n"

        // Read the outgoing Process Stack Pointer
        "mrs r0, psp\n"

        // Save R4-R11 on the outgoing thread stack
        "stmdb r0!, {r4-r11}\n"

        // Store the outgoing thread's saved stack pointer
        "ldr r1, =g_switch_current_task\n"
        "ldr r2, [r1]\n"
        "str r0, [r2, #8]\n"

        // Preserve EXC_RETURN and maintain stack alignment
        "push {r3, lr}\n"

        // Ask the C scheduler which thread should execute
        "bl context_switch_select_next\n"

        // Restore the handler's saved registers
        "pop {r3, lr}\n"

        // R0 now contains the selected TaskContext pointer
        "ldr r0, [r0, #8]\n"

        // Restore the incoming thread's R4-R11
        "ldmia r0!, {r4-r11}\n"

        // Set PSP for the incoming hardware exception frame
        "msr psp, r0\n"

        // Re-enable interrupts before exception return
        "cpsie i\n"

        // Resume the selected thread
        "bx lr\n"
    );
}

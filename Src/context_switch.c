#include <stdint.h>
#include <stddef.h>
#include "context_switch.h"
#include "systick.h"

// ARM System Control Block registers
#define SCB_ICSR  (*(volatile uint32_t *)0xE000ED04UL)
#define SCB_SHPR3 (*(volatile uint32_t *)0xE000ED20UL)

#define PENDSV_SET (1UL << 28)

// Reserve one additional slot for Idle
#define IDLE_THREAD_SLOT CONTEXT_MAX_USER_THREADS
#define CONTEXT_TOTAL_SLOTS (CONTEXT_MAX_USER_THREADS + 1U)

// Verify the structure layout used by assembly
_Static_assert(
    sizeof(void *) == 4U,
    "Context switching requires 32-bit pointers"
);

_Static_assert(
    offsetof(TaskContext, saved_sp) == 8U,
    "TaskContext.saved_sp must be at offset 8"
);

// Kernel Task Control Block
typedef struct
{
    TaskContext *context;
    const char *name;
    volatile ContextThreadState state;
    uint32_t wake_ms;
    uint32_t dispatch_count;
} KernelThread;

// Statically allocated thread control blocks
static KernelThread g_threads[CONTEXT_TOTAL_SLOTS];

// Thread registration and scheduling state
static volatile uint32_t g_user_thread_count = 0U;
static volatile uint32_t g_current_slot = IDLE_THREAD_SLOT;
static uint32_t g_initialized = 0U;

// Current context accessed by SVC and PendSV assembly
TaskContext * volatile g_switch_current_task = 0;

// Kernel control and diagnostics
volatile uint32_t g_switch_count = 0U;
volatile uint32_t g_switch_started = 0U;

static volatile uint32_t g_preemption_enabled = 0U;

// Name: context_switch_init
// Description: Resets the thread table and registers the
//              dedicated Idle Thread.
// Parameters: idle - Initialized Idle Thread context.
// Returns: 1 on success, 0 on failure.
uint8_t context_switch_init(TaskContext *idle)
{
    if ((g_switch_started != 0U) ||
        (idle == 0) ||
        (task_context_check_initial_frame(idle) == 0U))
    {
        return 0U;
    }

    // Reset all TCB slots
    for (uint32_t i = 0U;
         i < CONTEXT_TOTAL_SLOTS;
         i++)
    {
        g_threads[i] = (KernelThread){0};
    }

    // Register the special Idle Thread
    g_threads[IDLE_THREAD_SLOT].context = idle;
    g_threads[IDLE_THREAD_SLOT].name = "Idle";
    g_threads[IDLE_THREAD_SLOT].state =
        CONTEXT_THREAD_READY;

    g_user_thread_count = 0U;
    g_current_slot = IDLE_THREAD_SLOT;

    g_switch_current_task = 0;
    g_switch_count = 0U;
    g_switch_started = 0U;
    g_preemption_enabled = 0U;

    // PendSV receives the lowest exception priority
    SCB_SHPR3 =
        (SCB_SHPR3 & ~(0xFFUL << 16)) |
        (0xFFUL << 16);

    g_initialized = 1U;

    return 1U;
}

// Name: context_switch_add_thread
// Description: Adds a named application thread to the
//              kernel's statically allocated TCB table.
// Parameters: name - Persistent thread name.
//             context - Prepared task context.
// Returns: 1 on success, 0 on failure.
uint8_t context_switch_add_thread(
    const char *name,
    TaskContext *context
)
{
    if ((g_initialized == 0U) ||
        (g_switch_started != 0U) ||
        (name == 0) ||
        (context == 0) ||
        (g_user_thread_count >= CONTEXT_MAX_USER_THREADS))
    {
        return 0U;
    }

    // Reject invalid initial stack frames
    if (task_context_check_initial_frame(context) == 0U)
    {
        return 0U;
    }

    // Prevent registering the same context twice
    for (uint32_t i = 0U;
         i < g_user_thread_count;
         i++)
    {
        if (g_threads[i].context == context)
        {
            return 0U;
        }
    }

    uint32_t index = g_user_thread_count;

    g_threads[index].context = context;
    g_threads[index].name = name;
    g_threads[index].state = CONTEXT_THREAD_READY;
    g_threads[index].wake_ms = 0U;
    g_threads[index].dispatch_count = 0U;

    g_user_thread_count++;

    return 1U;
}

// Name: context_switch_enable_preemption
// Description: Arms automatic SysTick scheduling.
// Parameters: None
// Returns: None
void context_switch_enable_preemption(void)
{
    if ((g_initialized != 0U) &&
        (g_user_thread_count != 0U))
    {
        g_preemption_enabled = 1U;
    }
}

// Name: context_switch_on_systick
// Description: Requests PendSV after a system tick when
//              the preemptive kernel is running.
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
// Description: Requests a voluntary scheduling decision.
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
// Description: Marks the running application thread
//              sleeping until a future SysTick deadline.
// Parameters: milliseconds - Requested sleep duration.
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

    // Protect the update from interrupt-driven scheduling
    __asm volatile (
        "mrs %0, primask\n"
        "cpsid i\n"
        : "=r" (previous_mask)
        :
        : "memory"
    );

    if ((g_current_slot < g_user_thread_count) &&
        (g_threads[g_current_slot].state ==
         CONTEXT_THREAD_RUNNING))
    {
        KernelThread *thread =
            &g_threads[g_current_slot];

        thread->wake_ms =
            systick_get_milliseconds() + milliseconds;

        thread->state = CONTEXT_THREAD_SLEEPING;

        // The sleeping thread must stop executing
        SCB_ICSR = PENDSV_SET;
    }

    // Restore the previous interrupt mask
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
// Description: Returns actual changes between threads.
// Parameters: None
// Returns: Completed thread-switch count.
uint32_t context_switch_get_count(void)
{
    return g_switch_count;
}

// Name: context_switch_get_thread_count
// Description: Counts application threads plus Idle.
// Parameters: None
// Returns: Number of registered threads.
uint32_t context_switch_get_thread_count(void)
{
    if (g_initialized == 0U)
    {
        return 0U;
    }

    return g_user_thread_count + 1U;
}

// Name: context_switch_get_thread_info
// Description: Copies diagnostic information for a
//              registered application or Idle Thread.
// Parameters: index - Public thread index.
//             output - Destination for copied information.
// Returns: 1 on success, 0 on failure.
uint8_t context_switch_get_thread_info(
    uint32_t index,
    ContextThreadInfo *output
)
{
    if ((output == 0) ||
        (g_initialized == 0U) ||
        (index > g_user_thread_count))
    {
        return 0U;
    }

    // Idle appears immediately after registered threads
    uint32_t slot =
        (index == g_user_thread_count)
        ? IDLE_THREAD_SLOT : index;

    KernelThread *thread = &g_threads[slot];

    output->name = thread->name;
    output->state = thread->state;
    output->wake_ms = thread->wake_ms;
    output->dispatch_count = thread->dispatch_count;

    // Read the thread's stack guard and watermark
    if (task_context_get_stack_stats(
            thread->context,
            &output->stack) == 0U)
    {
        return 0U;
    }

    return 1U;

}

// Name: context_switch_get_thread_state
// Description: Reads a registered thread's current state.
// Parameters: index - Public thread index.
// Returns: Thread state or INVALID.
ContextThreadState context_switch_get_thread_state(
    uint32_t index
)
{
    ContextThreadInfo info;

    if (context_switch_get_thread_info(index, &info) == 0U)
    {
        return CONTEXT_THREAD_INVALID;
    }

    return info.state;
}

// Name: context_switch_select_next
// Description: Wakes expired sleeping threads and selects
//              a runnable thread using round-robin policy.
// Parameters: None
// Returns: Pointer to the selected TaskContext.
// Called by PendSV while interrupts are masked.
__attribute__((used, noinline))
TaskContext *context_switch_select_next(void)
{
    uint32_t now = systick_get_milliseconds();

    // Make sleeping application threads ready when due
    for (uint32_t i = 0U;
         i < g_user_thread_count;
         i++)
    {
        KernelThread *thread = &g_threads[i];

        if ((thread->state == CONTEXT_THREAD_SLEEPING) &&
            ((int32_t)(now - thread->wake_ms) >= 0))
        {
            thread->state = CONTEXT_THREAD_READY;
        }
    }

    uint32_t next = IDLE_THREAD_SLOT;
    uint32_t first = 0U;

    // Begin searching after the currently running thread
    if (g_current_slot < g_user_thread_count)
    {
        first =
            (g_current_slot + 1U) % g_user_thread_count;
    }

    // Search all registered application threads
    for (uint32_t offset = 0U;
         offset < g_user_thread_count;
         offset++)
    {
        uint32_t index =
            (first + offset) % g_user_thread_count;

        ContextThreadState state = g_threads[index].state;

        if ((state == CONTEXT_THREAD_READY) ||
            (state == CONTEXT_THREAD_RUNNING))
        {
            next = index;
            break;
        }
    }

    // Only count a switch when the selected thread changes
    if (next != g_current_slot)
    {
        KernelThread *previous =
            &g_threads[g_current_slot];

        // Make the outgoing thread ready if still runnable
        if (previous->state == CONTEXT_THREAD_RUNNING)
        {
            previous->state = CONTEXT_THREAD_READY;
        }

        // Activate the incoming thread
        g_threads[next].state = CONTEXT_THREAD_RUNNING;
        g_threads[next].dispatch_count++;

        g_current_slot = next;
        g_switch_current_task = g_threads[next].context;

        g_switch_count++;
    }

    return g_switch_current_task;
}

// Name: context_switch_start
// Description: Prepares the first registered thread
//              and starts it through the SVC exception.
// Parameters: None
// Returns: Does not return during normal operation.
void context_switch_start(void)
{
    if ((g_initialized == 0U) ||
        (g_user_thread_count == 0U) ||
        (g_switch_started != 0U))
    {
        for (;;)
        {
            __asm volatile ("wfi");
        }
    }

    // Select the first registered application thread
    g_current_slot = 0U;
    g_threads[0].state = CONTEXT_THREAD_RUNNING;
    g_threads[0].dispatch_count = 1U;

    g_switch_current_task = g_threads[0].context;

    __asm volatile ("svc 0");

    // The initial thread launch should never return
    for (;;)
    {
        __asm volatile ("wfi");
    }
}

// Name: SVC_Handler
// Description: Restores the first prepared thread context
//              and starts it in Thread mode using PSP.
// Parameters: None
// Returns: Through hardware exception return.
__attribute__((naked))
void SVC_Handler(void)
{
    __asm volatile (
        "ldr r0, =g_switch_current_task\n"
        "ldr r0, [r0]\n"
        "ldr r0, [r0, #8]\n"

        // Restore initial R4-R11
        "ldmia r0!, {r4-r11}\n"

        // Point PSP at the hardware exception frame
        "msr psp, r0\n"

        // Allow subsequent SysTick scheduling requests
        "ldr r1, =g_switch_started\n"
        "movs r2, #1\n"
        "str r2, [r1]\n"

        // Return to privileged Thread mode using PSP
        "ldr lr, =0xFFFFFFFD\n"
        "bx lr\n"
    );
}

// Name: PendSV_Handler
// Description: Saves the outgoing thread, calls the
//              scheduler, and restores the selected thread.
// Parameters: None
// Returns: Through hardware exception return.
__attribute__((naked))
void PendSV_Handler(void)
{
    __asm volatile (
        // Prevent interrupting the kernel state update
        "cpsid i\n"

        // Read the outgoing thread's process stack
        "mrs r0, psp\n"

        // Save the software-managed register context
        "stmdb r0!, {r4-r11}\n"

        // Store the outgoing thread's saved stack pointer
        "ldr r1, =g_switch_current_task\n"
        "ldr r2, [r1]\n"
        "str r0, [r2, #8]\n"

        // Preserve exception return and 8-byte alignment
        "push {r3, lr}\n"

        // Run the scheduling algorithm in C
        "bl context_switch_select_next\n"

        // Restore the handler's saved registers
        "pop {r3, lr}\n"

        // R0 contains the chosen TaskContext pointer
        "ldr r0, [r0, #8]\n"

        // Restore the selected thread's R4-R11
        "ldmia r0!, {r4-r11}\n"

        // PSP now points to its hardware exception frame
        "msr psp, r0\n"

        // Re-enable interrupts for exception return
        "cpsie i\n"

        // Resume the chosen thread
        "bx lr\n"
    );
}

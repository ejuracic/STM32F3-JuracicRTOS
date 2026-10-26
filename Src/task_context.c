#include <stdint.h>
#include "task_context.h"

// Initial basic context: R4-R11 plus hardware exception frame
#define TASK_CONTEXT_FRAME_WORDS 16U

// Minimum stack capacity for this implementation
#define TASK_CONTEXT_MIN_STACK_WORDS 32U

// Stack initialization and corruption-detection patterns
#define TASK_STACK_FILL 0xA5A5A5A5U
#define TASK_STACK_GUARD 0xDEADC0DEU

// xPSR Thumb-state bit
#define TASK_INITIAL_XPSR 0x01000000U

// Name: task_context_exit_trap
// Description: Stops a task from returning into invalid code.
// Parameters: None
// Returns: Does not return.
static void task_context_exit_trap(void)
{
    for (;;)
    {
        __asm volatile ("wfi");
    }
}

// Name: task_context_init
// Description: Initializes an independent stack with the
//              initial register state for a future task.
// Parameters: task - Task context to initialize.
//             stack - Stack memory with 8-byte alignment.
//             stack_words - Stack size in 32-bit words.
//             entry - Initial task function.
//             argument - Task function argument.
// Returns: 1 on success, 0 on failure.
uint8_t task_context_init(
    TaskContext *task,
    uint32_t *stack,
    uint32_t stack_words,
    TaskEntryFunction entry,
    void *argument
)
{
    if ((task == 0) ||
        (stack == 0) ||
        (entry == 0) ||
        (stack_words < TASK_CONTEXT_MIN_STACK_WORDS) ||
        ((stack_words & 1U) != 0U) ||
        (((uintptr_t)stack & 7U) != 0U))
    {
        return 0U;
    }

    // Fill unused stack memory with a recognizable pattern
    for (uint32_t i = 0U; i < stack_words; i++)
    {
        stack[i] = TASK_STACK_FILL;
    }

    // Reserve the lowest word as a stack guard
    stack[0] = TASK_STACK_GUARD;

    // Cortex-M stacks grow downward from high addresses
    uint32_t *sp =
        &stack[stack_words - TASK_CONTEXT_FRAME_WORDS];

    // Clear the software-saved and hardware-saved registers
    for (uint32_t i = 0U;
         i < TASK_CONTEXT_FRAME_WORDS;
         i++)
    {
        sp[i] = 0U;
    }

    // R4-R11 occupy positions 0 through 7
    // R0 contains the initial task argument
    sp[8] = (uint32_t)(uintptr_t)argument;

    // R1, R2, R3 and R12 remain initialized to zero

    // LR points to a trap if the task unexpectedly returns
    sp[13] =
        (uint32_t)(uintptr_t)task_context_exit_trap | 1U;

    // PC points to the first instruction of the task
    sp[14] = (uint32_t)(uintptr_t)entry | 1U;

    // xPSR starts with Thumb state enabled
    sp[15] = TASK_INITIAL_XPSR;

    // Store the prepared task context
    task->stack_base = stack;
    task->stack_top = &stack[stack_words];
    task->saved_sp = sp;
    task->stack_words = stack_words;

    return 1U;
}

// Name: task_context_check_initial_frame
// Description: Verifies the initial stack frame and guard.
// Parameters: task - Task context to inspect.
// Returns: 1 if valid, 0 otherwise.
uint8_t task_context_check_initial_frame(
    const TaskContext *task
)
{
    if ((task == 0) ||
        (task->stack_base == 0) ||
        (task->saved_sp == 0) ||
        (task->stack_words < TASK_CONTEXT_MIN_STACK_WORDS))
    {
        return 0U;
    }

    // Check the low-address stack guard
    if (task->stack_base[0] != TASK_STACK_GUARD)
    {
        return 0U;
    }

    // Check the initial stack-pointer location
    if (task->saved_sp !=
        &task->stack_base[
            task->stack_words - TASK_CONTEXT_FRAME_WORDS
        ])
    {
        return 0U;
    }

    // Check the Thumb bit of the prepared xPSR
    if (task->saved_sp[15] != TASK_INITIAL_XPSR)
    {
        return 0U;
    }

    // Check that the task entry address is Thumb code
    if ((task->saved_sp[14] & 1U) == 0U)
    {
        return 0U;
    }

    return 1U;
}

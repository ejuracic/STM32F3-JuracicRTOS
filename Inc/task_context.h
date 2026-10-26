#ifndef TASK_CONTEXT_H
#define TASK_CONTEXT_H

#include <stdint.h>

// Function executed when a task starts
typedef void (*TaskEntryFunction)(void *argument);

// A task's reserved stack and saved execution context
typedef struct
{
    uint32_t *stack_base;
    uint32_t *stack_top;
    uint32_t *saved_sp;
    uint32_t stack_words;
} TaskContext;

// Name: task_context_init
// Description: Prepares a new task stack containing an
//              initial Cortex-M4 register context.
// Parameters: task - Destination task context.
//             stack - Caller-provided stack memory.
//             stack_words - Stack capacity in 32-bit words.
//             entry - Function the task will execute.
//             argument - Pointer passed to the task.
// Returns: 1 on success, 0 on failure.
uint8_t task_context_init(
    TaskContext *task,
    uint32_t *stack,
    uint32_t stack_words,
    TaskEntryFunction entry,
    void *argument
);

// Name: task_context_check_initial_frame
// Description: Validates a prepared but not yet executed
//              task stack frame and its guard word.
// Parameters: task - Pointer to task context.
// Returns: 1 if the initial frame is valid, 0 otherwise.
uint8_t task_context_check_initial_frame(
    const TaskContext *task
);

#endif

#ifndef TASK_CONTEXT_H
#define TASK_CONTEXT_H

#include <stdint.h>


// Known values used to initialize each thread stack
#define TASK_CONTEXT_STACK_GUARD 0xDEADC0DEUL
#define TASK_CONTEXT_STACK_FILL  0xA5A5A5A5UL


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


// Measurements from a thread's stack watermark
typedef struct
{
    uint32_t usable_bytes;
    uint32_t peak_used_bytes;
    uint32_t minimum_free_bytes;
    uint8_t guard_intact;
} TaskStackStats;


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


// Name: task_context_get_stack_stats
// Description: Checks a stack guard and estimates peak
//              stack use using the initialization pattern.
// Parameters: task - Thread context to inspect.
//             output - Destination for stack statistics.
// Returns: 1 for a valid inspection, 0 for invalid inputs.
//          A damaged guard is reported through output.
uint8_t task_context_get_stack_stats(
    const TaskContext *task,
    TaskStackStats *output
);


#endif

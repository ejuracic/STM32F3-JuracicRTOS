#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

// Function pointer for a scheduled task
typedef void (*SchedulerTaskFunction)(void *context);

// Current cooperative task state
typedef enum
{
    SCHEDULER_TASK_WAITING = 0,
    SCHEDULER_TASK_RUNNING
} SchedulerTaskState;

// Public task diagnostic information
typedef struct
{
    const char *name;
    uint32_t period_ms;

    uint32_t execution_count;
    uint32_t late_starts;
    uint32_t skipped_periods;
    uint32_t max_lateness_ms;

    uint32_t last_execution_cycles;
    uint32_t max_execution_cycles;

    SchedulerTaskState state;
} SchedulerTaskStats;

// Name: scheduler_init
// Description: Initializes the cooperative scheduler and
//              enables available DWT cycle profiling.
// Parameters: None
// Returns: None
void scheduler_init(void);

// Name: scheduler_add_task
// Description: Registers a named periodic task.
// Parameters: name - Persistent task name string.
//             function - Task callback function.
//             context - Pointer passed to the callback.
//             period_ms - Task period in milliseconds.
// Returns: 1 on success, 0 on failure.
uint8_t scheduler_add_task(
    const char *name,
    SchedulerTaskFunction function,
    void *context,
    uint32_t period_ms
);

// Name: scheduler_get_task_count
// Description: Returns the number of registered tasks.
// Parameters: None
// Returns: Registered task count.
uint32_t scheduler_get_task_count(void);

// Name: scheduler_get_task_stats
// Description: Copies diagnostic information for a task.
// Parameters: index - Index of the registered task.
//             output - Destination structure.
// Returns: 1 on success, 0 on failure.
uint8_t scheduler_get_task_stats(
    uint32_t index,
    SchedulerTaskStats *output
);

// Name: scheduler_profiling_available
// Description: Indicates whether the DWT cycle counter is active.
// Parameters: None
// Returns: 1 if profiling is available, 0 otherwise.
uint8_t scheduler_profiling_available(void);

// Name: scheduler_run
// Description: Dispatches scheduled tasks and sleeps when idle.
// Parameters: None
// Returns: Does not return during normal operation.
void scheduler_run(void);

#endif
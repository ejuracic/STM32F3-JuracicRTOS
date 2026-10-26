
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

// A function pointer representing a scheduled task
typedef void (*SchedulerTaskFunction)(void *context);

// Name: scheduler_init
// Description: Initializes the cooperative task scheduler.
// Parameters: None
// Returns: None
void scheduler_init(void);

// Name: scheduler_add_task
// Description: Registers a task to execute periodically.
// Parameters: function - Function executed by the scheduler.
//             context - Pointer passed to the function.
//             period_ms - Task execution interval in milliseconds.
// Returns: 1 if registered successfully, 0 otherwise.
uint8_t scheduler_add_task(
    SchedulerTaskFunction function,
    void *context,
    uint32_t period_ms
);

// Name: scheduler_run
// Description: Continuously executes tasks when due and
//              sleeps using WFI when no tasks are ready.
// Parameters: None
// Returns: Does not return during normal operation.
void scheduler_run(void);

#endif

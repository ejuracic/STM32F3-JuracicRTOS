
#include <stdint.h>
#include "scheduler.h"
#include "systick.h"

#define SCHEDULER_MAX_TASKS 8U

// Information maintained for each registered task
typedef struct
{
    SchedulerTaskFunction function;
    void *context;
    uint32_t period_ms;
    uint32_t next_run_ms;
} SchedulerTask;

// Task table managed by the scheduler
static SchedulerTask tasks[SCHEDULER_MAX_TASKS];
static uint32_t task_count = 0U;

// Name: scheduler_init
// Description: Resets the registered task count.
// Parameters: None
// Returns: None
void scheduler_init(void)
{
    task_count = 0U;
}

// Name: scheduler_add_task
// Description: Adds a periodic task to the scheduler table.
// Parameters: function - Task function pointer.
//             context - Task-specific state pointer.
//             period_ms - Task execution interval in milliseconds.
// Returns: 1 on success, 0 on failure.
uint8_t scheduler_add_task(
    SchedulerTaskFunction function,
    void *context,
    uint32_t period_ms
)
{
    // Reject invalid tasks or a full task table
    if ((function == 0) ||
        (period_ms == 0U) ||
        (period_ms > INT32_MAX) ||
        (task_count >= SCHEDULER_MAX_TASKS))
    {
        return 0U;
    }

    SchedulerTask *task = &tasks[task_count];

    task->function = function;
    task->context = context;
    task->period_ms = period_ms;
    task->next_run_ms =
        systick_get_milliseconds() + period_ms;

    task_count++;

    return 1U;
}

// Name: scheduler_run
// Description: Checks each task's deadline, executes due
//              tasks, and sleeps when no work is ready.
// Parameters: None
// Returns: Does not return during normal operation.
void scheduler_run(void)
{
    for (;;)
    {
        uint8_t executed_task = 0U;

        for (uint32_t i = 0U; i < task_count; i++)
        {
            uint32_t now = systick_get_milliseconds();
            SchedulerTask *task = &tasks[i];

            // Check whether this task has reached its deadline
            if ((int32_t)(now - task->next_run_ms) >= 0)
            {
                // Schedule the next execution
                task->next_run_ms = now + task->period_ms;

                // Execute the registered task
                task->function(task->context);

                executed_task = 1U;
            }
        }

        // Sleep until the next interrupt if no task ran
        if (executed_task == 0U)
        {
            __asm volatile ("wfi");
        }
    }
}

#include <stdint.h>
#include "scheduler.h"
#include "systick.h"

#define SCHEDULER_MAX_TASKS 8U

// ARM Cortex-M4 debug and cycle counter registers
#define SCB_DEMCR   (*(volatile uint32_t *)0xE000EDFCUL)
#define DWT_CTRL    (*(volatile uint32_t *)0xE0001000UL)
#define DWT_CYCCNT  (*(volatile uint32_t *)0xE0001004UL)

// Internal scheduler task information
typedef struct
{
    SchedulerTaskFunction function;
    void *context;
    uint32_t next_run_ms;
    SchedulerTaskStats stats;
} SchedulerTask;

// Registered task table
static SchedulerTask tasks[SCHEDULER_MAX_TASKS];
static uint32_t task_count = 0U;
static uint8_t profiling_enabled = 0U;

// Name: scheduler_init
// Description: Resets the scheduler and initializes the
//              Cortex-M4 DWT cycle counter if available.
// Parameters: None
// Returns: None
void scheduler_init(void)
{
    task_count = 0U;
    profiling_enabled = 0U;

    // Enable access to trace and debug registers
    SCB_DEMCR |= (1UL << 24);

    // Check whether a cycle counter is supported
    if ((DWT_CTRL & (1UL << 25)) == 0U)
    {
        // Reset the cycle count
        DWT_CYCCNT = 0U;

        // Enable cycle counting
        DWT_CTRL |= (1UL << 0);

        // Verify that the counter is advancing
        uint32_t before = DWT_CYCCNT;

        for (volatile uint32_t i = 0U; i < 32U; i++)
        {
            __asm volatile ("nop");
        }

        if (DWT_CYCCNT != before)
        {
            profiling_enabled = 1U;
        }
    }
}

// Name: scheduler_add_task
// Description: Registers a named periodic task and initializes
//              its diagnostic information.
// Parameters: name - Task name with static lifetime.
//             function - Task callback.
//             context - Application-specific context pointer.
//             period_ms - Task execution period.
// Returns: 1 on success, 0 on failure.
uint8_t scheduler_add_task(
    const char *name,
    SchedulerTaskFunction function,
    void *context,
    uint32_t period_ms
)
{
    if ((name == 0) ||
        (function == 0) ||
        (period_ms == 0U) ||
        (period_ms > INT32_MAX) ||
        (task_count >= SCHEDULER_MAX_TASKS))
    {
        return 0U;
    }

    SchedulerTask *task = &tasks[task_count];

    task->function = function;
    task->context = context;
    task->next_run_ms =
        systick_get_milliseconds() + period_ms;

    // Initialize diagnostic information
    task->stats = (SchedulerTaskStats){0};

    task->stats.name = name;
    task->stats.period_ms = period_ms;
    task->stats.state = SCHEDULER_TASK_WAITING;

    task_count++;

    return 1U;
}

// Name: scheduler_get_task_count
// Description: Gets the number of registered tasks.
// Parameters: None
// Returns: Registered task count.
uint32_t scheduler_get_task_count(void)
{
    return task_count;
}

// Name: scheduler_get_task_stats
// Description: Copies the diagnostic data for a registered task.
// Parameters: index - Task index.
//             output - Destination for diagnostic information.
// Returns: 1 if successful, 0 otherwise.
uint8_t scheduler_get_task_stats(
    uint32_t index,
    SchedulerTaskStats *output
)
{
    if ((index >= task_count) || (output == 0))
    {
        return 0U;
    }

    *output = tasks[index].stats;

    return 1U;
}

// Name: scheduler_profiling_available
// Description: Checks whether DWT execution profiling is active.
// Parameters: None
// Returns: 1 when profiling is enabled, 0 otherwise.
uint8_t scheduler_profiling_available(void)
{
    return profiling_enabled;
}

// Name: scheduler_run
// Description: Executes due tasks, records scheduling and
//              execution statistics, and sleeps when idle.
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

            // Determine whether the task is due
            if ((int32_t)(now - task->next_run_ms) >= 0)
            {
                // Measure how late the task started
                uint32_t lateness_ms =
                    now - task->next_run_ms;

                if (lateness_ms > 0U)
                {
                    task->stats.late_starts++;

                    if (lateness_ms >
                        task->stats.max_lateness_ms)
                    {
                        task->stats.max_lateness_ms =
                            lateness_ms;
                    }
                }

                // Count periodic releases that were skipped
                task->stats.skipped_periods +=
                    lateness_ms / task->stats.period_ms;

                // Schedule the next execution from now
                task->next_run_ms =
                    now + task->stats.period_ms;

                // Mark task as running
                task->stats.state = SCHEDULER_TASK_RUNNING;
                task->stats.execution_count++;

                // Capture the initial processor cycle count
                uint32_t start_cycles =
                    (profiling_enabled != 0U)
                    ? DWT_CYCCNT : 0U;

                // Execute the actual task callback
                task->function(task->context);

                // Calculate callback execution time
                if (profiling_enabled != 0U)
                {
                    uint32_t elapsed_cycles =
                        DWT_CYCCNT - start_cycles;

                    task->stats.last_execution_cycles =
                        elapsed_cycles;

                    if (elapsed_cycles >
                        task->stats.max_execution_cycles)
                    {
                        task->stats.max_execution_cycles =
                            elapsed_cycles;
                    }
                }

                // Task has finished its scheduled callback
                task->stats.state = SCHEDULER_TASK_WAITING;

                executed_task = 1U;
            }
        }

        // Sleep when no scheduled tasks were executed
        if (executed_task == 0U)
        {
            __asm volatile ("wfi");
        }
    }
}

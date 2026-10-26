#ifndef CONTEXT_SWITCH_H
#define CONTEXT_SWITCH_H

#include <stdint.h>
#include "task_context.h"

// Maximum number of application threads
#define CONTEXT_MAX_USER_THREADS 4U

// Thread execution states
typedef enum
{
    CONTEXT_THREAD_READY = 0,
    CONTEXT_THREAD_RUNNING,
    CONTEXT_THREAD_SLEEPING,
    CONTEXT_THREAD_INVALID
} ContextThreadState;

// Public thread diagnostic information
typedef struct
{
    const char *name;
    ContextThreadState state;
    uint32_t wake_ms;
    uint32_t dispatch_count;
} ContextThreadInfo;

// Name: context_switch_init
// Description: Initializes the thread scheduler and
//              registers the dedicated Idle Thread.
// Parameters: idle - Prepared Idle Thread context.
// Returns: 1 on success, 0 on failure.
uint8_t context_switch_init(TaskContext *idle);

// Name: context_switch_add_thread
// Description: Registers an application thread before
//              the scheduler starts.
// Parameters: name - Persistent thread name.
//             context - Prepared task context.
// Returns: 1 on success, 0 on failure.
uint8_t context_switch_add_thread(
    const char *name,
    TaskContext *context
);

// Name: context_switch_start
// Description: Starts the first registered thread via SVC.
// Parameters: None
// Returns: Does not return during normal operation.
void context_switch_start(void);

// Name: context_switch_yield
// Description: Requests a voluntary scheduling decision.
// Parameters: None
// Returns: None
void context_switch_yield(void);

// Name: context_switch_sleep
// Description: Suspends the calling application thread
//              for a specified number of milliseconds.
// Parameters: milliseconds - Sleep duration.
// Returns: None
void context_switch_sleep(uint32_t milliseconds);

// Name: context_switch_enable_preemption
// Description: Enables SysTick-driven scheduling requests.
// Parameters: None
// Returns: None
void context_switch_enable_preemption(void);

// Name: context_switch_on_systick
// Description: Requests PendSV when preemption is active.
// Parameters: None
// Returns: None
void context_switch_on_systick(void);

// Name: context_switch_get_count
// Description: Returns the number of actual thread changes.
// Parameters: None
// Returns: Completed context-switch count.
uint32_t context_switch_get_count(void);

// Name: context_switch_get_thread_count
// Description: Counts registered threads including Idle.
// Parameters: None
// Returns: Registered thread count.
uint32_t context_switch_get_thread_count(void);

// Name: context_switch_get_thread_info
// Description: Copies one thread's diagnostic information.
// Parameters: index - Registered application thread index,
//             or final index for the Idle Thread.
//             output - Destination for the information.
// Returns: 1 on success, 0 on failure.
uint8_t context_switch_get_thread_info(
    uint32_t index,
    ContextThreadInfo *output
);

// Name: context_switch_get_thread_state
// Description: Reads a thread's execution state.
// Parameters: index - Thread index including Idle.
// Returns: Thread state or INVALID.
ContextThreadState context_switch_get_thread_state(
    uint32_t index
);

#endif

#ifndef CONTEXT_SWITCH_H
#define CONTEXT_SWITCH_H

#include <stdint.h>
#include "task_context.h"

// Thread execution states
typedef enum
{
    CONTEXT_THREAD_READY = 0,
    CONTEXT_THREAD_RUNNING,
    CONTEXT_THREAD_SLEEPING,
    CONTEXT_THREAD_INVALID
} ContextThreadState;

// Name: context_switch_configure
// Description: Registers two user threads and an idle thread.
// Parameters: first - Initial user thread context.
//             second - Second user thread context.
//             idle - Idle thread context.
// Returns: None
void context_switch_configure(
    TaskContext *first,
    TaskContext *second,
    TaskContext *idle
);

// Name: context_switch_start
// Description: Starts the first thread through SVC.
// Parameters: None
// Returns: Does not return during normal operation.
void context_switch_start(void);

// Name: context_switch_yield
// Description: Requests a voluntary scheduling decision.
// Parameters: None
// Returns: None
void context_switch_yield(void);

// Name: context_switch_sleep
// Description: Suspends the calling thread for a duration.
// Parameters: milliseconds - Requested sleep duration.
// Returns: None
void context_switch_sleep(uint32_t milliseconds);

// Name: context_switch_enable_preemption
// Description: Enables timer-driven scheduling requests.
// Parameters: None
// Returns: None
void context_switch_enable_preemption(void);

// Name: context_switch_on_systick
// Description: Requests scheduling on each active SysTick.
// Parameters: None
// Returns: None
void context_switch_on_systick(void);

// Name: context_switch_get_count
// Description: Gets the completed thread-switch count.
// Parameters: None
// Returns: Number of actual thread changes.
uint32_t context_switch_get_count(void);

// Name: context_switch_get_thread_state
// Description: Gets the execution state of a thread.
// Parameters: index - Thread index, 0, 1, or 2.
// Returns: Thread state or INVALID for an invalid index.
ContextThreadState context_switch_get_thread_state(
    uint32_t index
);

#endif

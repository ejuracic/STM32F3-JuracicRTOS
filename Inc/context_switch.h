#ifndef CONTEXT_SWITCH_H
#define CONTEXT_SWITCH_H

#include <stdint.h>
#include "task_context.h"

// Name: context_switch_configure
// Description: Registers two prepared task contexts for
//              the initial two-thread demonstration.
// Parameters: first - Initial thread context.
//             second - Second thread context.
// Returns: None
void context_switch_configure(
    TaskContext *first,
    TaskContext *second
);

// Name: context_switch_start
// Description: Starts the first thread using an SVC exception.
// Parameters: None
// Returns: Does not return during normal operation.
void context_switch_start(void);

// Name: context_switch_yield
// Description: Requests a switch to the other thread
//              by pending the PendSV exception.
// Parameters: None
// Returns: None
void context_switch_yield(void);


// Name: context_switch_enable_preemption
// Description: Arms automatic thread switching once
//              the first thread has started.
// Parameters: None
// Returns: None
void context_switch_enable_preemption(void);

// Name: context_switch_on_systick
// Description: Called by SysTick to request a thread
//              switch when preemption is active.
// Parameters: None
// Returns: None
void context_switch_on_systick(void);


// Name: context_switch_get_count
// Description: Gets the number of completed thread switches.
// Parameters: None
// Returns: Completed switch count.
uint32_t context_switch_get_count(void);

#endif

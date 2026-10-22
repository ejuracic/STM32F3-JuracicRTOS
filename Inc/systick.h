#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

// Name: systick_init
// Description: Initializes the SysTick timer to generate an
// interrupt every 1 millisecond.
// Parameters: None
// Returns: None
void systick_init(void);

// Name: systick_get_milliseconds
// Description: Returns the number of milliseconds elapsed
// since the SysTick timer was initialized.
// Parameters: None
// Returns: Number of elapsed milliseconds.
uint32_t systick_get_milliseconds(void);

#endif

#include <stdint.h>
#include "systick.h"

// SysTick register addresses
#define SYST_CSR  (*(volatile uint32_t *)0xE000E010UL)
#define SYST_RVR  (*(volatile uint32_t *)0xE000E014UL)
#define SYST_CVR  (*(volatile uint32_t *)0xE000E018UL)

// Timing configuration
#define CPU_CLOCK_HZ 8000000UL
#define TICK_RATE_HZ 1000UL

// Milliseconds elapsed since SysTick initialization
static volatile uint32_t g_milliseconds = 0U;

// Name: systick_init
// Description: Initializes the SysTick hardware timer to generate
//              an interrupt every 1 millisecond.
// Parameters: None
// Returns: None
void systick_init(void)
{
    // Disable SysTick during configuration
    SYST_CSR = 0U;

    // Reset the millisecond counter
    g_milliseconds = 0U;

    // Configure SysTick reload value
    SYST_RVR = (CPU_CLOCK_HZ / TICK_RATE_HZ) - 1UL;

    // Clear the current counter value
    SYST_CVR = 0U;

    // Enable SysTick, interrupt, and processor clock
    SYST_CSR = (1UL << 2) | (1UL << 1) | (1UL << 0);
}

// Name: systick_get_milliseconds
// Description: Retrieves the number of milliseconds elapsed
//              since the SysTick timer was initialized.
// Parameters: None
// Returns: Number of elapsed milliseconds.
uint32_t systick_get_milliseconds(void)
{
    return g_milliseconds;
}

// Name: SysTick_Handler
// Description: Handles SysTick interrupts by incrementing
//              the system millisecond counter every 1 ms.
// Parameters: None
// Returns: None
void SysTick_Handler(void)
{
    g_milliseconds++;
}

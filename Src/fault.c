#include <stdint.h>
#include "fault.h"

// ARM System Control Block fault registers
#define SCB_SHCSR (*(volatile uint32_t *)0xE000ED24UL)
#define SCB_CFSR  (*(volatile uint32_t *)0xE000ED28UL)
#define SCB_HFSR  (*(volatile uint32_t *)0xE000ED2CUL)
#define SCB_DFSR  (*(volatile uint32_t *)0xE000ED30UL)
#define SCB_MMFAR (*(volatile uint32_t *)0xE000ED34UL)
#define SCB_BFAR  (*(volatile uint32_t *)0xE000ED38UL)

// Debug Halting Control and Status Register
#define COREDEBUG_DHCSR (*(volatile uint32_t *)0xE000EDF0UL)

#define COREDEBUG_DEBUG_ENABLED (1UL << 0)

// Fault status validity flags
#define CFSR_MMFAR_VALID (1UL << 7)
#define CFSR_BFAR_VALID  (1UL << 15)

// Saved snapshot inspected through GDB
volatile FaultSnapshot g_fault_snapshot;

// Name: fault_capture
// Description: Records the exception frame and fault
//              status before stopping execution.
// Parameters: fault_stack - Stack used on exception entry.
//             exc_return - Exception return value from LR.
// Returns: Does not return.
__attribute__((used, noinline, noreturn))
void fault_capture(
    const uint32_t *fault_stack,
    uint32_t exc_return
);

// Name: HardFault_Handler
// Description: Determines which stack contains the
//              interrupted CPU register frame.
// Parameters: None
// Returns: Branches to fault_capture.
__attribute__((naked))
void HardFault_Handler(void)
{
    __asm volatile (
        // Prevent ordinary interrupt activity
        "cpsid i\n"

        // EXC_RETURN bit 2 selects MSP or PSP
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"

        // Pass EXC_RETURN as the second argument
        "mov r1, lr\n"

        // Continue into the non-returning C fault handler
        "b fault_capture\n"
    );
}

// Name: fault_capture
// Description: Saves the faulting context and SCB fault
//              registers into a debugger-visible snapshot.
// Parameters: fault_stack - Exception frame pointer.
//             exc_return - Exception return code.
// Returns: Does not return.
__attribute__((used, noinline, noreturn))
void fault_capture(
    const uint32_t *fault_stack,
    uint32_t exc_return
)
{
    // Mark the snapshot incomplete while capturing
    g_fault_snapshot.valid = 0U;

    // Save exception-return and stack-selection information
    g_fault_snapshot.exception_return = exc_return;
    g_fault_snapshot.exception_stack_pointer =
        (uint32_t)(uintptr_t)fault_stack;

    g_fault_snapshot.used_psp =
        ((exc_return & (1UL << 2)) != 0U) ? 1U : 0U;

    g_fault_snapshot.extended_frame =
        ((exc_return & (1UL << 4)) == 0U) ? 1U : 0U;

    // Read the automatically stacked core register frame
    g_fault_snapshot.r0   = fault_stack[0];
    g_fault_snapshot.r1   = fault_stack[1];
    g_fault_snapshot.r2   = fault_stack[2];
    g_fault_snapshot.r3   = fault_stack[3];
    g_fault_snapshot.r12  = fault_stack[4];
    g_fault_snapshot.lr   = fault_stack[5];
    g_fault_snapshot.pc   = fault_stack[6];
    g_fault_snapshot.xpsr = fault_stack[7];

    // Capture processor fault status registers
    uint32_t cfsr = SCB_CFSR;

    g_fault_snapshot.cfsr = cfsr;
    g_fault_snapshot.hfsr = SCB_HFSR;
    g_fault_snapshot.dfsr = SCB_DFSR;
    g_fault_snapshot.shcsr = SCB_SHCSR;

    // Check whether the fault-address registers are valid
    g_fault_snapshot.mmfar_valid =
        ((cfsr & CFSR_MMFAR_VALID) != 0U) ? 1U : 0U;

    g_fault_snapshot.bfar_valid =
        ((cfsr & CFSR_BFAR_VALID) != 0U) ? 1U : 0U;

    // Read addresses only when their valid flags are set
    g_fault_snapshot.mmfar =
        (g_fault_snapshot.mmfar_valid != 0U)
        ? SCB_MMFAR : 0U;

    g_fault_snapshot.bfar =
        (g_fault_snapshot.bfar_valid != 0U)
        ? SCB_BFAR : 0U;

    // Mark all snapshot fields as captured
    g_fault_snapshot.valid = FAULT_SNAPSHOT_MAGIC;

    // Break into the debugger if one is attached
    if ((COREDEBUG_DHCSR & COREDEBUG_DEBUG_ENABLED) != 0U)
    {
        __asm volatile ("bkpt #0");
    }

    // Never resume execution after an unhandled fault
    for (;;)
    {
        __asm volatile ("nop");
    }
}

// Name: fault_test_trigger
// Description: Executes an undefined Thumb instruction
//              to deliberately enter the fault handler.
// Parameters: None
// Returns: Does not return during the fault test.
__attribute__((used, noinline))
void fault_test_trigger(void)
{
    __asm volatile ("udf #0");
}

#ifndef FAULT_H
#define FAULT_H

#include <stdint.h>

// Identifies a completed fault snapshot
#define FAULT_SNAPSHOT_MAGIC 0x4641554CUL

// Saved CPU state and fault status registers
typedef struct
{
    uint32_t valid;

    uint32_t exception_return;
    uint32_t exception_stack_pointer;
    uint32_t used_psp;
    uint32_t extended_frame;

    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;

    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t dfsr;
    uint32_t shcsr;

    uint32_t mmfar;
    uint32_t bfar;

    uint32_t mmfar_valid;
    uint32_t bfar_valid;
} FaultSnapshot;

// Global snapshot preserved while the processor is halted
extern volatile FaultSnapshot g_fault_snapshot;

// Name: fault_test_trigger
// Description: Executes a deliberately undefined
//              instruction to test fault diagnostics.
// Parameters: None
// Returns: Does not return during the fault test.
void fault_test_trigger(void);

#endif

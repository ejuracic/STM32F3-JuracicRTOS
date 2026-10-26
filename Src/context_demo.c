#include <stdint.h>
#include "gpio.h"
#include "systick.h"
#include "uart.h"
#include "task_context.h"
#include "context_switch.h"
#include "context_demo.h"

#define DEMO_STACK_WORDS 256U

// Independent 1024-byte stacks for three threads
static uint32_t g_thread_a_stack[DEMO_STACK_WORDS]
    __attribute__((aligned(8)));

static uint32_t g_thread_b_stack[DEMO_STACK_WORDS]
    __attribute__((aligned(8)));

static uint32_t g_idle_stack[DEMO_STACK_WORDS]
    __attribute__((aligned(8)));

// Independent saved execution contexts
static TaskContext g_thread_a_context;
static TaskContext g_thread_b_context;
static TaskContext g_idle_context;

// Shared demonstration counters
static volatile uint32_t g_thread_a_toggles = 0U;
static volatile uint32_t g_thread_b_reports = 0U;

// Name: demo_thread_a
// Description: Toggles the blue LED and sleeps for 500 ms.
// Parameters: argument - Unused task argument.
// Returns: Does not return.
static void demo_thread_a(void *argument)
{
    (void)argument;

    uint8_t led_on = 0U;

    gpio_led_off();

    for (;;)
    {
        // Suspend this thread for 500 milliseconds
        context_switch_sleep(500U);

        // Toggle the LED after the thread wakes
        led_on = (led_on == 0U) ? 1U : 0U;

        if (led_on != 0U)
        {
            gpio_led_on();
        }
        else
        {
            gpio_led_off();
        }

        g_thread_a_toggles++;
    }
}

// Name: demo_thread_b
// Description: Reports thread activity every two seconds.
// Parameters: argument - Unused task argument.
// Returns: Does not return.
static void demo_thread_b(void *argument)
{
    (void)argument;

    for (;;)
    {
        // Suspend this thread for two seconds
        context_switch_sleep(2000U);

        g_thread_b_reports++;

        uart_write_string("\r\n[JuracicRTOS Threads] ");

        uart_write_uint32(
            systick_get_milliseconds() / 1000U
        );

        uart_write_string("s\r\n");

        // Report Thread A's LED activity
        uart_write_string("LED toggles: ");
        uart_write_uint32(g_thread_a_toggles);

        uart_write_string(" | Reports: ");
        uart_write_uint32(g_thread_b_reports);
        uart_write_string("\r\n");

        // Report actual thread changes
        uart_write_string("Context switches: ");
        uart_write_uint32(context_switch_get_count());
        uart_write_string("\r\n");

        // Inspect Thread A's current scheduling state
        uart_write_string("Thread A state: ");

        if (context_switch_get_thread_state(0U) ==
            CONTEXT_THREAD_SLEEPING)
        {
            uart_write_string("SLEEPING\r\n");
        }
        else
        {
            uart_write_string("READY or RUNNING\r\n");
        }
    }
}

// Name: demo_idle_thread
// Description: Waits for interrupts when no user thread
//              is currently available to execute.
// Parameters: argument - Unused task argument.
// Returns: Does not return.
static void demo_idle_thread(void *argument)
{
    (void)argument;

    for (;;)
    {
        // Sleep until an interrupt occurs
        __asm volatile ("wfi");
    }
}

// Name: context_demo_start
// Description: Initializes and starts two user threads
//              and one idle thread with independent stacks.
// Parameters: None
// Returns: Does not return during normal operation.
void context_demo_start(void)
{
    // Initialize demonstration hardware
    gpio_led_init();
    systick_init();
    uart_init();

    uart_write_string(
        "\r\nJuracicRTOS thread-state demo\r\n"
    );

    // Prepare Thread A
    uint8_t a_ready = task_context_init(
        &g_thread_a_context,
        g_thread_a_stack,
        DEMO_STACK_WORDS,
        demo_thread_a,
        0
    );

    // Prepare Thread B
    uint8_t b_ready = task_context_init(
        &g_thread_b_context,
        g_thread_b_stack,
        DEMO_STACK_WORDS,
        demo_thread_b,
        0
    );

    // Prepare the Idle Thread
    uint8_t idle_ready = task_context_init(
        &g_idle_context,
        g_idle_stack,
        DEMO_STACK_WORDS,
        demo_idle_thread,
        0
    );

    // Check initial stack creation
    if ((a_ready == 0U) ||
        (b_ready == 0U) ||
        (idle_ready == 0U))
    {
        uart_write_string(
            "ERROR: Stack initialization failed\r\n"
        );

        for (;;)
        {
            __asm volatile ("wfi");
        }
    }

    // Validate each prepared register frame
    if ((task_context_check_initial_frame(
            &g_thread_a_context) == 0U) ||
        (task_context_check_initial_frame(
            &g_thread_b_context) == 0U) ||
        (task_context_check_initial_frame(
            &g_idle_context) == 0U))
    {
        uart_write_string(
            "ERROR: Stack validation failed\r\n"
        );

        for (;;)
        {
            __asm volatile ("wfi");
        }
    }

    // Configure our three-thread scheduler
    context_switch_configure(
        &g_thread_a_context,
        &g_thread_b_context,
        &g_idle_context
    );

    // Enable timer-driven scheduling
    context_switch_enable_preemption();

    uart_write_string(
        "Three thread stacks ready\r\n"
    );

    uart_write_string(
        "Starting the thread-state scheduler\r\n"
    );

    // Launch the initial thread through SVC
    context_switch_start();

    // Execution should never reach this section
    for (;;)
    {
        __asm volatile ("wfi");
    }
}

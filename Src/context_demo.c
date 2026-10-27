#include <stdint.h>
#include "gpio.h"
#include "systick.h"
#include "uart.h"
#include "task_context.h"
#include "context_switch.h"
#include "context_demo.h"
#include "fault.h"

// Demonstration configuration
#define DEMO_STACK_WORDS 256U
#define DEMO_LED_INTERVAL_MS 500U
#define DEMO_BUTTON_INTERVAL_MS 10U
#define DEMO_BUTTON_DEBOUNCE_MS 30U
#define DEMO_REPORT_INTERVAL_MS 2000U

// Set to 1 only during the deliberate fault test
#define JURACIC_FAULT_TEST 0U

// Four independent 1024-byte task stacks
static uint32_t g_led_stack[DEMO_STACK_WORDS]
    __attribute__((aligned(8)));

static uint32_t g_button_stack[DEMO_STACK_WORDS]
    __attribute__((aligned(8)));

static uint32_t g_monitor_stack[DEMO_STACK_WORDS]
    __attribute__((aligned(8)));

static uint32_t g_idle_stack[DEMO_STACK_WORDS]
    __attribute__((aligned(8)));

// Prepared execution contexts
static TaskContext g_led_context;
static TaskContext g_button_context;
static TaskContext g_monitor_context;
static TaskContext g_idle_context;

// Shared demonstration state
static volatile uint32_t g_led_toggles = 0U;
static volatile uint32_t g_button_presses = 0U;

// Name: demo_led_thread
// Description: Toggles the blue LED every 500 ms.
// Parameters: argument - Unused thread argument.
// Returns: Does not return.
static void demo_led_thread(void *argument)
{
    (void)argument;

    uint8_t led_on = 0U;

    gpio_led_off();

    for (;;)
    {
        context_switch_sleep(DEMO_LED_INTERVAL_MS);

        led_on = (led_on == 0U) ? 1U : 0U;

        if (led_on != 0U)
        {
            gpio_led_on();
        }
        else
        {
            gpio_led_off();
        }

        g_led_toggles++;
    }
}

// Name: demo_button_thread
// Description: Polls and debounces the USER button using
//              an independent thread stack.
// Parameters: argument - Unused thread argument.
// Returns: Does not return.
static void demo_button_thread(void *argument)
{
    (void)argument;

    uint8_t raw_state = gpio_button_is_pressed();
    uint8_t stable_state = raw_state;
    uint32_t last_change_ms = systick_get_milliseconds();

    for (;;)
    {
        uint32_t now = systick_get_milliseconds();
        uint8_t raw = gpio_button_is_pressed();

        // Record changes to the instantaneous button state
        if (raw != raw_state)
        {
            raw_state = raw;
            last_change_ms = now;
        }

        // Accept a new state after the debounce interval
        if ((stable_state != raw_state) &&
            ((uint32_t)(now - last_change_ms) >=
             DEMO_BUTTON_DEBOUNCE_MS))
        {
            stable_state = raw_state;

            // Count each debounced press only once
            if (stable_state != 0U)
            {
                g_button_presses++;
            }
        }

        // Suspend the button thread until its next poll
        context_switch_sleep(DEMO_BUTTON_INTERVAL_MS);
    }
}

// Name: demo_thread_state_name
// Description: Converts a thread state to a readable name.
// Parameters: state - Thread state.
// Returns: String describing the state.
static const char *demo_thread_state_name(
    ContextThreadState state
)
{
    switch (state)
    {
        case CONTEXT_THREAD_READY:
            return "READY";

        case CONTEXT_THREAD_RUNNING:
            return "RUNNING";

        case CONTEXT_THREAD_SLEEPING:
            return "SLEEPING";

        default:
            return "INVALID";
    }
}


// Name: demo_report_threads
// Description: Reports stack usage, guard status, and
//              execution state for each registered thread.
// Parameters: None
// Returns: None
static void demo_report_threads(void)
{
    uint32_t count = context_switch_get_thread_count();

    uart_write_string(
        "\r\n--- Thread Stack Diagnostics ---\r\n"
    );

    for (uint32_t i = 0U; i < count; i++)
    {
        ContextThreadInfo info;

        if (context_switch_get_thread_info(i, &info) == 0U)
        {
            uart_write_string("ERROR: Could not inspect thread ");
            uart_write_uint32(i);
            uart_write_string("\r\n");
            continue;
        }

        uart_write_string(info.name);
        uart_write_string(": peak=");

        uart_write_uint32(info.stack.peak_used_bytes);
        uart_write_string("/");
        uart_write_uint32(info.stack.usable_bytes);

        uart_write_string(" B | min_free=");
        uart_write_uint32(info.stack.minimum_free_bytes);

        uart_write_string(" B | guard=");

        if (info.stack.guard_intact != 0U)
        {
            uart_write_string("OK");
        }
        else
        {
            uart_write_string("FAIL");
        }

        uart_write_string(" | state=");
        uart_write_string(
            demo_thread_state_name(info.state)
        );

        uart_write_string("\r\n");
    }
}


// Name: demo_monitor_thread
// Description: Periodically reports hardware activity
//              and registered thread states through UART.
// Parameters: argument - Unused thread argument.
// Returns: Does not return.
static void demo_monitor_thread(void *argument)
{
    (void)argument;

    for (;;)
    {
        context_switch_sleep(DEMO_REPORT_INTERVAL_MS);

        uint32_t now = systick_get_milliseconds();

        // Print compact system information
        uart_write_string("\r\n=== JuracicRTOS Threads | ");

        uart_write_uint32(now / 1000U);
        uart_write_string("s ===\r\n");

        uart_write_string("LED Toggles: ");
        uart_write_uint32(g_led_toggles);

        uart_write_string(" | Button Presses: ");
        uart_write_uint32(g_button_presses);
        uart_write_string("\r\n");


        // Print lightweight scheduling information
        uart_write_string("Threads: ");
        uart_write_uint32(context_switch_get_thread_count());

        uart_write_string(" | Switches: ");
        uart_write_uint32(context_switch_get_count());
        uart_write_string("\r\n");

        // Collect detailed stack statistics every 10 seconds
        static uint32_t last_stack_report_ms = 0U;

        if ((uint32_t)(now - last_stack_report_ms) >= 10000U)
        {
            last_stack_report_ms = now;
            demo_report_threads();
        }


        #if JURACIC_FAULT_TEST
                // Trigger one controlled fault after approximately 6 seconds
                if (now >= 6000U)
                {
                    uart_write_string(
                        "\r\nFAULT TEST: Triggering undefined instruction\r\n"
                    );

                    // Execution deliberately stops in the fault handler
                    fault_test_trigger();
                }
        #endif


    }
}

// Name: demo_idle_thread
// Description: Waits for interrupts when no application
//              thread is ready to execute.
// Parameters: argument - Unused thread argument.
// Returns: Does not return.
static void demo_idle_thread(void *argument)
{
    (void)argument;

    for (;;)
    {
        __asm volatile ("wfi");
    }
}

// Name: context_demo_start
// Description: Initializes hardware, prepares independent
//              stacks, registers threads, and starts the kernel.
// Parameters: None
// Returns: Does not return during normal operation.
void context_demo_start(void)
{
    // Initialize demonstration hardware
    gpio_led_init();
    gpio_button_init();
    systick_init();
    uart_init();

    uart_write_string(
        "\r\nJuracicRTOS thread registration demo\r\n"
    );

    // Prepare all independent task contexts
    uint8_t led_ready = task_context_init(
        &g_led_context,
        g_led_stack,
        DEMO_STACK_WORDS,
        demo_led_thread,
        0
    );

    uint8_t button_ready = task_context_init(
        &g_button_context,
        g_button_stack,
        DEMO_STACK_WORDS,
        demo_button_thread,
        0
    );

    uint8_t monitor_ready = task_context_init(
        &g_monitor_context,
        g_monitor_stack,
        DEMO_STACK_WORDS,
        demo_monitor_thread,
        0
    );

    uint8_t idle_ready = task_context_init(
        &g_idle_context,
        g_idle_stack,
        DEMO_STACK_WORDS,
        demo_idle_thread,
        0
    );

    // Stop if any stack could not be prepared
    if ((led_ready == 0U) ||
        (button_ready == 0U) ||
        (monitor_ready == 0U) ||
        (idle_ready == 0U))
    {
        uart_write_string(
            "ERROR: Thread stack initialization failed\r\n"
        );

        for (;;)
        {
            __asm volatile ("wfi");
        }
    }


    // Test guard detection before any threads begin running
    TaskStackStats guard_test;

    if ((task_context_get_stack_stats(
            &g_led_context, &guard_test) == 0U) ||
        (guard_test.guard_intact == 0U))
    {
        uart_write_string(
            "ERROR: Initial LED stack guard invalid\r\n"
        );

        for (;;)
        {
            __asm volatile ("wfi");
        }
    }

    // Temporarily corrupt the guard on an inactive stack
    uint32_t original_guard = g_led_stack[0];
    g_led_stack[0] = 0U;

    uint8_t corruption_detected =
        (task_context_get_stack_stats(
            &g_led_context, &guard_test) != 0U) &&
        (guard_test.guard_intact == 0U);

    // Restore the guard before registering the thread
    g_led_stack[0] = original_guard;

    if (corruption_detected == 0U)
    {
        uart_write_string(
            "ERROR: Stack guard self-test failed\r\n"
        );

        for (;;)
        {
            __asm volatile ("wfi");
        }
    }

    uart_write_string("Stack guard self-test: PASS\r\n");

    // Initialize the generalized thread scheduler
    if (context_switch_init(&g_idle_context) == 0U)
    {
        uart_write_string(
            "ERROR: Kernel initialization failed\r\n"
        );

        for (;;)
        {
            __asm volatile ("wfi");
        }
    }

    // Register application threads by name
    if ((context_switch_add_thread(
            "LED", &g_led_context) == 0U) ||
        (context_switch_add_thread(
            "Button", &g_button_context) == 0U) ||
        (context_switch_add_thread(
            "Monitor", &g_monitor_context) == 0U))
    {
        uart_write_string(
            "ERROR: Thread registration failed\r\n"
        );

        for (;;)
        {
            __asm volatile ("wfi");
        }
    }

    uart_write_string(
        "Registered 3 application threads + Idle\r\n"
    );

    // Enable SysTick-driven scheduling
    context_switch_enable_preemption();

    uart_write_string(
        "Starting generalized preemptive scheduler\r\n"
    );

    // Launch the first registered thread through SVC
    context_switch_start();

    for (;;)
    {
        __asm volatile ("wfi");
    }
}

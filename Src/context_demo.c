#include <stdint.h>
#include "gpio.h"
#include "systick.h"
#include "uart.h"
#include "task_context.h"
#include "context_switch.h"
#include "context_demo.h"

#define DEMO_STACK_WORDS 256U
#define DEMO_LED_INTERVAL_MS 500U
#define DEMO_REPORT_INTERVAL_MS 2000U

// Each thread has its own 1024-byte stack
static uint32_t g_thread_a_stack[DEMO_STACK_WORDS]
    __attribute__((aligned(8)));

static uint32_t g_thread_b_stack[DEMO_STACK_WORDS]
    __attribute__((aligned(8)));

// Independent saved execution contexts
static TaskContext g_thread_a_context;
static TaskContext g_thread_b_context;

// Shared demonstration counters
static volatile uint32_t g_thread_a_toggles = 0U;
static volatile uint32_t g_thread_b_reports = 0U;

// Name: demo_thread_a
// Description: Toggles the blue LED every 500 milliseconds
//              while executing on its own process stack.
// Parameters: argument - Unused for this demonstration.
// Returns: Does not return.
static void demo_thread_a(void *argument)
{
    (void)argument;

    uint32_t last_toggle_ms = systick_get_milliseconds();
    uint8_t led_on = 0U;

    gpio_led_off();

    for (;;)
    {
        uint32_t now = systick_get_milliseconds();

        if ((uint32_t)(now - last_toggle_ms)
            >= DEMO_LED_INTERVAL_MS)
        {
            last_toggle_ms = now;

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


        // Continue executing until SysTick preempts this thread
        __asm volatile ("nop");

    }
}

// Name: demo_thread_b
// Description: Prints demonstration statistics through UART
//              while executing on its own process stack.
// Parameters: argument - Unused for this demonstration.
// Returns: Does not return.
static void demo_thread_b(void *argument)
{
    (void)argument;

    uint32_t last_report_ms = systick_get_milliseconds();

    for (;;)
    {
        uint32_t now = systick_get_milliseconds();

        if ((uint32_t)(now - last_report_ms)
            >= DEMO_REPORT_INTERVAL_MS)
        {
            last_report_ms = now;
            g_thread_b_reports++;

            uart_write_string(
                "\r\n[Thread B] Context-switching demo\r\n"
            );

            uart_write_string("Uptime: ");
            uart_write_uint32(now);
            uart_write_string(" ms\r\n");

            uart_write_string("Thread A LED toggles: ");
            uart_write_uint32(g_thread_a_toggles);
            uart_write_string("\r\n");

            uart_write_string("Thread B reports: ");
            uart_write_uint32(g_thread_b_reports);
            uart_write_string("\r\n");

            uart_write_string("Context switches: ");
            uart_write_uint32(context_switch_get_count());
            uart_write_string("\r\n");
        }

        // Continue executing until SysTick preempts this thread
        __asm volatile ("nop");
    }
}

// Name: context_demo_start
// Description: Initializes hardware, prepares two private
//              stacks, and starts PendSV-based thread switching.
// Parameters: None
// Returns: Does not return during normal operation.
void context_demo_start(void)
{
    // Initialize only the hardware needed by the demo
    gpio_led_init();
    systick_init();
    uart_init();

    uart_write_string(
        "\r\nJuracicRTOS PendSV demonstration\r\n"
    );

    // Prepare both independent task stacks
    uint8_t thread_a_ready = task_context_init(
        &g_thread_a_context,
        g_thread_a_stack,
        DEMO_STACK_WORDS,
        demo_thread_a,
        0
    );

    uint8_t thread_b_ready = task_context_init(
        &g_thread_b_context,
        g_thread_b_stack,
        DEMO_STACK_WORDS,
        demo_thread_b,
        0
    );

    if ((thread_a_ready == 0U) ||
        (thread_b_ready == 0U))
    {
        uart_write_string(
            "ERROR: Task stack initialization failed\r\n"
        );

        for (;;)
        {
            __asm volatile ("wfi");
        }
    }

    // Check both prepared register frames
    if ((task_context_check_initial_frame(
            &g_thread_a_context) == 0U) ||
        (task_context_check_initial_frame(
            &g_thread_b_context) == 0U))
    {
        uart_write_string(
            "ERROR: Task stack validation failed\r\n"
        );

        for (;;)
        {
            __asm volatile ("wfi");
        }
    }

    // Register both threads for PendSV switching
    context_switch_configure(
        &g_thread_a_context,
        &g_thread_b_context
    );

    // Arm SysTick-driven preemptive context switching
    context_switch_enable_preemption();

    uart_write_string("Two thread stacks initialized\r\n");
    uart_write_string("Starting first thread through SVC\r\n");

    // Begin execution on Thread A's private stack
    context_switch_start();

    // Execution should never reach this line
    for (;;)
    {
        __asm volatile ("wfi");
    }
}
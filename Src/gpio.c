#include <stdint.h>
#include "gpio.h"

// Hardware register addresses 
#define RCC_AHBENR   (*(volatile uint32_t *)0x40021014UL)
#define GPIOE_MODER  (*(volatile uint32_t *)0x48001000UL)
#define GPIOE_BSRR   (*(volatile uint32_t *)0x48001018UL)

// GPIO Port A register addresses
#define GPIOA_MODER  (*(volatile uint32_t *)0x48000000UL)
#define GPIOA_PUPDR  (*(volatile uint32_t *)0x4800000CUL)
#define GPIOA_IDR    (*(volatile uint32_t *)0x48000010UL)

// USER button (B1) configuration
#define BUTTON_PIN    0U
#define GPIOA_CLK_EN  17U

// Blue LED (LD4) configuration 
#define LED_PIN       8U
#define GPIOE_CLK_EN  21U

// Name: gpio_led_init
// Description: Initializes GPIO Port E by enabling its peripheral clock and configuring PE8 as a general-purpose output for the blue LED (LD4). Sets the initial LED state to LOW.
// Parameters: None
// Returns: None
void gpio_led_init(void)
{
    // Enable the clock for GPIO Port E 
    RCC_AHBENR |= (1UL << GPIOE_CLK_EN);

    // Set initial LED output state to LOW 
    GPIOE_BSRR = (1UL << (LED_PIN + 16U));

    // Clear the two mode bits for PE8 */
    GPIOE_MODER &= ~(3UL << (LED_PIN * 2U));

    // Set PE8 to general-purpose output mode (01) 
    GPIOE_MODER |= (1UL << (LED_PIN * 2U));
}

// Name: gpio_led_on
// Description: Turns on the blue LED (LD4) by setting PE8 HIGH using the GPIOE Bit Set/Reset Register.
// Parameters: None
// Returns: None
void gpio_led_on(void)
{
    GPIOE_BSRR = (1UL << LED_PIN);
}

// Name: gpio_led_off
// Description: Turns off the blue LED (LD4) by resetting PE8 LOW using the GPIOE Bit Set/Reset Register.
// Parameters: None
// Returns: None
void gpio_led_off(void)
{
    GPIOE_BSRR = (1UL << (LED_PIN + 16U));
}


// Name: gpio_button_init
// Description: Initializes GPIO Port A by enabling its peripheral clock
//              and configuring PA0 as a digital input with an internal
//              pull-down resistor for the USER button (B1).
// Parameters: None
// Returns: None
void gpio_button_init(void)
{
    // Enable the clock for GPIO Port A
    RCC_AHBENR |= (1UL << GPIOA_CLK_EN);

    // Configure PA0 as input (00)
    GPIOA_MODER &= ~(3UL << (BUTTON_PIN * 2U));

    // Clear PA0's pull-up/pull-down configuration
    GPIOA_PUPDR &= ~(3UL << (BUTTON_PIN * 2U));

    // Enable internal pull-down for PA0 (10)
    GPIOA_PUPDR |= (2UL << (BUTTON_PIN * 2U));
}


// Name: gpio_button_is_pressed
// Description: Reads the USER button (B1) state from GPIOA's
//              input data register using a bitmask.
// Parameters: None
// Returns: 1 if pressed, 0 if released.
uint8_t gpio_button_is_pressed(void)
{
    // Read PA0 and isolate its corresponding bit
    uint32_t button_state = GPIOA_IDR & (1UL << BUTTON_PIN);

    // Return 1 if HIGH, otherwise return 0
    return (button_state != 0U) ? 1U : 0U;
}


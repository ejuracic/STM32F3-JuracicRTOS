#include <stdint.h>
#include "gpio.h"

// Hardware register addresses 
#define RCC_AHBENR   (*(volatile uint32_t *)0x40021014UL)
#define GPIOE_MODER  (*(volatile uint32_t *)0x48001000UL)
#define GPIOE_BSRR   (*(volatile uint32_t *)0x48001018UL)

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

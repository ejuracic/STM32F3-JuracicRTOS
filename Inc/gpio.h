#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>

// Initialize GPIO for the blue LED (LD4) 
void gpio_led_init(void);

// Turn the blue LED on 
void gpio_led_on(void);

// Turn the blue LED off 
void gpio_led_off(void);

// Name: gpio_button_init
// Description: Initializes GPIO Port A and configures PA0 as a
//              digital input for the USER button (B1).
// Parameters: None
// Returns: None
void gpio_button_init(void);

// Name: gpio_button_is_pressed
// Description: Reads the state of the USER button (B1).
// Parameters: None
// Returns: 1 if the button is pressed, 0 otherwise.
uint8_t gpio_button_is_pressed(void);

#endif // GPIO_H 


#include <stdint.h>
#include "spi.h"
#include "systick.h"

// Peripheral clock registers
#define RCC_AHBENR   (*(volatile uint32_t *)0x40021014UL)
#define RCC_APB2ENR  (*(volatile uint32_t *)0x40021018UL)

// GPIOA registers
#define GPIOA_MODER  (*(volatile uint32_t *)0x48000000UL)
#define GPIOA_AFRL   (*(volatile uint32_t *)0x48000020UL)

// SPI1 registers
#define SPI1_CR1     (*(volatile uint32_t *)0x40013000UL)
#define SPI1_CR2     (*(volatile uint32_t *)0x40013004UL)
#define SPI1_SR      (*(volatile uint32_t *)0x40013008UL)
#define SPI1_DR8     (*(volatile uint8_t *)0x4001300CUL)

#define SPI_TIMEOUT_MS 10U

// Name: spi1_init
// Description: Initializes SPI1 as master with 8-bit transfers,
//              mode 3, software NSS, and a clock prescaler of 16.
// Parameters: None
// Returns: None
void spi1_init(void)
{
    // Enable GPIOA and SPI1 peripheral clocks
    RCC_AHBENR |= (1UL << 17);
    RCC_APB2ENR |= (1UL << 12);

    // Configure PA5, PA6, and PA7 as alternate-function pins
    GPIOA_MODER &= ~((3UL << 10) |
                     (3UL << 12) |
                     (3UL << 14));

    GPIOA_MODER |= (2UL << 10) |
                   (2UL << 12) |
                   (2UL << 14);

    // Select alternate function AF5 for SPI1
    GPIOA_AFRL &= ~((15UL << 20) |
                    (15UL << 24) |
                    (15UL << 28));

    GPIOA_AFRL |= (5UL << 20) |
                  (5UL << 24) |
                  (5UL << 28);

    // Disable SPI before configuration
    SPI1_CR1 = 0U;

    // Set 8-bit data size and RX FIFO threshold
    SPI1_CR2 = (7UL << 8) | (1UL << 12);

    // Configure:
    // Master mode
    // Baud-rate prescaler /16
    // CPOL = 1 and CPHA = 1 (SPI mode 3)
    // Software slave management
    SPI1_CR1 = (3UL << 3) |
               (1UL << 2) |
               (1UL << 1) |
               (1UL << 0) |
               (1UL << 8) |
               (1UL << 9);

    // Enable SPI1
    SPI1_CR1 |= (1UL << 6);
}

// Name: spi1_transfer
// Description: Transfers one byte over SPI1 using polling.
//              Uses SysTick for timeout protection.
// Parameters: tx - Byte to transmit.
//             rx - Pointer to the received byte.
// Returns: 1 on success, 0 on timeout.
uint8_t spi1_transfer(uint8_t tx, uint8_t *rx)
{
    uint32_t start = systick_get_milliseconds();

    if (rx == 0)
    {
        return 0U;
    }

    // Wait until transmit buffer is ready
    while ((SPI1_SR & (1UL << 1)) == 0U)
    {
        if ((uint32_t)(systick_get_milliseconds() - start)
            >= SPI_TIMEOUT_MS)
        {
            return 0U;
        }
    }

    // Transmit one 8-bit frame
    SPI1_DR8 = tx;

    start = systick_get_milliseconds();

    // Wait until a byte has been received
    while ((SPI1_SR & (1UL << 0)) == 0U)
    {
        if ((uint32_t)(systick_get_milliseconds() - start)
            >= SPI_TIMEOUT_MS)
        {
            return 0U;
        }
    }

    // Read received byte
    *rx = SPI1_DR8;

    start = systick_get_milliseconds();

    // Wait for the SPI controller to become idle
    while ((SPI1_SR & (1UL << 7)) != 0U)
    {
        if ((uint32_t)(systick_get_milliseconds() - start)
            >= SPI_TIMEOUT_MS)
        {
            return 0U;
        }
    }

    return 1U;
}
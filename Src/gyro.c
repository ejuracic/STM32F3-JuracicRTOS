
#include <stdint.h>
#include "gyro.h"
#include "spi.h"

// GPIOE registers
#define RCC_AHBENR  (*(volatile uint32_t *)0x40021014UL)
#define GPIOE_MODER (*(volatile uint32_t *)0x48001000UL)
#define GPIOE_BSRR  (*(volatile uint32_t *)0x48001018UL)

// Gyroscope register addresses
#define GYRO_WHO_AM_I 0x0FU
#define GYRO_CTRL_REG1 0x20U
#define GYRO_CTRL_REG4 0x23U
#define GYRO_OUT_X_L 0x28U

#define GYRO_CS_PIN 3U

// Name: gyro_select
// Description: Pulls PE3 LOW to select the gyroscope.
// Parameters: None
// Returns: None
static void gyro_select(void)
{
    GPIOE_BSRR = (1UL << (GYRO_CS_PIN + 16U));
}

// Name: gyro_deselect
// Description: Pulls PE3 HIGH to deselect the gyroscope.
// Parameters: None
// Returns: None
static void gyro_deselect(void)
{
    GPIOE_BSRR = (1UL << GYRO_CS_PIN);
}

// Name: gyro_read_register
// Description: Reads one register from the gyroscope over SPI.
// Parameters: address - Register address.
//             value - Pointer to received register value.
// Returns: 1 on success, 0 on communication failure.
static uint8_t gyro_read_register(uint8_t address, uint8_t *value)
{
    uint8_t discarded = 0U;
    uint8_t success;

    gyro_select();

    // Bit 7 selects a read operation
    success = spi1_transfer((uint8_t)(address | 0x80U),
                            &discarded);

    if (success != 0U)
    {
        success = spi1_transfer(0U, value);
    }

    gyro_deselect();

    return success;
}

// Name: gyro_write_register
// Description: Writes one byte to a gyroscope register.
// Parameters: address - Register address.
//             value - Value to write.
// Returns: 1 on success, 0 on communication failure.
static uint8_t gyro_write_register(uint8_t address, uint8_t value)
{
    uint8_t discarded = 0U;
    uint8_t success;

    gyro_select();

    // Bit 7 is cleared for writing
    success = spi1_transfer((uint8_t)(address & 0x3FU),
                            &discarded);

    if (success != 0U)
    {
        success = spi1_transfer(value, &discarded);
    }

    gyro_deselect();

    return success;
}

// Name: gyro_init
// Description: Initializes SPI1 and the onboard gyroscope.
//              Recognizes L3GD20 and I3G4250D variants.
// Parameters: None
// Returns: Detected WHO_AM_I value, or 0 on communication failure.
uint8_t gyro_init(void)
{
    uint8_t device_id = 0U;
    uint8_t discarded = 0U;

    // Enable GPIOE peripheral clock
    RCC_AHBENR |= (1UL << 21);

    // Default chip select to HIGH
    gyro_deselect();

    // Configure PE3 as GPIO output
    GPIOE_MODER &= ~(3UL << (GYRO_CS_PIN * 2U));
    GPIOE_MODER |= (1UL << (GYRO_CS_PIN * 2U));

    // Initialize SPI1 controller
    spi1_init();

    // Perform an initial transfer with CS HIGH
    // to establish the SPI clock idle state
    if (spi1_transfer(0xFFU, &discarded) == 0U)
    {
        return 0U;
    }

    // Read sensor identification register
    if (gyro_read_register(GYRO_WHO_AM_I, &device_id) == 0U)
    {
        return 0U;
    }

    // Recognize supported gyroscope variants
    if ((device_id != 0xD4U) && (device_id != 0xD3U))
    {
        return device_id;
    }

    // Enable block-data update only on the L3GD20
    if (device_id == 0xD4U)
    {
        if (gyro_write_register(GYRO_CTRL_REG4, 0x80U) == 0U)
        {
            return 0U;
        }
    }

    // I3G4250D uses its default CTRL_REG4 configuration
    // The register's bit 7 is reserved on this sensor

    // Enable X, Y, and Z axes in normal operating mode
    if (gyro_write_register(GYRO_CTRL_REG1, 0x0FU) == 0U)
    {
        return 0U;
    }

    return device_id;
}

// Name: gyro_read_axes
// Description: Reads six consecutive gyroscope output registers.
//              Combines low and high bytes into signed 16-bit values.
// Parameters: axes - Pointer to the output structure.
// Returns: 1 on success, 0 on communication failure.
uint8_t gyro_read_axes(GyroAxes *axes)
{
    uint8_t data[6];
    uint8_t discarded = 0U;
    uint8_t success;

    if (axes == 0)
    {
        return 0U;
    }

    gyro_select();

    // Set read bit and auto-increment bit
    success = spi1_transfer(
        (uint8_t)(GYRO_OUT_X_L | 0x80U | 0x40U),
        &discarded
    );

    // Read X_L, X_H, Y_L, Y_H, Z_L, Z_H
    for (uint32_t i = 0U; (i < 6U) && (success != 0U); i++)
    {
        success = spi1_transfer(0U, &data[i]);
    }

    gyro_deselect();

    if (success == 0U)
    {
        return 0U;
    }

    // Combine low and high bytes
    axes->x = (int16_t)((uint16_t)data[0] |
                        ((uint16_t)data[1] << 8));

    axes->y = (int16_t)((uint16_t)data[2] |
                        ((uint16_t)data[3] << 8));

    axes->z = (int16_t)((uint16_t)data[4] |
                        ((uint16_t)data[5] << 8));

    return 1U;
}
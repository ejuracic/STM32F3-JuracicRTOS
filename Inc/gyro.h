#ifndef GYRO_H
#define GYRO_H

#include <stdint.h>

// Raw gyroscope axis measurements
typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} GyroAxes;

// Name: gyro_init
// Description: Initializes the onboard gyroscope and identifies it.
// Parameters: None
// Returns: WHO_AM_I value, or 0 on communication failure.
uint8_t gyro_init(void);

// Name: gyro_read_axes
// Description: Reads the raw X, Y, and Z angular-rate values.
// Parameters: axes - Pointer to the output structure.
// Returns: 1 on success, 0 on communication failure.
uint8_t gyro_read_axes(GyroAxes *axes);

#endif
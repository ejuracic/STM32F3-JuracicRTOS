#ifndef GYRO_PROCESSING_H
#define GYRO_PROCESSING_H

#include <stdint.h>
#include "gyro.h"

#define GYRO_CALIBRATION_SAMPLES 64U

// Gyroscope calibration and filtering state
typedef struct
{
    // Accumulated stationary readings
    int32_t sum_x;
    int32_t sum_y;
    int32_t sum_z;

    // Calculated zero-rate offsets
    int32_t offset_x;
    int32_t offset_y;
    int32_t offset_z;

    // Offset-corrected measurements
    int32_t calibrated_x;
    int32_t calibrated_y;
    int32_t calibrated_z;

    // Low-pass filtered measurements
    int32_t filtered_x;
    int32_t filtered_y;
    int32_t filtered_z;

    // Internal filter accumulators
    int32_t filter_x_q8;
    int32_t filter_y_q8;
    int32_t filter_z_q8;

    uint32_t samples_collected;
    uint8_t calibrated;
    uint8_t filter_initialized;
} GyroProcessor;

// Name: gyro_processor_init
// Description: Resets gyroscope calibration and filtering state.
// Parameters: processor - Pointer to the processing state.
// Returns: None
void gyro_processor_init(GyroProcessor *processor);

// Name: gyro_processor_update
// Description: Collects calibration samples, calculates offsets,
//              and filters subsequent angular-rate measurements.
// Parameters: processor - Pointer to the processing state.
//             raw - Pointer to the latest gyroscope measurement.
// Returns: None
void gyro_processor_update(GyroProcessor *processor, const GyroAxes *raw);

#endif

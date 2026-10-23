#include <stdint.h>
#include "gyro_processing.h"

#define GYRO_FILTER_DIVISOR 4
#define GYRO_FILTER_SCALE 256

// Name: gyro_processor_init
// Description: Clears the gyroscope calibration and filtering data.
// Parameters: processor - Pointer to the processing state.
// Returns: None
void gyro_processor_init(GyroProcessor *processor)
{
    if (processor == 0)
    {
        return;
    }

    *processor = (GyroProcessor){0};
}

// Name: gyro_processor_update
// Description: Calculates stationary offsets from 64 samples,
//              subtracts offsets from subsequent readings,
//              and applies a low-pass filter.
// Parameters: processor - Pointer to the processing state.
//             raw - Pointer to the latest gyroscope measurement.
// Returns: None
void gyro_processor_update(GyroProcessor *processor, const GyroAxes *raw)
{
    if ((processor == 0) || (raw == 0))
    {
        return;
    }

    // Collect stationary calibration samples
    if (processor->calibrated == 0U)
    {
        processor->sum_x += raw->x;
        processor->sum_y += raw->y;
        processor->sum_z += raw->z;

        processor->samples_collected++;

        if (processor->samples_collected < GYRO_CALIBRATION_SAMPLES)
        {
            return;
        }

        // Compute the average stationary offset for each axis
        processor->offset_x =
            processor->sum_x / (int32_t)GYRO_CALIBRATION_SAMPLES;

        processor->offset_y =
            processor->sum_y / (int32_t)GYRO_CALIBRATION_SAMPLES;

        processor->offset_z =
            processor->sum_z / (int32_t)GYRO_CALIBRATION_SAMPLES;

        processor->calibrated = 1U;
    }

    // Remove the stationary offset from each measurement
    processor->calibrated_x = (int32_t)raw->x - processor->offset_x;
    processor->calibrated_y = (int32_t)raw->y - processor->offset_y;
    processor->calibrated_z = (int32_t)raw->z - processor->offset_z;

    // Initialize the filter with the first calibrated sample
    if (processor->filter_initialized == 0U)
    {
        processor->filter_x_q8 = processor->calibrated_x * GYRO_FILTER_SCALE;
        processor->filter_y_q8 = processor->calibrated_y * GYRO_FILTER_SCALE;
        processor->filter_z_q8 = processor->calibrated_z * GYRO_FILTER_SCALE;

        processor->filter_initialized = 1U;
    }
    else
    {
        // Apply an exponential moving-average low-pass filter
        processor->filter_x_q8 +=
            ((processor->calibrated_x * GYRO_FILTER_SCALE)
             - processor->filter_x_q8) / GYRO_FILTER_DIVISOR;

        processor->filter_y_q8 +=
            ((processor->calibrated_y * GYRO_FILTER_SCALE)
             - processor->filter_y_q8) / GYRO_FILTER_DIVISOR;

        processor->filter_z_q8 +=
            ((processor->calibrated_z * GYRO_FILTER_SCALE)
             - processor->filter_z_q8) / GYRO_FILTER_DIVISOR;
    }

    // Convert the filtered fixed-point values back to raw counts
    processor->filtered_x = processor->filter_x_q8 / GYRO_FILTER_SCALE;
    processor->filtered_y = processor->filter_y_q8 / GYRO_FILTER_SCALE;
    processor->filtered_z = processor->filter_z_q8 / GYRO_FILTER_SCALE;
}

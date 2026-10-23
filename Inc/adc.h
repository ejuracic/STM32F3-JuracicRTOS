
#ifndef ADC_H
#define ADC_H

#include <stdint.h>

// Name: adc_temperature_init
// Description: Initializes ADC1, the internal temperature sensor,
//              and the internal voltage reference.
// Parameters: None
// Returns: None
void adc_temperature_init(void);

// Name: adc_temperature_read_raw
// Description: Reads the internal temperature sensor using ADC1.
// Parameters: None
// Returns: Raw 12-bit temperature ADC reading.
uint16_t adc_temperature_read_raw(void);

// Name: adc_vrefint_read_raw
// Description: Reads the internal voltage reference using ADC1.
// Parameters: None
// Returns: Raw 12-bit VREFINT ADC reading.
uint16_t adc_vrefint_read_raw(void);

// Name: adc_get_vdda_mv
// Description: Estimates VDDA using the factory VREFINT calibration.
// Parameters: vref_raw - Raw VREFINT ADC reading.
// Returns: Estimated VDDA in millivolts, or 0 if invalid.
uint32_t adc_get_vdda_mv(uint16_t vref_raw);

// Name: adc_temperature_celsius
// Description: Calculates MCU temperature using factory calibration
//              and VREFINT compensation.
// Parameters: temp_raw - Raw temperature ADC reading.
//             vref_raw - Raw VREFINT ADC reading.
// Returns: Estimated MCU temperature in degrees Celsius.
//          Returns 0 if calibration or VREFINT data is invalid.
int32_t adc_temperature_celsius(uint16_t temp_raw, uint16_t vref_raw);

#endif

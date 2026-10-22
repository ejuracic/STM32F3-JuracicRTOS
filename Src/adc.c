
#include <stdint.h>
#include "adc.h"
#include "systick.h"

// RCC clock register
#define RCC_AHBENR   (*(volatile uint32_t *)0x40021014UL)

// ADC1 registers
#define ADC1_ISR     (*(volatile uint32_t *)0x50000000UL)
#define ADC1_CR      (*(volatile uint32_t *)0x50000008UL)
#define ADC1_CFGR    (*(volatile uint32_t *)0x5000000CUL)
#define ADC1_SMPR2   (*(volatile uint32_t *)0x50000018UL)
#define ADC1_SQR1    (*(volatile uint32_t *)0x50000030UL)
#define ADC1_DR      (*(volatile uint32_t *)0x50000040UL)

// ADC1/ADC2 common control register
#define ADC12_CCR    (*(volatile uint32_t *)0x50000308UL)

// Factory calibration values
#define TS_CAL1      (*(const uint16_t *)0x1FFFF7B8UL)
#define TS_CAL2      (*(const uint16_t *)0x1FFFF7C2UL)
#define VREFINT_CAL  (*(const uint16_t *)0x1FFFF7BAUL)

// ADC configuration
#define ADC12_CLK_EN 28U
#define TEMP_CHANNEL 16U
#define VREF_CHANNEL 18U
#define CALIBRATION_VDDA_MV 3300U

// Name: adc_wait_ms
// Description: Waits for ADC hardware startup using SysTick.
// Parameters: delay_ms - Number of milliseconds to wait.
// Returns: None
static void adc_wait_ms(uint32_t delay_ms)
{
    uint32_t start = systick_get_milliseconds();

    while ((uint32_t)(systick_get_milliseconds() - start) < delay_ms)
    {
    }
}

// Name: adc_temperature_init
// Description: Initializes ADC1, the temperature sensor,
//              and VREFINT with factory ADC calibration.
// Parameters: None
// Returns: None
void adc_temperature_init(void)
{
    // Enable ADC1 and ADC2 peripheral clock
    RCC_AHBENR |= (1UL << ADC12_CLK_EN);
    (void)RCC_AHBENR;

    // Select synchronous ADC clock at HCLK / 2
    ADC12_CCR &= ~(3UL << 16);
    ADC12_CCR |= (2UL << 16);

    // Enable temperature sensor and internal voltage reference
    // Both must be enabled before ADC1 is enabled
    ADC12_CCR |= (1UL << 23) | (1UL << 22);

    // Reset ADC voltage regulator configuration
    ADC1_CR &= ~(3UL << 28);

    // Enable ADC voltage regulator
    ADC1_CR |= (1UL << 28);

    // Allow regulator to stabilize
    adc_wait_ms(1U);

    // Start single-ended ADC calibration
    ADC1_CR |= (1UL << 31);

    // Wait for calibration to complete
    while ((ADC1_CR & (1UL << 31)) != 0U)
    {
    }

    // Allow time after calibration
    adc_wait_ms(1U);

    // Set 12-bit resolution and software triggering
    ADC1_CFGR = 0U;

    // Set long sampling times for channels 16 and 18
    // Channel 16: Temperature sensor
    // Channel 18: VREFINT
    ADC1_SMPR2 = (7UL << 18) | (7UL << 24);

    // Initially select the temperature sensor
    ADC1_SQR1 = (TEMP_CHANNEL << 6);

    // Clear ADC ready flag
    ADC1_ISR = (1UL << 0);

    // Enable ADC1
    ADC1_CR |= (1UL << 0);

    // Wait until ADC1 is ready
    while ((ADC1_ISR & (1UL << 0)) == 0U)
    {
    }

    // Allow internal sensors to stabilize
    adc_wait_ms(1U);
}

// Name: adc_read_channel
// Description: Selects an ADC1 regular channel and performs
//              one software-triggered conversion.
// Parameters: channel - ADC channel number.
// Returns: Raw 12-bit ADC conversion result.
static uint16_t adc_read_channel(uint32_t channel)
{
    // Select one channel in the regular conversion sequence
    // The sequence contains exactly one conversion
    ADC1_SQR1 = (channel << 6);

    // Clear previous conversion flags
    ADC1_ISR = (1UL << 2) | (1UL << 3);

    // Start ADC regular conversion
    ADC1_CR |= (1UL << 2);

    // Wait until conversion is complete
    while ((ADC1_ISR & (1UL << 2)) == 0U)
    {
    }

    // Read and return conversion result
    return (uint16_t)ADC1_DR;
}

// Name: adc_temperature_read_raw
// Description: Reads ADC1 channel 16 (temperature sensor).
// Parameters: None
// Returns: Raw 12-bit temperature sensor value.
uint16_t adc_temperature_read_raw(void)
{
    return adc_read_channel(TEMP_CHANNEL);
}

// Name: adc_vrefint_read_raw
// Description: Reads ADC1 channel 18 (internal reference).
// Parameters: None
// Returns: Raw 12-bit internal voltage reference value.
uint16_t adc_vrefint_read_raw(void)
{
    return adc_read_channel(VREF_CHANNEL);
}

// Name: adc_get_vdda_mv
// Description: Estimates the analog supply voltage using
//              the factory VREFINT calibration value.
// Parameters: vref_raw - Raw VREFINT ADC reading.
// Returns: Estimated VDDA in millivolts, or 0 if invalid.
uint32_t adc_get_vdda_mv(uint16_t vref_raw)
{
    if (vref_raw == 0U)
    {
        return 0U;
    }

    return (CALIBRATION_VDDA_MV * (uint32_t)VREFINT_CAL)
           / (uint32_t)vref_raw;
}

// Name: adc_temperature_celsius
// Description: Calculates an estimated MCU temperature using
//              VREFINT compensation and factory calibration.
// Parameters: temp_raw - Temperature sensor ADC reading.
//             vref_raw - Internal voltage reference ADC reading.
// Returns: Estimated temperature in degrees Celsius.
//          Returns 0 if the calibration data is invalid.
int32_t adc_temperature_celsius(uint16_t temp_raw, uint16_t vref_raw)
{
    int32_t cal1 = (int32_t)TS_CAL1;
    int32_t cal2 = (int32_t)TS_CAL2;

    if ((vref_raw == 0U) || (cal2 == cal1))
    {
        return 0;
    }

    // Normalize the temperature ADC result to 3.3 V
    uint32_t adjusted_raw =
        ((uint32_t)temp_raw * (uint32_t)VREFINT_CAL)
        / (uint32_t)vref_raw;

    // Interpolate using factory temperature calibration
    int32_t temperature =
        30 + (((int32_t)adjusted_raw - cal1) * 80)
        / (cal2 - cal1);

    return temperature;
}
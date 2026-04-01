// ==============================================================================
// === BME280 I2C LIBRARY =======================================================
// ==============================================================================

// Double inclusion prevention
#ifndef BME280_I2C_H_
#define BME280_I2C_H_

// Including required libraries
#include "stm32f4xx_hal.h"
#include "bme280.h"

// --- C++ Compatibility ---
// Allows this C code to be safely used in C++ projects (prevents name mangling)
#ifdef __cplusplus
extern "C" {
#endif

extern I2C_HandleTypeDef hi2c1; // Link to the hardware I2C1 configuration.

// Structure for ready results (The BME280 library has a similar structure, but it has too many parameters)
typedef struct {
    float temperature; // st.C
    float humidity;    // %
    float pressure;    // hPa
} BME280_Results;


// Function prototypes
int8_t BME280_Hardware_Init(void); /* Initialization function, if it returns 0, the initialization was successful,
									another number is the error code */
// Reads sensor data and saves it to the provided structure of type BME280_Results
int8_t BME280_Read_Measurements(BME280_Results *results);


// --- C++ Compatibility (Closing) ---
#ifdef __cplusplus
}
#endif

#endif /* BME280_I2C_H_ */

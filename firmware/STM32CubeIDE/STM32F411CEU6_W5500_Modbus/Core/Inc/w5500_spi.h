// ==============================================================================
// === W5500 SPI LIBRARY ========================================================
// ==============================================================================

// Double inclusion prevention
#ifndef W5500_SPI_H_
#define W5500_SPI_H_

#include "stm32f4xx_hal.h" // Downloads port definitions and HAL library

// --- C++ Compatibility ---
// Allows this C code to be safely used in C++ projects (prevents name mangling)
#ifdef __cplusplus
extern "C" {
#endif

extern SPI_HandleTypeDef hspi1; // Link to the hardware SPI1 configuration.

// Function prototype
void W5500_Hardware_Init(void);
void W5500_Process_Interrupt(void);


// --- C++ Compatibility (Closing) ---
#ifdef __cplusplus
}
#endif

#endif /* W5500_SPI_H_ */

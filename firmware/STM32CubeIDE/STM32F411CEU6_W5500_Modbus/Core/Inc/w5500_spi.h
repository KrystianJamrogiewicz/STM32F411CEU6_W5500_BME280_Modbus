// ==============================================================================
// === W5500 SPI LIBRARY ========================================================
// ==============================================================================

#ifndef W5500_SPI_H_
#define W5500_SPI_H_

// --- C++ Compatibility ---
// Allows this C code to be safely used in C++ projects (prevents name mangling)
#ifdef __cplusplus
extern "C" {
#endif

#include "main.h" // Downloads port definitions and HAL library


void W5500_Hardware_Init(void);


// --- C++ Compatibility (Closing) ---
#ifdef __cplusplus
}
#endif

#endif /* W5500_SPI_H_ */

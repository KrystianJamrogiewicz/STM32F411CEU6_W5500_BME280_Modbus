// ==============================================================================
// === MODBUS TCP SERVER LIBRARY (Header File) ==================================
// ==============================================================================

#ifndef INC_MODBUS_TCP_H_
#define INC_MODBUS_TCP_H_

// --- C++ Compatibility ---
// Allows this C code to be safely used in C++ projects (prevents name mangling)
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h> // Required for fixed-width integer types (uint16_t, uint8_t)


// ==============================================================================
// === CONSTANTS & MACROS =======================================================
// ==============================================================================

// Defines the total number of 16-bit Holding Registers available in Modbus TCP
#define MB_REG_COUNT 10


// ==============================================================================
// === GLOBAL VARIABLES =========================================================
// ==============================================================================

// To make a variable available to other files, use 'extern' and include the .h file in them
extern uint16_t MB_HoldingRegisters[MB_REG_COUNT];


// ==============================================================================
// === PUBLIC FUNCTION PROTOTYPES ===============================================
// ==============================================================================

// Function prototypes do not require 'extern' to be shared with other files. Simply including the .h file is enough
void Modbus_Init(void);
void Modbus_Loop(void);


#ifdef __cplusplus
}
#endif

#endif /* INC_MODBUS_TCP_H_ */

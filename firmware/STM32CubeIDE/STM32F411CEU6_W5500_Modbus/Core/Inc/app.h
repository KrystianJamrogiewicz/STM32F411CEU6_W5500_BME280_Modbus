// ==============================================================================
// === APP LIBRARY ==============================================================
// ==============================================================================

// Double inclusion prevention
#ifndef INC_APP_H_
#define INC_APP_H_

#include "main.h"

// --- C++ Compatibility ---
// Allows this C code to be safely used in C++ projects (prevents name mangling)
#ifdef __cplusplus
extern "C" {
#endif

extern UART_HandleTypeDef huart1; // Link to the hardware UART1 configuration.

extern char msg[128]; 	  // A array that stores the notification/message to be displayed.
extern char err_msg[100]; // A array that stores the error to be displayed.

extern uint8_t sensors_error_mask;

// ==========================================
// FINITE STATE MACHINE (FSM) SETUP
// ==========================================
// 1. Define all possible states of our system
typedef enum {
    STATE_INIT,             // Setting up variables and initial timers
    STATE_IDLE,             // Waiting for network events or sensor read timeouts
    STATE_NETWORK_PROCESS,  // Processing incoming Modbus TCP requests
    STATE_READ_SENSOR,      // Fetching data from the BME280 sensor
	STATE_SENSOR_ERROR,
	STATE_NETWORK_ERROR     // Critical failure state (e.g., sensor disconnected)
} SystemState_t;

// 2. Variable holding the current system state.
extern SystemState_t current_state;

// Application I/O initial state setup
void App_Init(void);

// Application main loop
void App_Loop(void);


// --- C++ Compatibility (Closing) ---
#ifdef __cplusplus
}
#endif


#endif

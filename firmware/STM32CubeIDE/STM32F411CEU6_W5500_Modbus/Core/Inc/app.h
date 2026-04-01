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


// Application I/O initial state setup
void App_Init(void);

// Application main loop
void App_Loop(void);


// --- C++ Compatibility (Closing) ---
#ifdef __cplusplus
}
#endif


#endif

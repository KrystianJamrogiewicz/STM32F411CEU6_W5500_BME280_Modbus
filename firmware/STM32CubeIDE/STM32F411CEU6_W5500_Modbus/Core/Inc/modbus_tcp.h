#ifndef INC_MODBUS_TCP_H_
#define INC_MODBUS_TCP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define MB_REG_COUNT 10

extern uint16_t MB_HoldingRegisters[MB_REG_COUNT]; // Udostępniamy tablicę dla innych plików

void Modbus_Init(void);
void Modbus_Loop(void);

#ifdef __cplusplus
}
#endif


#endif

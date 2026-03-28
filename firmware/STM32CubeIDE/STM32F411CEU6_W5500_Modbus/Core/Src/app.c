#include "main.h"
#include "app.h"
#include "modbus_tcp.h"
#include "w5500_spi.h"
#include <stdio.h>  // sprintf
#include <string.h> // strlen


extern UART_HandleTypeDef huart1; // Declaring global variables from other files

// Time tracking variable (used for non-blocking delay)
uint32_t last_tick = 0;


void App_Init(void)
{
	// Initialize the W5500 module
	W5500_Hardware_Init();

	// Initialize the MODBUS TCP server
	Modbus_Init();

	// Set initial values for the MODBUS TCP registers
	MB_HoldingRegisters[0] = 100;
	MB_HoldingRegisters[1] = 200;
	MB_HoldingRegisters[2] = 300;
	MB_HoldingRegisters[3] = 400;


	// Startup message on UART
    char msg[64]; // Create a buffer for the text message (max 64 characters)
    sprintf(msg, "System START! Modbus TCP Server is running.\r\n"); // Write the text into the buffer
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100); // Send the exact length of the message via UART (timeout: 100 ms)

}

void App_Loop(void)
{
	// Handle MODBUS TCP communication (must run continuously)
	Modbus_Loop();


	if (HAL_GetTick() - last_tick >= 1000) {

		last_tick = HAL_GetTick();

		// Value modification (Simulating sensor data)
		MB_HoldingRegisters[0]++;

		if (MB_HoldingRegisters[0] > 150) {

		    MB_HoldingRegisters[0] = 100;
		 }


		// Display on UART
		char msg[128];
		sprintf(msg, "MB_HoldingRegisters [0]:%d  [1]:%d  [2]:%d  [3]:%d \r\n",
				MB_HoldingRegisters[0],
				MB_HoldingRegisters[1],
				MB_HoldingRegisters[2],
				MB_HoldingRegisters[3]);

		HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);

		// Blink LED
		// HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
	}


}

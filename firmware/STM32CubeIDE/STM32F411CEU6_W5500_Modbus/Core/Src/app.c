#include "main.h"
#include "app.h"
#include "modbus_tcp.h"
#include "w5500_spi.h"
#include "wizchip_conf.h"
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



	// ==============================================================================
	// === W5500 UART comunication mesage ===========================================
	// ==============================================================================

	// Declare an empty structure (based on WIZnet blueprint "wizchip_conf.h") to hold the read network data
	wiz_NetInfo readInfo;

	/*
	 * typedef struct wiz_NetInfo_t
		{
		   uint8_t mac[6];  ///< Source Mac Address
		   uint8_t ip[4];   ///< Source IP Address
		   uint8_t sn[4];   ///< Subnet Mask
		   uint8_t gw[4];   ///< Gateway IP Address
		   uint8_t dns[4];  ///< DNS server IP Address
		   dhcp_mode dhcp;  ///< 1 - Static, 2 - DHCP
		} wiz_NetInfo;
	 */


	// Reading network data from the W5500 module
	wizchip_getnetinfo(&readInfo);

	// Startup message on UART
    char msg[128]; // Create a buffer for the text message (max 128 characters)

    // Write the text into the buffer (msg)
	sprintf(msg, "\r\n--- SYSTEM START ---\r\n"
	             "W5500 SPI Bridge OK!\r\n"
	             "Adres IP: %d.%d.%d.%d\r\n"
	             "Port Modbus TCP: %d\r\n"
	             "--------------------\r\n",
	             readInfo.ip[0], readInfo.ip[1], readInfo.ip[2], readInfo.ip[3],
	             MB_TCP_PORT);

	// Send the exact length of the message via UART (timeout: 100 ms)
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);

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

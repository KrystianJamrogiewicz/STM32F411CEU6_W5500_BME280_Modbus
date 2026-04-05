#include "main.h"
#include "app.h"
#include "modbus_tcp.h"
#include "w5500_spi.h"
#include "wizchip_conf.h"
#include "bme280_i2c.h"
#include <stdio.h>  // sprintf
#include <string.h> // strlen

// ==========================================
// SENSOR DATA & FLAGS
// ==========================================
static BME280_Results my_weather; // Structure storing measurements from the BME280 sensor

// 'volatile' tells the compiler that this variable can change unexpectedly in the background (via Hardware Interrupt)
volatile uint8_t W5500_Event_Flag = 1; // Set to 1 initially to force the first Modbus loop run


// Time tracking variables for non-blocking delays
static uint32_t last_bme_tick = 0;
static uint32_t last_network_tick = 0;
static uint32_t last_error_tick = 0;
static uint32_t last_sensor_error_tick = 0;

// Error mask: 0 = OK, Bit 0 = BME280, Bit 1 = New sensor...
// By default, the sensors are assumed to be working.
uint8_t sensors_error_mask = 0;

char msg[128]; 	   // A array that stores the notification/message to be displayed.
char err_msg[100]; // A array that stores the error information to be displayed.

// Variable holding the current system state (starts at INIT).
SystemState_t current_state = STATE_INIT;



// ==========================================
// SYSTEM INITIALIZATION
// ==========================================
void App_Init(void)
{

    // ==============================================================================
    // === Initialize Communication W5500 and MODBUS TCP/IP =========================
    // ==============================================================================

    // Initialize the W5500 module
    W5500_Hardware_Init();

    // Initialize the MODBUS TCP server
    Modbus_Init();

    // === W5500 UART Communication Message =========================================
    wiz_NetInfo readInfo;
    wizchip_getnetinfo(&readInfo); // Read network data from the W5500 module

    // SUCCES - Connected to W5500 module via SPI.
    if (readInfo.ip[0] != 0) {
		sprintf(msg, "\r\n--- SYSTEM START ---\r\n"
					 "W5500 SPI Bridge OK!\r\n"
					 "IP Address: %d.%d.%d.%d\r\n"
					 "Modbus TCP Port: %d\r\n"
					 "--------------------\r\n",
					 readInfo.ip[0], readInfo.ip[1], readInfo.ip[2], readInfo.ip[3],
					 MB_TCP_PORT);

		HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
     }

    // ERROR - The IP address of the W5500 module was not read.
    else {
    	sprintf(err_msg, "\r\n[DIAG] Error: W5500 Module failure during initialization - device reset required\r\n");
        current_state = STATE_NETWORK_ERROR;
        // RETURN: Exits the ENTIRE function immediately. Any code below this line will NOT execute!
        return;
    }

    // ==============================================================================
    // === Initialize BME280  =======================================================
    // ==============================================================================

    // Call the hardware init function and store the exact status code.
    int8_t bme_init_status = BME280_Hardware_Init();

    // === BME280 UART Communication Message ========================================
    sprintf(msg, "\r\n--- BME280 Init ---\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);

    // Check the stored status explicitly (Clean Code approach).
    // SUCCES - Connected to BME280 module via I2C.
    if (bme_init_status == 0) { // 0 represents BME280_OK
        sprintf(msg, "BME280: FOUND & INITIALIZED!\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
    }

    // ERROR
    else {
        // Set error flag for BME280 sensor.
    	sprintf(err_msg, "\r\n[DIAG] Error: BME280 sensor failure during initialization - device reset required\r\n");
        sensors_error_mask |= (1 << 0); // Set bit number 0 to value 1.
    }
}



// ==========================================
// MAIN APPLICATION LOOP (FSM)
// ==========================================
void App_Loop(void)
{
    // FINITE STATE MACHINE (FSM).
    switch (current_state) {

        // ----------------------------------------------------------------------
        case STATE_INIT:
            // Establish the starting point for our non-blocking timers.
            last_bme_tick = HAL_GetTick();
            last_network_tick = HAL_GetTick();

            // Initialization complete, move to the waiting state.
            current_state = STATE_IDLE;
            // BREAK: Exits ONLY the current loop (for/while) or switch statement.
            // The rest of the function keeps running.
            break;


        // ----------------------------------------------------------------------
        case STATE_IDLE:
            // MAIN WAITING ROOM: The MCU waits here and checks conditions.

            // Condition 1: Did the W5500 hardware interrupt trigger, or did 200ms pass?
            if ((W5500_Event_Flag == 1) || (HAL_GetTick() - last_network_tick >= 200)) {
                current_state = STATE_NETWORK_PROCESS;
            }
            // Condition 2: Did 1000ms (1 second) pass since the last sensor read?
            else if (HAL_GetTick() - last_bme_tick >= 1000) {
                current_state = STATE_READ_SENSOR;
            }
            // 3. Condition 2: If any sensor error occurs, go to diagnostics state (STATE_SENSOR_ERROR)
            else if (sensors_error_mask != 0) {
            	current_state = STATE_SENSOR_ERROR;
            }
            break;


        // ----------------------------------------------------------------------
        case STATE_NETWORK_PROCESS:
            // 1. Reset the event flags and timers.
            W5500_Event_Flag = 0;
            last_network_tick = HAL_GetTick();

            // 2. Process the Modbus TCP request.
            Modbus_Loop();

            // 3. Return to waiting room.
            current_state = STATE_IDLE;
            break;


        // ----------------------------------------------------------------------
        case STATE_READ_SENSOR:
            // 1. Reset the sensor timer.
            last_bme_tick = HAL_GetTick();

            // 2. Fetch the data and store the status.
            int8_t sensor_status = BME280_Read_Measurements(&my_weather);

            // 3. Evaluate the status
            if (sensor_status == 0) { // SUCCES!
            	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET); // Error LED OFF

            	// Setting the flag, the sensor is working.
            	// CLEAR BIT number 0 IN THE MASK! (The tilde '~' reverses the mask, '&=' clears only that one bit).
            	sensors_error_mask &= ~(1 << 0);

                // Convert floats to integers for Modbus (e.g., 23.54 C becomes 2354)
                MB_HoldingRegisters[0] = (uint16_t)(my_weather.temperature * 100);
                MB_HoldingRegisters[1] = (uint16_t)(my_weather.humidity * 100);
                MB_HoldingRegisters[2] = (uint16_t)(my_weather.pressure); // hPa

                // Optional: Print live data to UART.
                sprintf(msg, "Temp: %d C | Hum: %d %% | Press: %d hPa\r\n",
                		(int)my_weather.temperature, (int)my_weather.humidity, (int)my_weather.pressure);
                HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
            }

            else { // READ ERROR!
        		sprintf(err_msg, "\r\n[DIAG] Error: BME280 Sensor Failure during operation\r\n");
            	sensors_error_mask |= (1 << 0);  // Set Bit number 0 to value 1 (true)  (BME280 ERROR)
            }
            current_state = STATE_IDLE; // Go back to IDLE STATE
            break;


        // ----------------------------------------------------------------------
        case STATE_SENSOR_ERROR:
        // Sending a message about a damaged sensor.

            if (HAL_GetTick() - last_sensor_error_tick >= 1000) {
            	last_sensor_error_tick = HAL_GetTick();

            	// Error LED blinking.
            	HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);

            	// Displaying error information on UART.
            	if (sensors_error_mask & (1 << 0)) {
            		HAL_UART_Transmit(&huart1, (uint8_t*)err_msg, strlen(err_msg), 100);
            	}

            	/*// For future sensors
            	if (sensors_error_mask & (1 << 1)) {
            		Modbus_Log("\r\n[DIAG] Error: Future Sensor Failure\r\n");
            	}
            	*/
            }
            current_state = STATE_IDLE;
            break;


        // ----------------------------------------------------------------------
        case STATE_NETWORK_ERROR:
            // CRITICAL FAILURE STATE
            // The system gets trapped here if the W5500 module fails. Modbus and sensor reading will halt.

        	if (HAL_GetTick() - last_error_tick >= 1000) {
        		last_error_tick = HAL_GetTick();
        		HAL_UART_Transmit(&huart1, (uint8_t*)err_msg, strlen(err_msg), 100);
        		HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        	}
        	break;
    }
}



// ==============================================================================
// === HARDWARE INTERRUPT CALLBACK ===
// ==============================================================================
/**
 * @brief This function is automatically triggered by STM32 hardware whenever a
 * state change (e.g., Falling Edge) is detected on the configured EXTI pins.
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	// Checking if the interruption concerns the W5500 module.
    if (GPIO_Pin == W5500_INT_Pin) {
        W5500_Process_Interrupt(); // Function call from W5500_spi.c.
    }
}

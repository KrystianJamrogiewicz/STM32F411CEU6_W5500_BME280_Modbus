#include "main.h"
#include "app.h"
#include "modbus_tcp.h"
#include "w5500_spi.h"
#include "wizchip_conf.h"
#include "bme280_i2c.h"
#include <stdio.h>  // sprintf
#include <string.h> // strlen

extern UART_HandleTypeDef huart1; // Link to the hardware UART1 configuration.

// ==========================================
// SENSOR DATA & FLAGS
// ==========================================
BME280_Results my_weather; // Structure storing measurements from the BME280 sensor

// 'volatile' tells the compiler that this variable can change unexpectedly in the background (via Hardware Interrupt)
volatile uint8_t W5500_Event_Flag = 1; // Set to 1 initially to force the first Modbus loop run

// Time tracking variables for non-blocking delays
uint32_t last_bme_tick = 0;
uint32_t last_network_tick = 0;


// ==========================================
// FINITE STATE MACHINE (FSM) SETUP
// ==========================================
// 1. Define all possible states of our system
typedef enum {
    STATE_INIT,             // Setting up variables and initial timers
    STATE_IDLE,             // Waiting for network events or sensor read timeouts
    STATE_NETWORK_PROCESS,  // Processing incoming Modbus TCP requests
    STATE_READ_SENSOR,      // Fetching data from the BME280 sensor
    STATE_ERROR             // Critical failure state (e.g., sensor disconnected)
} SystemState_t;

// 2. Variable holding the current system state (starts at INIT)
SystemState_t current_state = STATE_INIT;



// ==========================================
// SYSTEM INITIALIZATION
// ==========================================
void App_Init(void)
{
    // 1. Initialize the W5500 module
    W5500_Hardware_Init();

    // 2. Initialize the MODBUS TCP server
    Modbus_Init();

    // Set initial values for the MODBUS TCP registers
    MB_HoldingRegisters[0] = 100;
    MB_HoldingRegisters[1] = 200;
    MB_HoldingRegisters[2] = 300;
    MB_HoldingRegisters[3] = 400;

    // ==============================================================================
    // === W5500 UART Communication Message =========================================
    // ==============================================================================
    wiz_NetInfo readInfo;
    wizchip_getnetinfo(&readInfo); // Read network data from the W5500 module

    char msg[128];
    sprintf(msg, "\r\n--- SYSTEM START ---\r\n"
                 "W5500 SPI Bridge OK!\r\n"
                 "IP Address: %d.%d.%d.%d\r\n"
                 "Modbus TCP Port: %d\r\n"
                 "--------------------\r\n",
                 readInfo.ip[0], readInfo.ip[1], readInfo.ip[2], readInfo.ip[3],
                 MB_TCP_PORT);

    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);

    // ==============================================================================
    // === BME280 UART Communication Message ========================================
    // ==============================================================================
    char uart_buf[100];
    sprintf(uart_buf, "\r\n--- BME280 Init ---\r\n");
    HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), 100);

    // Call the hardware init function and store the exact status code
    int8_t bme_init_status = BME280_Hardware_Init();

    // Check the stored status explicitly (Clean Code approach)
    if (bme_init_status == 0) { // 0 represents BME280_OK
        sprintf(uart_buf, "BME280: FOUND & INITIALIZED!\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), 100);
    }
    else {
        // Print the specific error code to UART for easy debugging
        sprintf(uart_buf, "BME280: ERROR! Code: %d (Check I2C wiring!)\r\n", bme_init_status);
        HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), 100);

        // Critical hardware failure! Force the FSM into the ERROR state.
        current_state = STATE_ERROR;
    }
}


// ==========================================
// MAIN APPLICATION LOOP (FSM)
// ==========================================
void App_Loop(void)
{
    // FINITE STATE MACHINE (FSM)
    switch (current_state) {

        // ----------------------------------------------------------------------
        case STATE_INIT:
            // Establish the starting point for our non-blocking timers
            last_bme_tick = HAL_GetTick();
            last_network_tick = HAL_GetTick();

            // Initialization complete, move to the waiting state
            current_state = STATE_IDLE;
            break;


        // ----------------------------------------------------------------------
        case STATE_IDLE:
            // MAIN WAITING ROOM: The MCU waits here and checks conditions

            // Condition 1: Did the W5500 hardware interrupt trigger, or did 200ms pass?
            if ((W5500_Event_Flag == 1) || (HAL_GetTick() - last_network_tick >= 200)) {
                current_state = STATE_NETWORK_PROCESS;
            }
            // Condition 2: Did 1000ms (1 second) pass since the last sensor read?
            else if (HAL_GetTick() - last_bme_tick >= 1000) {
                current_state = STATE_READ_SENSOR;
            }
            break;


        // ----------------------------------------------------------------------
        case STATE_NETWORK_PROCESS:
            // 1. Reset the event flags and timers
            W5500_Event_Flag = 0;
            last_network_tick = HAL_GetTick();

            // 2. Process the Modbus TCP request
            Modbus_Loop();

            // 3. Return to waiting room
            current_state = STATE_IDLE;
            break;


        // ----------------------------------------------------------------------
        case STATE_READ_SENSOR:
            // 1. Reset the sensor timer
            last_bme_tick = HAL_GetTick();

            // 2. Fetch the data and store the status explicitly (Clean Code)
            int8_t sensor_status = BME280_Read_Measurements(&my_weather);

            // 3. Evaluate the status
            if (sensor_status == 0) { // Success!

                // Convert floats to integers for Modbus (e.g., 23.54 C becomes 2354)
                MB_HoldingRegisters[0] = (uint16_t)(my_weather.temperature * 100);
                MB_HoldingRegisters[1] = (uint16_t)(my_weather.humidity * 100);
                MB_HoldingRegisters[2] = (uint16_t)(my_weather.pressure); // hPa

                // Optional: Print live data to UART (for monitoring)
                int temp_int = (int)my_weather.temperature;
                int temp_frac = (int)((my_weather.temperature - temp_int) * 100);
                if(temp_frac < 0) temp_frac = -temp_frac;

                char msg[100];
                sprintf(msg, "Live -> Temp: %d.%02d C | Hum: %d %% | Press: %d hPa\r\n",
                        temp_int, temp_frac, (int)my_weather.humidity, (int)my_weather.pressure);
                HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);

                current_state = STATE_IDLE; // Successfully read, go back to waiting
            }
            else { // Read Error!

                char err_msg[100];
                sprintf(err_msg, "\r\n[!] SENSOR READ ERROR! Code: %d\r\n", sensor_status);
                HAL_UART_Transmit(&huart1, (uint8_t*)err_msg, strlen(err_msg), 100);

                current_state = STATE_ERROR; // Sensor disconnected during operation! Go to Error state.
            }
            break;


        // ----------------------------------------------------------------------
        case STATE_ERROR:
            // CRITICAL FAILURE STATE
            // The system gets trapped here if the sensor fails. Modbus and sensor reading will halt.
            // In a real industrial application, you might blink a red LED, trigger an alarm relay,
            // or attempt a software reset via NVIC_SystemReset().

            /* Example: Blink an error LED every 500ms
            if (HAL_GetTick() - last_error_tick >= 500) {
                last_error_tick = HAL_GetTick();
                HAL_GPIO_TogglePin(LED_ERROR_GPIO_Port, LED_ERROR_Pin);
            }
            */
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
    // Assuming your pin is named W5500_INT_Pin in CubeMX
    if (GPIO_Pin == W5500_INT_Pin) {

        // Signal the FSM that Modbus data is waiting!
        W5500_Event_Flag = 1;

        // Read and clear the master interrupt register (IR) on the W5500
        uint8_t ir = getIR();
        setIR(ir);
    }
}

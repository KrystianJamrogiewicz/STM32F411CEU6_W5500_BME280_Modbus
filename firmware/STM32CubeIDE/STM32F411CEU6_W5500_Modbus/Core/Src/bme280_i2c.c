// ==============================================================================
// === BME280 I2C DRIVER ========================================================
// ==============================================================================

#include "bme280_i2c.h"


// -----------------------------------------------------------------------------
// PRIVATE MODULE VARIABLES
// -----------------------------------------------------------------------------
static struct bme280_dev bme280_device; // Sensor context storing interface pointers and calibration data.
static uint8_t bme280_dev_addr;         // Stores the shifted I2C address.
static I2C_HandleTypeDef *bme280_hi2c;  /* Pointer to the hardware I2C handle.
Saved during initialization to be used globally by the I2C read/write callbacks. */


// -----------------------------------------------------------------------------
// BME280 LIBRARY WRAPPER FUNCTIONS (HAL <-> BME280 Bridge)
// -----------------------------------------------------------------------------

/**  READ
 * The user_i2c_read function is used to read data from I2C.
 * Its arguments are imposed by the BME280 library:
 * - reg_addr: internal register address in the sensor to read from.
 * - reg_data: pointer to the buffer where read bytes will be stored.
 * - len: number of bytes to read.
 * - intf_ptr: void pointer from which we can directly extract the I2C device address.
 *
 * Return type (BME280_INTF_RET_TYPE) is a portable macro in bme280_defs.h (usually int8_t).
 * This ensures the library's universality, allowing it to run on any microcontroller or OS.
 * BME280_INTF_RET_TYPE Returns 0 (BME280_OK) on success, or an error code on failure.
 *
 * Usage: Assigned to the BME280 library during initialization (bme280_device.read = user_i2c_read).
 */
static BME280_INTF_RET_TYPE user_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr) {

    // Cast the void pointer to uint8_t pointer, then extract the actual value (dereference)
    uint8_t dev_addr = *(uint8_t*)intf_ptr;

    /*
     * --- POINTERS QUICK REFERENCE ---
     * '&' (Address-of) : Gets the physical memory address of a variable.
     * '*' (Declaration): Indicates that a variable is a pointer (holds an address).
     * '*' (Dereference): Goes to the stored address and extracts the actual value.
     *
     * --- BREAKDOWN OF: *(uint8_t*)intf_ptr ---
     * 1. (uint8_t*) : Casts the generic 'void' pointer to a specific 8-bit pointer.
     * 2. *( ... )   : Dereferences it to fetch the actual 8-bit value from memory
     * (e.g., extracting the I2C device address like 0xEC).
     */

    // HAL_I2C_Mem_Read returns a status: HAL_OK, HAL_ERROR, HAL_BUSY, or HAL_TIMEOUT
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(bme280_hi2c, dev_addr, reg_addr, I2C_MEMADD_SIZE_8BIT, reg_data, len, 100);
    // HAL_I2C_Mem_Read executes a complete memory read transaction:
    // It connects to 'dev_addr', points to the internal register 'reg_addr' (8-bit size),
    // and reads 'len' bytes of data into the 'reg_data' buffer, with a 100ms timeout.

    // Short IF syntax (ternary operator):
    // If status == HAL_OK, return BME280_OK. Otherwise, return BME280_E_COMM_FAIL.
    return (status == HAL_OK) ? BME280_OK : BME280_E_COMM_FAIL;
}


/**  WRITE
 * @brief I2C write wrapper function matching the BME280 library signature.
 * @param reg_addr Internal register address in the BME280 sensor to write to.
 * @param reg_data Pointer to the data buffer containing bytes to be sent. The 'const' keyword ensures data safety.
 * @param len Number of bytes to send.
 * @param intf_ptr Pointer to the interface data (contains the shifted I2C device address).
 * @return BME280_INTF_RET_TYPE Returns 0 (BME280_OK) on success, or an error code on failure.
 */
static BME280_INTF_RET_TYPE user_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    // Extract the physical I2C device address
    uint8_t dev_addr = *(uint8_t*)intf_ptr;

    // Send 'len' bytes of data to the specified 'reg_addr' inside the sensor.
    // Note: We cast 'reg_data' 'const uint8_t*' to 'uint8_t*' because the STM32 HAL library
    // lacks the 'const' qualifier in its function signature, which would cause a compiler warning.
    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(bme280_hi2c, dev_addr, reg_addr, I2C_MEMADD_SIZE_8BIT, (uint8_t*)reg_data, len, 100);

    // Translate STM32 HAL status code to BME280 library status code
    return (status == HAL_OK) ? BME280_OK : BME280_E_COMM_FAIL;
}


/**  DELAY
 * @brief Delay wrapper function matching the BME280 library signature.
 * @param period Delay time required by the sensor, in microseconds (us).
 * @param intf_ptr Pointer to the interface data (unused in this blocking delay).
 * The 'intf_ptr' argument is provided by the library for OS integration (to identify the delayed device),
 * but it is unused in our bare-metal STM32 setup.
 */
static void user_delay_us(uint32_t period, void *intf_ptr) {
    /* * STM32 HAL_Delay() only supports milliseconds.
     * We convert microseconds to milliseconds by dividing by 1000.
     * The "+ 1" ensures we ALWAYS round up (e.g., 500us becomes 1ms).
     * It is much safer to wait slightly longer than to read data before the sensor finishes measuring.
     */
    uint32_t delay_ms = (period / 1000) + 1;

    HAL_Delay(delay_ms);
}


// -----------------------------------------------------------------------------
// PUBLIC FUNCTIONS
// -----------------------------------------------------------------------------

/*
 * @brief Initializes the BME280 sensor hardware, API interface, and measurement settings.
 * * @details This function performs a complete setup sequence:
 * 1. Configures the STM32 I2C handle and shifted device address.
 * 2. Pings the hardware to verify the physical connection.
 * 3. Binds STM32 wrapper functions (read, write, delay) to the BME280 API.
 * 4. Initializes the sensor and downloads factory calibration coefficients.
 * 5. Applies default measurement parameters (1x oversampling, IIR filter OFF).
 * 6. Sets the sensor to NORMAL power mode for continuous background readings.
 * * @return int8_t Execution status. Returns BME280_OK (0) on success, or a negative
 * error code (e.g., BME280_E_DEV_NOT_FOUND) if initialization fails.
 */
int8_t BME280_Hardware_Init(void) {

	// Initialize a status tracker variable. It will be overwritten by each sensor function
	// to check for communication errors.
    int8_t rslt = BME280_OK;

    // 1. Assigning a pointer to i2c information
    bme280_hi2c = &hi2c1;


    // 2. Shift the 7-bit I2C address to the left (STM32 HAL requirement)
    bme280_dev_addr = BME280_I2C_ADDR_PRIM << 1;
    // There are 2 hardware addresses defined in the library file (bme280_defs.h):
    // BME280_I2C_ADDR_PRIM (0x76) and BME280_I2C_ADDR_SEC (0x77).
    // We use the primary physical address of the sensor here.


    // 3. Ping the device to check if it's physically connected
    if (HAL_I2C_IsDeviceReady(bme280_hi2c, bme280_dev_addr, 3, 100) != HAL_OK) {

    	/*
    	* HAL_I2C_IsDeviceReady checks if the I2C device acknowledges its address on the bus.
    	* Arguments:
    	* - bme280_hi2c   : Pointer to the I2C peripheral handle (e.g., &hi2c1).
    	* - bme280_dev_addr: The shifted 8-bit target device address (0xEC).
    	* - 3             : Number of trials (attempts to connect).
    	* - 100           : Timeout duration in milliseconds for each trial.
    	* Returns:
    	* - HAL_StatusTypeDef: HAL_OK if the device responds, otherwise HAL_ERROR, HAL_BUSY, or HAL_TIMEOUT.
    	*/

        return BME280_E_DEV_NOT_FOUND; // Hardware error (e.g., disconnected wire)
        // BME280_E_DEV_NOT_FOUND - BME280 library specific error code.
    }


    // 4. Configure the BME280 library "Bridge"
    // Bind our custom STM32 wrapper functions to the BME280 device structure.
    // This gives the library the physical means to communicate over I2C and execute delays.
    bme280_device.intf_ptr  = &bme280_dev_addr;
    bme280_device.intf 		= BME280_I2C_INTF;
    bme280_device.read 		= user_i2c_read;
    bme280_device.write 	= user_i2c_write;
    bme280_device.delay_us 	= user_delay_us;

    // 5. Initialize the sensor - a soft reset, verifies the Chip ID, and downloads
    // the factory calibration coefficients essential for data compensation.
    rslt = bme280_init(&bme280_device);
    if (rslt != BME280_OK) return rslt; // Abort if initialization fails

    // 6. Set up measurement parameters
    // Declare the 'settings' variable based on the library's struct
    // to store our measurement parameters before sending them to the sensor
    struct bme280_settings settings;

    // Fetch default settings first
    // Pass the address of 'settings' to fill it with current sensor data.
    rslt = bme280_get_sensor_settings(&settings, &bme280_device);
    if (rslt != BME280_OK) return rslt; // Abort and return error code if communication fails.

    //----------------------------------------
    //			 ACCURACY SETTINGS
    //----------------------------------------
    // Average 1 sample (Options: 1X, 2X, 4X, 8X, 16X)
    settings.osr_h  = BME280_OVERSAMPLING_1X; // Humidity oversampling
    settings.osr_p  = BME280_OVERSAMPLING_1X; // Pressure oversampling
    settings.osr_t  = BME280_OVERSAMPLING_1X; // Temperature oversampling
    settings.filter = BME280_FILTER_COEFF_OFF; // Noise filter - sudden measurement jumps (Options: OFF, 2, 4, 8, 16)

    // Create a checklist of WHICH settings to update.
    // This mask does NOT contain the actual values (1X, OFF), only the selection.
    // Actual values are in the 'settings' struct
    uint8_t settings_sel = BME280_SEL_OSR_PRESS | BME280_SEL_OSR_TEMP | BME280_SEL_OSR_HUM | BME280_SEL_FILTER;

    // Apply the selected parameters (settings_sel) and their values (&settings) to the specific sensor instance (&bme280_device).
    rslt = bme280_set_sensor_settings(settings_sel, &settings, &bme280_device);
    if (rslt != BME280_OK) return rslt; // If the I2C/SPI transmission fails, return the error code immediately.


    // 7. Switch the sensor to NORMAL mode (continuous background measurements)
    // Other options: FORCED (one-time measurement then sleep) or SLEEP (standby but comunication is still working).
    rslt = bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &bme280_device);

    return rslt; // Return final initialization status to the main program.
}



/**
 * @brief Fetches and compensates temperature, pressure, and humidity data from the BME280 sensor.
 * * @param[out] results Pointer to a custom structure where data will be stored.
 * @return int8_t Execution status (BME280_OK on success, negative code on error).
 * * @note Pressure is automatically converted from Pascals (Pa) to Hectopascals (hPa).
 */
int8_t BME280_Read_Measurements(BME280_Results *results) {
    struct bme280_data comp_data; // Declare the 'comp_data' structure based on the "bme280_data" from BME280 library.
    int8_t rslt; // Declare the "rslt" variable for return result or error code.

    // Read all sensor data (Temp, Press, Hum) and apply factory compensation math.
    // Read data: [What to read], [Where to save it], [Which sensor to use] -> Returns status.
    rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, &bme280_device);

    // Write data to the structure only if the I2C/SPI read was successful
    if (rslt == BME280_OK) {
    	// Use '->' operator to assign values to the pointer struct.
    	results->temperature = comp_data.temperature;
        results->humidity 	 = comp_data.humidity;
        results->pressure 	 = comp_data.pressure / 100.0; // Pressure is divided by 100 to convert from Pascals (Pa) to Hectopascals (hPa)
    }

    return rslt; // Return final status to the main program.
}

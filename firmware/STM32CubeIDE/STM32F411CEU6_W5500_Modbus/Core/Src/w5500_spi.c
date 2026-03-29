// ==============================================================================
// === W5500 SPI ================================================================
// ==============================================================================

#include "w5500_spi.h"
#include "wizchip_conf.h"

extern SPI_HandleTypeDef hspi1; // Set SPI1 (hardware port - hspi1)



// ==========================================
// HELPER FUNCTIONS
// ==========================================

// CS (Chip Select)
void W5500_Select(void) {
    HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_RESET);
}

void W5500_Unselect(void) {
    HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_SET);
}


// SPI (Read/Write)
// Transmits a single byte of data to the W5500 chip via SPI1
// byte: The 8-bit data payload to be transmitted
void W5500_WriteByte(uint8_t byte) {
    HAL_SPI_Transmit(&hspi1, &byte, 1, HAL_MAX_DELAY);

    // &hspi1        - Pointer to the SPI1 handle
    // &byte         - Memory address of the byte to send
    // 1             - Number of bytes to transmit
    // HAL_MAX_DELAY - Block execution until transmission is complete
}

// Reads a single byte from the W5500 chip via SPI1
uint8_t W5500_ReadByte(void) {
    uint8_t rx = 0;		// Stores the received byte and is returned by the function
    uint8_t tx = 0xFF;  // Dummy byte (sent to generate the SPI clock)
    HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, HAL_MAX_DELAY);
    return rx;

    /* Since SPI is full-duplex, a dummy byte (0xFF) must be transmitted to generate the SCK clock pulses
    required for receiving data */
    // &hspi1 		 - Pointer to the SPI1 handle
    // &tx 			 - Pointer to the dummy data to transmit
    // &rx P		 - ointer to the buffer where received data will be saved
    // 1 			 - Data size (exchange exactly 1 byte)
    // HAL_MAX_DELAY - Timeout (block execution until exchange is complete)
}



// ==========================================================================================
// MAIN FUNCTION Initializes the W5500 hardware and sets up the static network configuration
// ==========================================================================================
void W5500_Hardware_Init(void) {

	// Hardware Reset Sequence, RST pin (active low)
	HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_RESET);
	HAL_Delay(10); // Hold reset for 10ms to ensure complete power cycle
	HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_SET);
	HAL_Delay(50); // Minimum stabilization time recommended by the datasheet


	// Register SPI/CS callbacks
	// Link the WIZnet core library with the helper functions
    reg_wizchip_cs_cbfunc(W5500_Select, W5500_Unselect); 	 // CS
    reg_wizchip_spi_cbfunc(W5500_ReadByte, W5500_WriteByte); // SPI


    /* The W5500 chip has 32KB of internal RAM (16KB for RX, 16KB for TX).
    This memory is shared among 8 independent logical hardware sockets.
    Here we allocate evenly: 2KB of RX and 2KB of TX memory per socket.
    */
    uint8_t rx_tx_buff_sizes[] = {2, 2, 2, 2, 2, 2, 2, 2}; // RAM allocation to each socket
    wizchip_init(rx_tx_buff_sizes, rx_tx_buff_sizes);	   // Sets the RAM allocation for the buffer (RX, TX)

    // Example usage of multiple sockets over a single physical cable:
    // - Socket 0: Modbus TCP Server (Port 502)  <-- We are using this one!
    // - Socket 1: HTTP Web Server (Port 80)
    // - Socket 2: MQTT IoT Client (Port 1883)
    // - Socket 3..7: Other network tasks (e.g., NTP time sync, FTP)


    // Structure with NETWORK CONFIGURATION
    wiz_NetInfo netInfo = {
    		.mac  = {0x00, 0x08, 0xdc, 0xab, 0xcd, 0xef}, // MAC Address (Physical ID, 00:08:DC is WIZnet OUI)
    		.ip   = {192, 168, 0, 4},                     // Static IP Address of the STM32
    		.sn   = {255, 255, 255, 0},                   // Subnet Mask (Determines the local network range)
    		.gw   = {192, 168, 0, 1},                     // Default Gateway (Router IP address)
    		.dns  = {8, 8, 8, 8},                         // DNS Server (Google's public DNS)
    		.dhcp = NETINFO_STATIC                        // Mode: Static IP (No DHCP client used)
    };

    wizchip_setnetinfo(&netInfo); // Transferring data netInfo to W5500 memory
}

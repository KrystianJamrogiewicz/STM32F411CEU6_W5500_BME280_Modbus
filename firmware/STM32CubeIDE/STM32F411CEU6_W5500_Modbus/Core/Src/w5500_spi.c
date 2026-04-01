// ==============================================================================
// === W5500 SPI ================================================================
// ==============================================================================

#include "w5500_spi.h"
#include "wizchip_conf.h"


// ==========================================
// HELPER FUNCTIONS
// ==========================================

// CS (Chip Select) management
void W5500_Select(void) {
    HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_RESET);
}

void W5500_Unselect(void) {
    HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_SET);
}

// SPI (Single Byte Write)
void W5500_WriteByte(uint8_t byte) {
    HAL_SPI_Transmit(&hspi1, &byte, 1, HAL_MAX_DELAY);
}

// SPI (Single Byte Read)
uint8_t W5500_ReadByte(void) {
    uint8_t rx = 0;
    uint8_t tx = 0xFF; // Dummy byte to generate SCK clock
    HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

/**
 * @brief SPI Burst Write - Optimized for high-speed block data transfer.
 * @param pBuf Pointer to the data buffer to be transmitted.
 * @param len Length of the data block.
 * Using HAL_SPI_Transmit for the whole block is much faster than byte-by-byte calls.
 */
void W5500_WriteBuff(uint8_t* pBuf, uint16_t len) {
    HAL_SPI_Transmit(&hspi1, pBuf, len, HAL_MAX_DELAY);
}

/**
 * @brief SPI Burst Read - Optimized for high-speed block data retrieval.
 * @param pBuf Pointer to the buffer where received data will be stored.
 * @param len Length of the data block to read.
 */
void W5500_ReadBuff(uint8_t* pBuf, uint16_t len) {
    // HAL_SPI_Receive automatically handles dummy byte generation
    HAL_SPI_Receive(&hspi1, pBuf, len, HAL_MAX_DELAY);
}


// ==========================================================================================
// MAIN FUNCTION - Initializes hardware, binds optimized callbacks, and sets network info
// ==========================================================================================

/**
 * @brief Full hardware and software initialization for W5500.
 * Sets up SPI burst callbacks and enables basic interrupt masking.
 */
void W5500_Hardware_Init(void) {

    // 1. Hardware Reset Sequence
    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(10); // Ensure reset is recognized
    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(50); // Stabilization time

    // 2. Register SPI/CS callbacks
    // Binding standard single-byte functions
    reg_wizchip_cs_cbfunc(W5500_Select, W5500_Unselect);
    reg_wizchip_spi_cbfunc(W5500_ReadByte, W5500_WriteByte);

    // Binding OPTIMIZED burst functions for high performance (e.g., Web Server, large Modbus frames)
    reg_wizchip_spiburst_cbfunc(W5500_ReadBuff, W5500_WriteBuff);


    // 3. WIZchip Initialization
    /* Allocate 2KB RX and 2KB TX memory to each of the 8 sockets (Total 32KB) */
    uint8_t rx_tx_buff_sizes[] = {2, 2, 2, 2, 2, 2, 2, 2};

    // Initialize with memory allocation and check for success (bulletproof approach)
    if (wizchip_init(rx_tx_buff_sizes, rx_tx_buff_sizes) != 0) {
        // Here you could add error handling if SPI communication fails
        return;
    }


    // 4. Network Configuration
    wiz_NetInfo netInfo = {
        .mac  = {0x00, 0x08, 0xdc, 0xab, 0xcd, 0xef}, // 00:08:DC is WIZnet OUI
        .ip   = {192, 168, 0, 4},
        .sn   = {255, 255, 255, 0},
        .gw   = {192, 168, 0, 1},
        .dns  = {8, 8, 8, 8},
        .dhcp = NETINFO_STATIC
    };

    wizchip_setnetinfo(&netInfo);


    // 5. Interrupt Configuration (using the W5500_INT pin)
    /* Enable interrupts for IP Conflict and Socket 0 (where Modbus usually sits).
       This allows the MCU to react instantly instead of constant polling. */
    setIMR(IK_IP_CONFLICT | IK_SOCK_0);
}

/**
 * @brief Handle W5500 Interrupts. Call this function from HAL_GPIO_EXTI_Callback
 * when the W5500_INT pin (PA2) triggers.
 */
void W5500_Process_Interrupt(void) {
    uint8_t ir = getIR(); // Read common interrupt register

    // Clear the handled interrupt bits by writing them back
    setIR(ir & (IK_IP_CONFLICT | IK_DEST_UNREACH));

    if (ir & IK_IP_CONFLICT) {
        // Logic: Notify user about IP address collision on the network
    }

    if (ir & IK_SOCK_0) {
        // Socket 0 event (e.g., Data Received, Disconnected)
        uint8_t sir = getSn_IR(0);
        setSn_IR(0, sir); // Clear socket interrupt

        if (sir & Sn_IR_RECV) {
            // Logic: New data arrived! Set a flag to process Modbus/HTTP in main loop
        }
    }
}

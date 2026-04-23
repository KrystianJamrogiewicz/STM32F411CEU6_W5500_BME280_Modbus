// ==============================================================================
// === W5500 SPI ================================================================
// ==============================================================================

#include "w5500_spi.h"
#include "wizchip_conf.h"
#include "app.h"
#include <stdio.h>


// ==========================================
// === HELPER FUNCTIONS
// ==========================================

// CS (Chip Select) management
static void W5500_Select(void) {
    HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_RESET);
}

static void W5500_Unselect(void) {
    HAL_GPIO_WritePin(W5500_CS_GPIO_Port, W5500_CS_Pin, GPIO_PIN_SET);
}

// SPI (Single Byte Write) byte-by-byte
static void W5500_WriteByte(uint8_t byte) {
    HAL_SPI_Transmit(&hspi1, &byte, 1, HAL_MAX_DELAY);
}

// SPI (Single Byte Read) byte-by-byte
static uint8_t W5500_ReadByte(void) {
    uint8_t rx = 0;
    uint8_t tx = 0xFF; // Dummy byte to generate SCK clock
    HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

/**
 * @brief SPI Burst Write - Optimized for high-speed block data transfer.
 * @param pBuf Pointer to the data buffer to be transmitted (e.g., Modbus frame, HTML page).
 * This makes the function universal - it doesn't create its own array,
 * it just acts as a "courier" for any external data array passed to it.
 * @param len Length of the data block to send.
 * * Using HAL_SPI_Transmit for the whole block is much faster than byte-by-byte calls.
 */
static void W5500_WriteBuff(uint8_t* pBuf, uint16_t len) {

    // pBuf is just a memory address. The physical array (like tx_buff[512]) lives elsewhere!
    HAL_SPI_Transmit(&hspi1, pBuf, len, HAL_MAX_DELAY);
}

/**
 * @brief SPI Burst Read - Optimized for high-speed block data retrieval.
 * @param pBuf Pointer to the empty buffer where received data will be stored.
 * @param len Length of the data block to read.
 */
static void W5500_ReadBuff(uint8_t* pBuf, uint16_t len) {

    // HAL_SPI_Receive automatically handles dummy byte generation
    HAL_SPI_Receive(&hspi1, pBuf, len, HAL_MAX_DELAY);
}



// ==========================================================================================
// MAIN FUNCTION - Initializes hardware, binds callbacks, and sets network info
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

    // Binding OPTIMIZED burst functions for high performance (e.g., Web Server)
    reg_wizchip_spiburst_cbfunc(W5500_ReadBuff, W5500_WriteBuff);


    // 3. WIZchip Initialization
    /* Allocate 2KB RX and 2KB TX memory to each of the 8 sockets (Total 32KB) */
    uint8_t rx_tx_buff_sizes[] = {2, 2, 2, 2, 2, 2, 2, 2};

    // Initialize with memory allocation and check for success.
    if (wizchip_init(rx_tx_buff_sizes, rx_tx_buff_sizes) != 0) {
        // Place for error handling if SPI communication fails.
        return;
    }


    // 4. Network Configuration
    wiz_NetInfo netInfo = {
        .mac  = {0x00, 0x08, 0xdc, 0xab, 0xcd, 0xef}, // MAC Address: Physical hardware ID. '00:08:DC' is the official WIZnet manufacturer prefix.
        .ip   = {192, 168, 0, 4},                     // IP Address: The static logical address of your STM32 on the local network.
        .sn   = {255, 255, 255, 0},                   // Subnet Mask: Defines the local network boundaries (devices must match the first 3 numbers to see each other).
        .gw   = {192, 168, 0, 1},                     // Default Gateway: The IP address of your main router (the doorway to the Internet/other networks).
        .dns  = {8, 8, 8, 8},                         // DNS Server: Translates web names to IPs (e.g., google.com -> 142.250.190.46). 8.8.8.8 is Google's public DNS.
        .dhcp = NETINFO_STATIC                        // DHCP Mode: 'NETINFO_STATIC' forces the W5500 to use the exact IP defined above instead of asking the router for one.
    };

    wizchip_setnetinfo(&netInfo); // Set network parameters


    // 5. Setting what activates the interrupt - Interrupt Mask Configuration -
    // W5500 uses two separate registers for masking interrupts

    // GLOBAL REGISTER - It is responsible for global errors, not related to specific connections or sockets.
    // I allow you to activate the INT pin if you detect on our LAN that any other device has the same IP address as us (IP Conflict).
    setIMR(IK_IP_CONFLICT | IK_DEST_UNREACH); // IK_IP_CONFLICT or IK_DEST_UNREACH is activate the interrupt by global register.

    // IK_IP_CONFLICT  		- ERROR The IP of the W5500 module is the same as the IP of another device on the network.
    // IK_DEST_UNREACH 		- ERROR - Destination IP address does not exist or device is disabled.
	// IK_MAGIC_PACKET 		- NOTIFICATION - Used to wake up STM32 after detecting a "Magic Packet" a specially crafted data frame with the MAC number of the W5500 module.
    // IK_PPPOE_TERMINATED  - NOTIFICATION in 99% do not use - Used only in the old "PPPoE" standard - informs that the operator has dropped the connection.

    // SOCKET REGISTER - This 8-bit register allows you to select which slots can activate the interrupt.
    setSIMR(1 << 0);        // 1 << 0) = move digit 1 left 0 places 00000001 Socket Interrupt Mask: Enable interrupts for Socket 0
    // If socket 1 is also in use: setSIMR((1 << 0) | (1 << 1));
}



// ==============================================================================
// INTERRUPT HANDLER
// ==============================================================================

// This variable inform about INTERRUPT on W5500 module, it exists in app.c
extern volatile uint8_t W5500_Event_Flag;

// A variable that stores the current state of the state machine from the app.c file
extern SystemState_t current_state;

/**
 * @brief Universal Interrupt Handler for W5500 (Call from EXTI Callback in app.c)
 * This function filters W5500 interrupts to save STM32 CPU cycles.
 */
void W5500_Process_Interrupt(void) {

    // --------------------------------------------------------------------------
    // 1. GLOBAL INTERRUPTS (IR)
    // --------------------------------------------------------------------------
    // getIR() reads the global W5500 interrupt register (e.g., IP Conflict).
    // This function does NOT check for incoming Modbus data, only global chip events.
    uint8_t ir = getIR();

    if (ir > 0) {
        // Clear the handled interrupts by writing a '1' back to the specific bits.
        // We use bitwise OR (|) to combine masks, and bitwise AND (&) to filter out
        // ONLY the bits we want to clear, preventing accidental deletion of other flags.
        setIR(ir & (IK_IP_CONFLICT | IK_DEST_UNREACH));

        // Check if the bit (IP Conflict) is set
        if (ir & IK_IP_CONFLICT) {
            // CRITICAL ERROR: Someone else on the network has our IP address!
            wiz_NetInfo readInfo;
            wizchip_getnetinfo(&readInfo); // Read network data from the W5500 module

        	sprintf(err_msg, "\r\n[CRITICAL] W5500 IP CONFLICT! (IP %d.%d.%d.%d is TAKEN).\r\n",
        			readInfo.ip[0], readInfo.ip[1], readInfo.ip[2], readInfo.ip[3]);
            current_state = STATE_NETWORK_ERROR; // Uncomment to lock the FSM in app.c
        }
        // Check if the bit (IP IK_DEST_UNREACH) is set
        if (ir & IK_DEST_UNREACH) {
            // ERROR: Destination IP address does not exist or device is disabled.
        	sprintf(err_msg, "\r\nDestination IP address does not exist or device is disabled.\r\n");
            current_state = STATE_NETWORK_ERROR; // Uncomment to lock the FSM in app.c
        }
    }


    // --------------------------------------------------------------------------
    // 2. SOCKET INTERRUPT (SIR)
    // --------------------------------------------------------------------------
    // SIR (Socket Interrupt Register) tells us which of the 8 sockets is calling.
    // Bit 0 = Socket 0, Bit 1 = Socket 1, etc.
    uint8_t sir = getSIR();

    // Check if Socket 0 (Our Modbus TCP Server) triggered an event: (1 << 0), socket 1: (1 << 1), etc.
    if (sir & (1 << 0)) {

        // ----------------------------------------------------------------------
        // 3. SOCKET-SPECIFIC INTERRUPTS (Sn_IR)
        // ----------------------------------------------------------------------
        // Now we ask Socket 0: "Exactly what happened to you?"
        // getSn_IR(0) reads the detailed event register for Socket 0.
        uint8_t sn_ir = getSn_IR(0);

        // Acknowledge and clear all the interrupts Socket 0 just reported
        setSn_IR(0, sn_ir);

        // ----------------------------------------------------------------------
        // 4. INTELLIGENT WAKE-UP (Data Check)
        // ----------------------------------------------------------------------
        // Sn_IR_RECV means new data arrived from the network and is waiting in RAM.
        if (sn_ir & Sn_IR_RECV) {

            // We ONLY wake up the main Modbus_Loop() in app.c if there is actual data.
            // Events like "Data sent successfully" are silently cleared above and ignored.
            W5500_Event_Flag = 1;
        }

        // Future examples of other socket events you can handle:
        // if (sn_ir & Sn_IR_DISCON) { /* Client disconnected */ }
        // if (sn_ir & Sn_IR_CON)    { /* New client connected */ }
    }
}

#include "modbus_tcp.h"
#include "W5500/socket.h"
#include <string.h>

#define MB_TCP_PORT 502 // The standard Modbus TCP port is 502
#define MB_SOCKET   0 // Socked selection from 0 to 7 (One socked for one task)



// Transmission register array (type 16-bit = 2 byte standard in ModbusTCP)
// Array size (MB_REG_COUNT in .h) = Number of registers

uint16_t MB_HoldingRegisters[MB_REG_COUNT] = {0};

//uint16_t MB_HoldingRegisters[10] = {0};
// OR
// uint16_t MB_HoldingRegisters[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// Limit: ModbusTCP frame size is limited (max. ~260 bytes)
// The safe limit is 120 registers = 240 bytes
/*
   This is a limit for one transfer, after exceeding this number
   the data will simply be sent in several packages, which increases the transfer time
*/



// Network Communication buffers array (type 8-bit = 1 byte) The wire transmits data in bytes one by one
// Array size = Buffer Size in Bytes

uint8_t rx_buff[512]; // Transmit buffer
uint8_t tx_buff[512]; // Receive buffer

/* The maximum Modbus TCP frame is ~260 bytes but use 512 bytes to provide a safety margin
   for handling multiple packets arriving quickly
*/



// Initialize the Modbus TCP Server
void Modbus_Init() {

    socket(MB_SOCKET, Sn_MR_TCP, MB_TCP_PORT, 0); /* Configuration the hardware socket:
    B_SOCKET: Select the hardware socket index (0 - 7)
    Sn_MR_TCP: Set the protocol mode to TCP (reliable connection)
    MB_TCP_PORT: Set the local port (502 - standard Modbus port)

    This changes the state of the system from SOCK_CLOSED to SOCK_INIT
    */

    listen(MB_SOCKET);	// Start listening for incoming client connections
    // This changes the state of the system from SOCK_INIT to SOCK_LISTEN
}



// Helper function to send a Modbus Exception Response
// Used when the device cannot process the Master's request
void send_exception(uint8_t funcCode, uint8_t exceptionCode, uint16_t transId) {

	// --- MBAP (Modbus Application Protocol) Header - The "Envelope" ---

	// 1. Transaction ID: Echo back the ID received from the Master
	// Splitting a 16-bit value into two 8-bit ones
	tx_buff[0] = transId >> 8; // Shift bits right 8 positions (Extracts the High Byte)
	tx_buff[1] = transId & 0xFF; /* Compare a 16-bit variable to an 8-bit variable "0xFF"
	0xFF = 255 = 1111 1111 = 0000 0000 1111 1111 (Extracts the Low Byte) */

	// 2. Protocol ID: Always 0 for Modbus TCP
	tx_buff[2] = 0;
	tx_buff[3] = 0;

	// 3. Length: Always 3. Number of following bytes (Unit ID + Func Code + Ex Code = 3 bytes)
	tx_buff[4] = 0;
	tx_buff[5] = 3;

	// 4. Unit ID: Address of the slave (defaults to 1 or copied from request)
	tx_buff[6] = rx_buff[6];


	// --- PDU (Protocol Data Unit) - Exception Body - The "Letter" ---

	// 5. Function Code with Error Flag:
	// Set the MSB (Most Significant Bit) to 1 with 0x80 = 1000 0000 = 128
	tx_buff[7] = funcCode | 0x80; // Add a 8-bit variable Error Flag "0x80" to variable funcCode

	// 6. Exception Code: The specific reason for the error
	// 0x01: Illegal Function
	// 0x02: Illegal Data Address
	// 0x03: Illegal Data Value
	tx_buff[8] = exceptionCode;

	// Send the constructed 9-byte frame via the socket
	send(MB_SOCKET, tx_buff, 9);
}



void Modbus_Loop() {
    // Check the current status of the hardware socket
    uint8_t sn_state = getSn_SR(MB_SOCKET);

    switch(sn_state) {
        case SOCK_ESTABLISHED:
            // Clear the connection interrupt flag (required by W5500 hardware)
            if(getSn_IR(MB_SOCKET) & Sn_IR_CON) {
                setSn_IR(MB_SOCKET, Sn_IR_CON);
            }

            // Check how many bytes have been received in the hardware buffer
            uint16_t len = getSn_RX_RSR(MB_SOCKET);

            if(len > 0) {
                // --- BULLETPROOFING 1: Buffer Overflow Protection ---
                // Ensure we don't read more bytes than our software array (rx_buff) can hold.
                // If a network error sends 1000 bytes, we cap it to 512 to save the microcontroller's RAM.
                if (len > 512) {
                    len = 512;
                }

                // Read data from W5500 hardware buffer into our software rx_buff
                recv(MB_SOCKET, rx_buff, len);

                // --- BULLETPROOFING 2: Minimum Frame Size ---
                // A valid Modbus TCP request (Read/Write) must be at least 12 bytes long:
                // 7 bytes (Header) + 1 byte (Func Code) + 2 bytes (Address) + 2 bytes (Value/Count)
                if (len < 12) {
                    return; // The frame is too short to be valid. Ignore this network garbage.
                }

                // --- MODBUS TCP PARSING ---

                // 1. Extract the MBAP Header
                uint16_t transId = (rx_buff[0] << 8) | rx_buff[1]; // Transaction ID (Echoed back)
                uint16_t protoId = (rx_buff[2] << 8) | rx_buff[3]; // Protocol ID
                uint16_t mbapLen = (rx_buff[4] << 8) | rx_buff[5]; // Declared Length
                uint8_t funcCode = rx_buff[7];                     // What to do? (03 or 06)

                // --- BULLETPROOFING 3: Protocol ID Verification ---
                // The Modbus TCP standard dictates that Protocol ID must always be 0x0000.
                if (protoId != 0x0000) {
                    return; // Ignore, this is not a Modbus TCP packet.
                }

                // --- BULLETPROOFING 4: Frame Length Consistency ---
                // mbapLen tells us how many bytes follow the Length field itself.
                // Total expected frame size = 6 bytes (Header before Length) + mbapLen.
                if (len < (6 + mbapLen)) {
                    return; // Fragmented or cut-off packet. Ignore, the Master will resend.
                }

                // Copy Transaction ID and Protocol ID to the response (always the same)
                tx_buff[0] = rx_buff[0];
                tx_buff[1] = rx_buff[1];
                tx_buff[2] = rx_buff[2];
                tx_buff[3] = rx_buff[3];
                tx_buff[6] = rx_buff[6]; // Unit ID (Device address)

                // 2. Function Handling
                if (funcCode == 0x03) // READ HOLDING REGISTERS
                {
                    uint16_t startAddr = (rx_buff[8] << 8) | rx_buff[9]; // From which register?
                    uint16_t count = (rx_buff[10] << 8) | rx_buff[11];   // How many registers?

                    // --- BULLETPROOFING 5: Modbus Standard Limits ---
                    // Modbus standard forbids reading 0 registers or more than 125 at once.
                    if (count == 0 || count > 125) {
                        send_exception(funcCode, 0x03, transId); // Error 0x03: Illegal Data Value
                    }
                    // Protection: check if they are asking for too much (Array Size is 10)
                    else if ((startAddr + count) > MB_REG_COUNT) {
                        send_exception(funcCode, 0x02, transId); // Error 0x02: Illegal Data Address
                    }
                    else {
                        // Build the response
                        tx_buff[7] = 0x03;          // Function Code
                        tx_buff[8] = count * 2;     // Number of data bytes (2 bytes per register)

                        // Loop copying data from MB_HoldingRegisters to the transmit buffer
                        for (int i = 0; i < count; i++) {
                            tx_buff[9 + i*2] = MB_HoldingRegisters[startAddr + i] >> 8;     // High byte
                            tx_buff[10 + i*2] = MB_HoldingRegisters[startAddr + i] & 0xFF;  // Low byte
                        }

                        // Fill in the length of the entire frame (Data bytes + 3 bytes of header remainder)
                        uint16_t packetLen = 3 + (count * 2);
                        tx_buff[4] = packetLen >> 8;
                        tx_buff[5] = packetLen & 0xFF;

                        // Send the response! (6 bytes of MBAP start + packetLen)
                        send(MB_SOCKET, tx_buff, 6 + packetLen);
                    }
                }
                else if (funcCode == 0x06) // WRITE SINGLE REGISTER
                {
                    uint16_t addr = (rx_buff[8] << 8) | rx_buff[9];  // Where to write?
                    uint16_t val  = (rx_buff[10] << 8) | rx_buff[11];// What to write?

                    // Protection: check if the address exists (Array Size is 10)
                    if (addr >= MB_REG_COUNT) {
                        send_exception(funcCode, 0x02, transId); // Error 0x02: Illegal Data Address
                    }
                    else {
                        // SAVE TO MEMORY
                        MB_HoldingRegisters[addr] = val;

                        // Echo - send back exactly what we received (Confirmation)
                        tx_buff[4] = 0; // Length High
                        tx_buff[5] = 6; // Length Low (UnitID + Func + Addr + Val = 6 bytes)
                        tx_buff[7] = 0x06;
                        tx_buff[8] = rx_buff[8];
                        tx_buff[9] = rx_buff[9];
                        tx_buff[10] = rx_buff[10];
                        tx_buff[11] = rx_buff[11];

                        send(MB_SOCKET, tx_buff, 12);
                    }
                }
                else {
                    // Unknown function
                    send_exception(funcCode, 0x01, transId); // Error 0x01: Illegal Function
                }
            }
            break;

        case SOCK_CLOSE_WAIT:
            disconnect(MB_SOCKET);
            break;

        case SOCK_CLOSED:
            // Reopen the socket to listen for new connections
            socket(MB_SOCKET, Sn_MR_TCP, MB_TCP_PORT, 0);
            listen(MB_SOCKET);
            break;
    }
}


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
    // Check the current state of the hardware socket
    uint8_t sn_state = getSn_SR(MB_SOCKET);

    // State machine
    switch(sn_state) {


    	// Connection established
        case SOCK_ESTABLISHED:

            // Clear the connection interrupt flag (required by W5500 hardware)
        	if(getSn_IR(MB_SOCKET) & Sn_IR_CON) {
        	    // getSn_IR() - Read the 8-bit interrupt register and & Sn_IR_CON - check the connection establishment bit
        	    // if (getSn_IR(MB_SOCKET) & Sn_IR_CON) is true, it means a connection has been established

        	    setSn_IR(MB_SOCKET, Sn_IR_CON);
        	    // Clearing the flag is done by writing a true to it
        	    // setSn_IR - acknowledges and clears the flag for: Sn_IR_CON (indicating a successful connection)
        	}

    		// Check the exact number of BYTES waiting to be read from the W5500 RX buffer
            uint16_t len = getSn_RX_RSR(MB_SOCKET);

            // --- VALIDATION 1: Payload Size Verification / Buffer Overflow Protection ---
            if(len > 0) {

            	// Limit the incoming data to 512 bytes to prevent buffer overflow and protect MCU RAM.
            	// A valid Modbus TCP frame never exceeds 260 bytes anyway.
                if (len > 512) {
                    len = 512;
                }



				// Fetch the Modbus frame from the W5500 hardware buffer into the MCU's RAM buffer (rx_buff)
				recv(MB_SOCKET, rx_buff, len);



				// --- VALIDATION 2: Minimum Frame Size ---
				// A valid Modbus TCP request (Read/Write) must be at least 12 bytes long:
				if (len < 12) {
					return; // Abort processing and exit the function (Modbus_Loop) immediately. Drop this invalid frame.
				}

				// ==============================================================================
				// === REFERENCE: MODBUS TCP RX FRAME ANATOMY (12-Byte Request Map) ===
				// ==============================================================================

				// --- MBAP (Modbus Application Protocol) Header (7 bytes) - The "Envelope" ---

				// 1. Transaction ID (2 bytes): Identifier of the request set by the Master.
				// We must echo this exact ID back in our response so the Master knows we replied.
				// Byte 0: Transaction ID High Byte
				// Byte 1: Transaction ID Low Byte

				// 2. Protocol ID (2 bytes): Always 0x0000 for the Modbus TCP standard.
				// Byte 2: Protocol ID High Byte (0x00)
				// Byte 3: Protocol ID Low Byte (0x00)

				// 3. Length (2 bytes): Number of following bytes remaining in this frame.
				// For a standard 12-byte request, this is usually 6 (1 byte Unit ID + 5 bytes PDU).
				// Byte 4: Length High Byte
				// Byte 5: Length Low Byte

				// 4. Unit ID (1 byte): Address of the specific target device (Slave).
				// Default 1 or copied from request (tx_buff[6] = rx_buff[6])
				// Byte 6: Unit ID


				// --- PDU (Protocol Data Unit) (5 bytes minimum) - The "Letter" inside ---

				// 5. Function Code (1 byte): What does the Master want us to do?
				// e.g., 0x03 = Read Holding Registers, 0x06 = Write Single Register.
				// Byte 7: Function Code

				// 6. Address (2 bytes): The starting register address the Master is asking for.
				// e.g., Register No. 0.
				// Byte 8: Address High Byte
				// Byte 9: Address Low Byte

				// 7. Value / Count (2 bytes): The data payload of the request.
				// If reading (0x03): How many registers the Master wants to read (e.g., 2 registers).
				// If writing (0x06): The actual value to write into the register.
				// Byte 10: Value / Count High Byte
				// Byte 11: Value / Count Low Byte

				// Total: 7 bytes (MBAP) + 5 bytes (PDU) = 12 bytes



				// --- MODBUS TCP PARSING ---
				// Reconstruct 16-bit (Little-Endian - Low Byte, High Byte) values from two 8-bit values Modbus (Big-Endian - High Byte, Low Byte) format: (High_Byte << 8) | Low_Byte
				uint16_t transId = (rx_buff[0] << 8) | rx_buff[1]; // Master's sequence number (must be echoed back)
				uint16_t protoId = (rx_buff[2] << 8) | rx_buff[3]; // Protocol ID (Must be 0x0000 for Modbus TCP)
				uint16_t mbapLen = (rx_buff[4] << 8) | rx_buff[5]; // Length of the remaining bytes in this frame
				// Unit ID (rx_buff[6]) is skipped here as it's directly copied during response building
				uint8_t funcCode = rx_buff[7];                     // The requested action (e.g., 0x03 - Read or 0x06 - Write)



				// --- VALIDATION 3: Protocol ID Verification ---
				// The Modbus TCP standard dictates that Protocol ID must always be 0x0000
				if (protoId != 0x0000) {
					return; // Ignore, this is not a Modbus TCP packet. Abort processing and exit the function (Modbus_Loop) immediately
				}

				// --- VALIDATION 4: Frame Completeness Check (TCP Stream) ---
				// len: The actual physical number of bytes received from the W5500 RX buffer
				// mbapLen declares the payload size AFTER the Length field
				// A complete frame requires: 6 bytes (MBAP start) + mbapLen
				if (len < (6 + mbapLen)) {
					return; // Incomplete frame (fragmented). Abort processing and exit the function (Modbus_Loop) immediately
				}



				// ==============================================================================
				// === REFERENCE: MODBUS TCP TX FRAME ANATOMY (Function 0x03 RESPONSE) ===
				// ==============================================================================

				// --- MBAP (Modbus Application Protocol) Header (7 bytes) - The "Envelope" ---

				// 1. Transaction ID (2 bytes): Echoed exactly as received from the Master.
				// Byte 0: Transaction ID High Byte
				// Byte 1: Transaction ID Low Byte

				// 2. Protocol ID (2 bytes): Always 0x0000 for the Modbus TCP standard.
				// Byte 2: Protocol ID High Byte (0x00)
				// Byte 3: Protocol ID Low Byte (0x00)

				// 3. Length (2 bytes): Number of following bytes remaining in this frame.
				// Calculated as: 3 (Unit ID + Func Code + Byte Count) + (Number of Data Bytes)
				// Byte 4: Length High Byte
				// Byte 5: Length Low Byte

				// 4. Unit ID (1 byte): Echoed exactly as received from the Master.
				// Byte 6: Unit ID


				// --- PDU (Protocol Data Unit) - The "Letter" inside ---

				// 5. Function Code (1 byte): Confirming the action we performed.
				// 0x03 = Read Holding Registers.
				// Byte 7: Function Code

				// 6. Byte Count (1 byte): How many bytes of actual DATA are we sending back?
				// Since each register is 16 bits (2 bytes), Byte Count = (Requested Registers * 2).
				// Byte 8: Byte Count

				// 7. Register Data (Variable Length): The actual values requested by the Master!
				// This is why our data array starts at index 9!
				// Byte 9:  Register 0 High Byte  (e.g., startAddr + 0)
				// Byte 10: Register 0 Low Byte
				// Byte 11: Register 1 High Byte  (e.g., startAddr + 1)
				// Byte 12: Register 1 Low Byte
				// ... and so on for every requested register.

				// Total length: 7 bytes (MBAP) + 2 bytes (Func & Count) + Data Bytes




				// --- PREPARING RESPONSE: Echoing Header Identifiers ---
				// Copy Transaction ID: Ensures the Master matches this response to its request
				tx_buff[0] = rx_buff[0];
				tx_buff[1] = rx_buff[1];
				// Copy Protocol ID: Standard Modbus TCP (always 0x0000)
				tx_buff[2] = rx_buff[2];
				tx_buff[3] = rx_buff[3];
				// Copy Unit ID: Echo back the Device Address (Station ID)
				tx_buff[6] = rx_buff[6];


				// Function Handling Reading

				//------------------------//
				// READ HOLDING REGISTERS //
				//------------------------//

				if (funcCode == 0x03) {

					// Reconstruct 16-bit (Little-Endian - Low Byte, High Byte) values from two 8-bit values Modbus (Big-Endian - High Byte, Low Byte) format: (High_Byte << 8) | Low_Byte
					uint16_t startAddr = (rx_buff[8] << 8) | rx_buff[9]; // From which register?
					uint16_t count = (rx_buff[10] << 8) | rx_buff[11];   // How many registers?

					// --- VALIDATION 5: Modbus Standard Limits ---
					// Modbus standard forbids reading 0 registers or more than 125 at once.
					if (count == 0 || count > 125) {
						send_exception(funcCode, 0x03, transId); // Using the error sending function (send_exception)
						// Error 0x03 - Illegal Data Value
					 }

					 // Protection: check if they are asking for too many registers (Array Size is MB_REG_COUNT)
					 else if ((startAddr + count) > MB_REG_COUNT) {
						 send_exception(funcCode, 0x02, transId); // Error 0x02: Illegal Data Address
					 }



					 else {
						// Build the response
						tx_buff[7] = 0x03; // Function code (Confirmation, response to function 0x03 = Read Holding Registers)
						tx_buff[8] = count * 2; // The number of bytes of data that will be sent (2 bytes per register)


						// --- DATA PACKING: Splitting 16-bit (Little-Endian) registers into 8-bit bytes (Big-Endian format) ---
						// Loop copying data from MB_HoldingRegisters to the transmit buffer (tx_buff)
						// Starting at tx_buff index 9 (7 bytes MBAP + 1 byte FuncCode + 1 byte ByteCount = 9)
						for (int i = 0; i < count; i++) {

						// 1. Extract and store the High Byte
						tx_buff[9 + i*2] = MB_HoldingRegisters[startAddr + i] >> 8; /* >> 8 : Shift bits right 8 positions
						Moves the upper 8 bits to the lower 8 bits position (Extracts the High Byte).
						Example: 0x1234 >> 8 = 0x0012 = 0x12 */

						// 2. Extract and store the Low Byte
						tx_buff[10 + i*2] = MB_HoldingRegisters[startAddr + i] & 0xFF; /* & 0xFF : Bitwise AND operator with mask 0xFF
						0xFF = 255 = 1111 1111 (Zeros out the upper 8 bits and keeps only the lower 8 bits).
						Example: 0x1234 & 0x00FF = 0x0034 = 0x34 */
						}

						// Fill in the length of the entire frame (Data bytes + 3 bytes of header remainder)
						uint16_t packetLen = 3 + (count * 2);
						tx_buff[4] = packetLen >> 8; // Store Length High Byte
						tx_buff[5] = packetLen & 0xFF; // Store Length Low Byte

						// Send the response (6 bytes of MBAP start + packetLen)
						send(MB_SOCKET, tx_buff, 6 + packetLen);
					 }
				}


				//------------------------//
				// WRITE SINGLE REGISTER  //
				//------------------------//

				else if (funcCode == 0x06) {

					// Reconstruct 16-bit (Little-Endian - Low Byte, High Byte) values from two 8-bit values Modbus (Big-Endian - High Byte, Low Byte) format: (High_Byte << 8) | Low_Byte
					uint16_t addr = (rx_buff[8] << 8) | rx_buff[9];  // Where to write?
					uint16_t val  = (rx_buff[10] << 8) | rx_buff[11];// What to write?

					// VALIDATION 6: Check if the address exists (Array Size is MB_REG_COUNT)
					if (addr >= MB_REG_COUNT) {
						send_exception(funcCode, 0x02, transId); // Error 0x02: Illegal Data Address
					}

					else {
						// SAVE TO MEMORY
						MB_HoldingRegisters[addr] = val;

						// Echo - send back exactly what we received (Confirmation)
						tx_buff[4] = 0; // Length High
						tx_buff[5] = 6; // Length Low (UnitID + Func + Addr + Val = 6 bytes)
						tx_buff[7] = 0x06; // Echo function code
						tx_buff[8] = rx_buff[8]; // Echo Address High
						tx_buff[9] = rx_buff[9]; // Echo Address Low
						tx_buff[10] = rx_buff[10]; // Echo Value High
						tx_buff[11] = rx_buff[11]; // Echo Value Low

						send(MB_SOCKET, tx_buff, 12); // Send the 12-byte confirmation frame (Echo) back to the network
						// For function 0x06, the response is always exactly 12 bytes long.
					}
				}

				else {
						// Unknown function
						send_exception(funcCode, 0x01, transId); // Error 0x01: Illegal Function
				}
            }
            break;

        // DISCONNECT: The Master wants to close the connection
        case SOCK_CLOSE_WAIT:
            disconnect(MB_SOCKET);
            break;

        // AFRER DISCONNECT: Re-initialize and wait for new clients
        case SOCK_CLOSED:
            // Reopen the socket to listen for new connections
            socket(MB_SOCKET, Sn_MR_TCP, MB_TCP_PORT, 0);
            listen(MB_SOCKET);
            break;
    }
}


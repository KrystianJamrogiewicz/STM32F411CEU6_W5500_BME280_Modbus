from pyModbusTCP.client import ModbusClient
import time # For time.sleep

# MODBUS TCP/IP Server data
SERVER_IP = "192.168.0.4"
SERVER_PORT = 502

# Displaying a message in the console
print(f"Attempting to connect to {SERVER_IP}:{SERVER_PORT}...")



#-----------------------------------------#
# Establishing a connection to the server #
#-----------------------------------------#
# Creates a "client" object of the "ModbusClient" class (from the pyModbusTCP.client library)
client = ModbusClient(host=SERVER_IP, port=SERVER_PORT, auto_open=True)
# auto_open=True - automatically connects to the server (recommended)

try:

    # Infinite loop for continuous data polling from the server
    while True:

        #-----------------------------------#
        # READING REGISTERS (Function 0x03) #
        #-----------------------------------#
        # Reads 3 registers, starting from address 0 and writes them to the "MB_HoldingRegisters" array.
        MB_HoldingRegisters = client.read_holding_registers(reg_addr=0, reg_nb=3)

        if MB_HoldingRegisters: # If the read was successful (the list is not empty/None).

            temperature = MB_HoldingRegisters[0] / 100.0
            humidity = MB_HoldingRegisters[1] / 100.0
            pressure = MB_HoldingRegisters[2]

            # Display the parsed data in the console.
            print("--- BME280 SENSOR DATA ---")
            print(f"Temperature: {temperature}°C\t"
                f"Humidity: {humidity}%\t"
                f"Pressure: {pressure}hPa")
            print("--------------------------------------------------------------\n")
            time.sleep(1)  # Wait 1s (delay)

        else:
            # Retrieve the internal error code from the pyModbusTCP library.
            error_code = client.last_error

            # =========================================================================
            # pyModbusTCP MAIN ERROR CODES (client.last_error):
            # -------------------------------------------------------------------------
            # 0 = MB_NO_ERR      : No error.
            # 1 = MB_RESOLVE_ERR : Cannot resolve hostname/IP address.
            # 2 = MB_CONNECT_ERR : TCP connection failed (Server off, cable unplugged).
            # 3 = MB_SEND_ERR    : TCP packet send failed (Connection broken during TX).
            # 4 = MB_RECV_ERR    : TCP packet receive failed (Connection broken during RX).
            # 5 = MB_TIMEOUT_ERR : Timeout (TCP connected, but target device didn't reply in time).
            # 6 = MB_FRAME_ERR   : Modbus frame error (Bad packet structure/length).
            # 7 = MB_EXCEPT_ERR  : Modbus Exception (Valid TCP, but logical/hardware fault).
            # 8 = MB_CRC_ERR     : CRC/LRC error (Data corruption in transit).
            # =========================================================================


            # Error Code 7 indicates a "Modbus Exception".
            # This means the physical TCP/IP network is working perfectly, but the target device
            # returned a logical error frame (e.g., sensor hardware failure).
            if error_code == 7:

                # Retrieve the specific Modbus Exception code sent by the target device.
                except_code = client.last_except

                if except_code == 1:
                    print("[WARN] Modbus Exception 0x01: ILLEGAL FUNCTION")
                    print("The target device does not support the requested Modbus command.\n")

                elif except_code == 2:
                    print("[WARN] Modbus Exception 0x02: ILLEGAL DATA ADDRESS")
                    print("You are trying to read a register that does not exist on the target device.\n")

                elif except_code == 3:
                    print("[WARN] Modbus Exception 0x03: ILLEGAL DATA VALUE")
                    print("A value sent in the query field is not an allowable value for the target device.\n")

                elif except_code == 4:
                    print("[CRITICAL] SENSOR FAULT! (Modbus Exception 0x04: TARGET DEVICE FAILURE)")
                    print("Network is OK, but the target device cannot communicate with the sensor.\n")

                elif except_code == 5:
                    print("[INFO] Modbus Exception 0x05: ACKNOWLEDGE")
                    print("The target device accepted the command but needs a long time to process it.\n")

                elif except_code == 6:
                    print("[WARN] Modbus Exception 0x06: TARGET DEVICE BUSY")
                    print("The target device is engaged in processing a long-duration program command. Try again later.\n")

                else:
                    print(f"[WARN] Unknown Modbus Exception returned: 0x0{except_code}\n")

                # Network is alive, so DO NOT close the connection. Just wait 1s and retry.
                # The TCP connection is still alive and stable, so DO NOT close the socket.
                # Just wait 1 second and try requesting data again.
                time.sleep(1)

            # -------------------------------------------------------------------------
            # 2. PHYSICAL NETWORK & TIMEOUT FAULTS (TCP connection broken or MCU frozen)
            # -------------------------------------------------------------------------
            else:
                if error_code == 1:
                    print("[NETWORK ERROR] Cannot resolve IP address (Error 1).\n")
                elif error_code == 2:
                    print("[NETWORK ERROR] TCP connection failed (Error 2). Cable unplugged or target device powered off?\n")
                elif error_code == 3:
                    print("[NETWORK ERROR] TCP packet send failed (Error 3). Connection broken during TX.\n")
                elif error_code == 4:
                    print("[NETWORK ERROR] TCP packet receive failed (Error 4). Connection broken during RX.\n")
                elif error_code == 5:
                    print("[TIMEOUT ERROR] TCP connected, but target device didn't reply (Error 5). MCU frozen?\n")
                elif error_code == 6:
                    print("[NETWORK WARN] Modbus frame error (Error 6). Bad packet structure. EMI noise?\n")
                elif error_code == 8:
                    print("[NETWORK WARN] CRC/LRC error (Error 8). Data corrupted in transit. EMI noise?\n")
                else:
                    print(f"[NETWORK ERROR] Unknown TCP Connection lost (Error Code: {error_code}).\n")

                client.close()  # Closing the connection to the server (can be restarted).
                time.sleep(3)  # Wait longer for hardware to negotiate the connection.


# Handle the user pressing Ctrl+C in the terminal.
except KeyboardInterrupt:
    print("\n[INFO] Polling stopped by the user (Ctrl+C).")

# The 'finally' block ALWAYS executes at the end, ensuring the port is freed.
finally:
    client.close() # Closing the connection to the server.
    print("[INFO] TCP Connection gracefully closed.")
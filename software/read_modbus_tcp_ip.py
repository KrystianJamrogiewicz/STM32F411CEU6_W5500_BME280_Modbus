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

    # Infinite loop for continuous polling
    while True:

        #-----------------------------------#
        # READING REGISTERS (Function 0x03) #
        #-----------------------------------#
        # Reads 3 registers, starting from address 0 and writes them to the "MB_HoldingRegisters" array
        print("\n--- READING ---")
        MB_HoldingRegisters = client.read_holding_registers(reg_addr=0, reg_nb=3)

        if MB_HoldingRegisters:

            temperature = MB_HoldingRegisters[0] / 100.0
            humidity = MB_HoldingRegisters[1] / 100.0
            pressure = MB_HoldingRegisters[2]

            print("--- BME280 SENSOR DATA ---")
            print(f"Temperature : {temperature} °C")
            print(f"Humidity    : {humidity} %")
            print(f"Pressure    : {pressure} hPa")
            print("--------------------------\n")
            time.sleep(1)  # Wait 1s (delay)

        else:
            print("[WARN] Reading error. Check connection or sensor hardware status.\n")
            client.close()
            time.sleep(3)


# Handle the user pressing Ctrl+C in the terminal.
except KeyboardInterrupt:
    print("\n[INFO] Polling stopped by the user (Ctrl+C).")

# The 'finally' block ALWAYS executes at the end, ensuring the port is freed.
finally:
    client.close() # Closing the connection to the server.
    print("[INFO] TCP Connection gracefully closed.")
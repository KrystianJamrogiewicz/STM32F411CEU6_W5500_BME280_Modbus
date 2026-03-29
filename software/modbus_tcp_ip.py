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



#-----------------------------------#
# READING REGISTERS (Function 0x03) #
#-----------------------------------#
# Reads 4 registers, starting from address 0 and writes them to the "MB_HoldingRegisters" array
print("\n--- READING ---")
MB_HoldingRegisters = client.read_holding_registers(reg_addr=0, reg_nb=4)

if MB_HoldingRegisters:
    print(f"Success! MB_HoldingRegisters values read")
    print(f"{MB_HoldingRegisters}")
else:
    print("Reading error. Check connection")

time.sleep(1) # Wait 1s (delay)



#-----------------------------------#
# WRITING REGISTERS (Function 0x06) #
#-----------------------------------#
# Zmieńmy wartość w Rejestrze [2] (domyślnie było tam 300) na nową wartość: 999
print("\n--- WRITING ---")
nev_value = 9879
print(f"I'm trying to save a value: {nev_value} to MB_HoldingRegister [2]...")

successful_write = client.write_single_register(reg_addr=2, reg_value=nev_value)
# After a successful write, the "client.write_single_register" function returns TRUE

if successful_write:
    print("Write was successful!")
else:
    print("Write error!")

time.sleep(1)



#------------------------------------------------------------------------------#
# READING REGISTERS (Function 0x03) (Checking if the new value has been saved) #
#------------------------------------------------------------------------------#
print("\n--- READ AFTER WRITE ---")
MB_HoldingRegisters_after_write = client.read_holding_registers(reg_addr=0, reg_nb=4)

if MB_HoldingRegisters_after_write:
    print(f"New MB_HoldingRegisters values read: {MB_HoldingRegisters_after_write}")
else:
    print("Reading error. Check connection")



# Closing the connection to the server
client.close()
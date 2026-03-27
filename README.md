# Simple Vehicle Communication Project 

This project simulates a simplified in-vehicle communication architecture using three independent modules interacting over I2C, LIN, CAN, and SPI.

---

## Architecture Overview

The system consists of three modules:

### 1. SimpleTemperatureMonitor
- Reads temperature from a sensor via I2C
- Stores recent samples to compute an average temperature
- Responds to LIN master requests:
  - Current temperature
  - Average temperature

---

### 2. BatteryTemperatureVehicleModule
- Acts as LIN master
- Periodically requests:
  - Current temperature
  - Average temperature
- Stores received values internally
- Responds to CAN RTR (Remote Transmission Request) frames with:
  - Current temperature
  - Average temperature
  - Current timestamp (seconds)

---

### 3. DataLoggingVehicleModule
- Sends CAN RTR requests to the Battery module
- Receives CAN DATA frames containing:
  - Current temperature
  - Average temperature
  - Timestamp
- Buffers received samples
- Writes data to SPI flash (CS1) in pages

---

## Communication Flow
```
I2C → LIN → CAN → SPI

SimpleTemperatureMonitor
   → (LIN)
BatteryTemperatureVehicleModule
   → (CAN)
DataLoggingVehicleModule
   → (SPI Flash)
```
---

## Getting Started

### Build the project

```
</> Bash
mkdir build
cd build
cmake ..
make
```

### Running the System

Run each module in a separate terminal:

```
</> Bash
./SimpleSensorMonitor
./BatteryTemperatureVehicleModule
./DataLoggingVehicleModule
```

### Dependencies
- CMake
- C++17 compiler

## Testing

System behavior was validated by:

- Verifying I2C temperature reads
- Observing LIN communication between sensor and battery module
- Confirming CAN request/response behavior
- Validating SPI flash page writes

Example output from DataLoggingVehicleModule:
```
Average temperature data: 59
Current temperature data: 60
Current time data: 1774526781
Wrote SPI flash page: 0
```
## Project Instructions
### SimpleTemperatureMonitor
### Configure the I2C hardware controller
The i2c_write_config function in I2C.h is used to set the I2C configuration register.
The I2C hardware controller should be set to match the given requirements:
- The I2C hardware address is 0xFF000030
- 100 KHz clock
- I2C host
- Bus Idle Low
- Clock Rising Edge.

```
#define I2C_HW_ADDR 0xFF000030

    // Configure the I2C controller
    uint32_t i2c_config =
        I2C_CLK_100KHZ |
        I2C_HOST |
        I2C_IDLE_LOW |
        I2C_CLK_RISING_EDGE;

    // Apply configuration
    i2c_write_config(I2C_HW_ADDR, i2c_config);
````

### Configure the LIN hardware controller
The lin_write_config function in LIN.h is used to set the LIN configuration register.
The LIN hardware controller should be set to match the given requirements:
9600 bps baud rate
1 Start bit
1 Stop bit
8 Data bits
No Parity bits
No flow control
LIN mode

```
#define LIN_HW_ADDR 0xFF000040

    // Configure the LIN controller
    uint32_t lin_config =
        LIN_BAUD_RATE_9600 |
        LIN_START_BITS_1 |
        LIN_STOP_BITS_1 |
        LIN_DATA_BITS_8 |
        LIN_PARITY_NONE |
        LIN_NO_FLOW_CONTROL |
        LIN_MODE_FOLLOWER;

    // Apply configuration
    lin_write_config(LIN_HW_ADDR, lin_config);
```

### Implement expected polling and response
- Poll the current temperature every 1 second
```
#define SLEEP_DURATION_MS 1000
while(true) {
    ...
    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_DURATION_MS));
}
```
This also shows that the loop runs every 1000 ms (i.e., once per second).

- Read 8-bit temperature sensor data at i2c address 0x20
```
#define ADC_ADDR 0x20
#define ADC_REG  0x00

uint8_t temp_val = 0;
uint8_t reg = ADC_REG;

// Select ADC register
i2c_write_data(ADC_ADDR, &reg, 1);

// Read 8-bit temperature sensor data
i2c_read_data(ADC_ADDR, &temp_val, 1);
```

- Cache the latest temperature sensor data to send over LIN
```
g_current_val = temp_val;
// Read 8-bit temperature sensor data
i2c_read_data(ADC_ADDR, &temp_val, 1);

// Cache latest temperature sensor data
g_current_val = temp_val;
```
This line stores the last read temperature in g_current_val. Then this value can be used when a LIN request is received.

Respond to LIN average temperature and LIN current temperature IDs in lin_rx_isr function
- Send the average temperature over LIN if the interrupt comes from LIN_AVG_TEMP_SENSOR_ID
```
if(id == LIN_AVG_TEMP_SENSOR_ID) {
    uint8_t avg_temp = calculate_average();
    printf("LIN avg temp request received, sending: %d\n", avg_temp);
    lin_write_response_data(LIN_AVG_TEMP_SENSOR_ID, &avg_temp, 1);
}
```
This part:
It checks whether the received interrupt ID is LIN_AVG_TEMP_SENSOR_ID,
calculates the average using calculate_average(),
and sends it over LIN.

- Send the latest temperature over LIN if the interrupt comes from LIN_CURRENT_TEMP_SENSOR_ID
```
else if(id == LIN_CURRENT_TEMP_SENSOR_ID) {
    printf("LIN current temp request received, sending: %d\n", g_current_val);
    lin_write_response_data(LIN_CURRENT_TEMP_SENSOR_ID, &g_current_val, 1);
}
```
This part:
If the received ID is LIN_CURRENT_TEMP_SENSOR_ID,
it sends the latest temperature stored in the cache (g_current_val) over LIN.

- Clear the ISR
```
// Clear the lin interrupt before exit isr:
lin_clear_rx_frame_interrupt();
```
This also clears the interrupt flag at the end of the ISR.


### BatteryTemperatureVehicleModule
#### Configure the LIN main hardware controller
The lin_write_config function in LIN.h is used to set the LIN configuration register.
The LIN hardware controller should be set to match the given requirements:
- 9600 bps baud rate
- 1 Start bit
- 1 Stop bit
- 8 Data bits
- No Parity bits
- No flow control
- LIN master mode
```
    // Configure the LIN controller and CAN controller here:
    uint32_t lin_config =
        LIN_BAUD_RATE_9600 |
        LIN_START_BITS_1 |
        LIN_STOP_BITS_1 |
        LIN_DATA_BITS_8 |
        LIN_PARITY_NONE |
        LIN_NO_FLOW_CONTROL |
        LIN_MODE_LEADER;
    
    lin_write_config(0xFF000040, lin_config);
```
#### Configure the CAN hardware controller
The CAN hardware controller should be set to match the given requirements:
- Baud Rate 100 kbps
- 11-bit format
```
    // Configure the CAN controller
    uint32_t can_config =
        CAN_BAUD_RATE_100K |
        CAN_FORMAT_11BIT;

    can_write_config(CAN_HARDWARE_REGISTER, can_config);
```
#### Implement the CAN ISR to respond to CAN requests
Respond to CAN Request frames using the can_send_new_packet function for the following CAN IDs defined in CAN.h.
- CAN_AVG_TEMPERATURE_11_SENSOR_ID= 0x14f
  - Setup a CAN DATA frame to send average temperature
- CAN_CURRENT_TEMP_11_SENSOR_ID = 0x15f
  - Setup a CAN DATA frame to send the current temperature
- CAN_TIME_11_SENSOR_ID = 0x18f
  - Setup a CAN DATA frame to send the current time
```
void can_new_packet_isr(uint32_t id, CAN_FRAME_TYPES type, uint8_t *data, uint8_t len) {
       
    if(id == CAN_AVG_TEMPERATURE_11_SENSOR_ID && type == CAN_RTR_FRAME) {
        if (!g_avg_ready) return;

        uint8_t avg_data[1] = { g_avg_temp };

        // Setup CAN DATA frame to send the avg temperature data
        can_send_new_packet(CAN_AVG_TEMPERATURE_11_SENSOR_ID, CAN_DATA_FRAME, avg_data, 1);

    } else if (id == CAN_CURRENT_TEMP_11_SENSOR_ID && type == CAN_RTR_FRAME) {
        if (!g_current_ready) return;

        uint8_t current_data[1] = { g_current_temp };

        // Setup CAN DATA frame to send the current temperature data
        can_send_new_packet(CAN_CURRENT_TEMP_11_SENSOR_ID, CAN_DATA_FRAME, current_data, 1);

    } else if (id == CAN_TIME_11_SENSOR_ID) {
        uint8_t time_data[4];
        time_data[0] = static_cast<uint8_t>(g_timestamp & 0xFF);
        time_data[1] = static_cast<uint8_t>((g_timestamp >> 8) & 0xFF);
        time_data[2] = static_cast<uint8_t>((g_timestamp >> 16) & 0xFF);
        time_data[3] = static_cast<uint8_t>((g_timestamp >> 24) & 0xFF);

        // Setup CAN DATA frame to send the current time data
        can_send_new_packet(CAN_TIME_11_SENSOR_ID, CAN_DATA_FRAME, time_data, 4);
    }

    // Clear the can interrupt before exit isr:
    can_clear_rx_packet_interrupt();
}
```
For each supported CAN request:

If the request is for average temperature, send g_avg_temp
If the request is for current temperature, send g_current_temp
If the request is for time, send g_timestamp as 4 bytes
Clear the CAN interrupt before leaving the ISR

#### Clear CAN and LIN ISR
```
```
#### Implement requests for the current and average temperature
```
```

### DataLoggingVehicleModule
- Configured CAN and SPI controllers
- Sent periodic CAN RTR requests
- Implemented CAN ISR to receive data frames
- Buffered received samples (temperature + timestamp)
- Wrote buffered data to SPI flash in page-sized chunks

## Built With

* C++17 - Core programming language
* CMake - Build system
* Custom hardware simulation layer (I2C, LIN, CAN, SPI)

## License
This project is part of a Udacity course and is intended for educational purposes only.

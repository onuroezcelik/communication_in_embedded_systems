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
- If the request is for average temperature, send g_avg_temp
- If the request is for current temperature, send g_current_temp
- If the request is for time, send g_timestamp as 4 bytes
- Clear the CAN interrupt before leaving the ISR

#### Clear CAN and LIN ISR
Clear the CAN interrupt before exiting the CAN ISR.
Clear the LIN interrupt before exiting the LIN ISR
```
can_clear_rx_packet_interrupt();
lin_clear_frame_resp_interrupt();
```
#### Implement requests for the current and average temperature
Periodically request the current temperature and average temperature from any support LIN peripherals on the bus by writing the frame headers to the LIN bus
- Should request the data every 500 milliseconds
- Save a timestamp for each request
```
    while(true) {
       
        // Request average temperature from the temperature module
        g_timestamp = static_cast<uint32_t>(TIME_NOW_S());
        lin_write_frame_header(LIN_AVG_TEMP_SENSOR_ID);

        // Request current temperature from the temperature module
        g_timestamp = static_cast<uint32_t>(TIME_NOW_S());
        lin_write_frame_header(LIN_CURRENT_TEMP_SENSOR_ID);

        // for every 500 ms
        SLEEP_MS(500);

    }
```

### DataLoggingVehicleModule
#### Configure the CAN and SPI hardware controllers
The CAN hardware controller should be set to match the given requirements:
- Baud Rate 100 kbps
- 11 bit format
The SPI hardware controller should be set to match the given requirements:
- 1 MHz clock rate
```
    // Configure the SPI and CAN controllers;
    
    uint32_t spi_config =
        SPI_CLK_1MHZ |
        SPI_CS_1 ;

    spi_write_config(SPI_HARDWARE_REGISTER, spi_config);

    uint32_t can_config =
        CAN_BAUD_RATE_100K |
        CAN_FORMAT_11BIT;

    can_write_config(CAN_HARDWARE_REGISTER, can_config);
```
#### Implement the CAN ISR to temporarily store data until an entire page can be saved
The CAN ISR should be setup similar to the Battery Temperature Vehicle Module, but instead of sending CAN data frames, it should store the appropriate temperature and time data until an entire page can be saved/written to SPI.

The CAN ISR should filter for the data frames with the CAN_AVG_TEMPERATURE_11_SENSOR_ID, CAN_CURRENT_TEMP_11_SENSOR_ID, and CAN_TIME_11_SENSOR_ID IDs.
```
void can_packet_isr(uint32_t id, CAN_FRAME_TYPES type, uint8_t *data, uint8_t len) {
    // we can not save the page in the isr, too slow
    // so you need to accomodate for this in the main loop

    // Do something with the CAN packet
    if (type == CAN_DATA_FRAME) {
        if (id == CAN_AVG_TEMPERATURE_11_SENSOR_ID && len >= 1) {
            g_pending_avg_temp = data[0];
            g_has_avg_temp = true;

            printf("Average temperature data: %d\n", data[0]);
            try_commit_sample();

        } else if (id == CAN_CURRENT_TEMP_11_SENSOR_ID && len >= 1) {
            g_pending_current_temp = data[0];
            g_has_current_temp = true;

            printf("Current temperature data: %d\n", data[0]);
            try_commit_sample();

        } else if (id == CAN_TIME_11_SENSOR_ID && len >= 4) {
            g_pending_time =
                static_cast<unsigned int>(data[0]) |
                (static_cast<unsigned int>(data[1]) << 8) |
                (static_cast<unsigned int>(data[2]) << 16) |
                (static_cast<unsigned int>(data[3]) << 24);

            g_has_time = true;

            printf("Current time data: %u\n", g_pending_time);
            try_commit_sample();

        } else {
            printf("Unknown CAN packet received\n");
        }
    } else {
        printf("Unknown CAN packet received\n");
    }

    // Clear the can interrupt before exit isr:
    can_clear_rx_packet_interrupt();
}
```
The ISR filters incoming CAN frames by:
- Checking type == CAN_DATA_FRAME
- Matching the IDs:
  - CAN_AVG_TEMPERATURE_11_SENSOR_ID
  - CAN_CURRENT_TEMP_11_SENSOR_ID
  - CAN_TIME_11_SENSOR_ID
Instead of sending CAN responses (as in the previous module), the ISR:
- Stores the received values in temporary variables:
  - g_pending_avg_temp
  - g_pending_current_temp
  - g_pending_time
- Sets corresponding flags to indicate valid data
The function try_commit_sample() is then called to:
- Combine avg temperature, current temperature, and time into a single data point
- Store it into a page buffer (g_page_buffer)
This mechanism ensures that:
Data is accumulated until a full SPI flash page is ready to be written, instead of writing inside the ISR

The function for “until an entire page can be saved/written to SPI”
```
static void try_commit_sample() {
    if (g_has_avg_temp && g_has_current_temp && g_has_time) {
        if (g_page_buffer_count < SPI_FLASH_DATA_PTS_PER_PAGE) {
            g_page_buffer[g_page_buffer_count].avg_temp = g_pending_avg_temp;
            g_page_buffer[g_page_buffer_count].current_temp = g_pending_current_temp;
            g_page_buffer[g_page_buffer_count].time = g_pending_time;
            g_page_buffer_count++;
        }

        g_has_avg_temp = false;
        g_has_current_temp = false;
        g_has_time = false;
    }
}
```

#### Write temperature data and timestamps to SPI Flash
After regularly polling
Use the pre-processor definition SPI_FLASH_DATA_PTS_PER_PAGE to identify when you are ready to write a page in the SPI flash, which is whenever a page is full.
- The SPI Flash is on CS1, not CS0
- The SPI Flash has a Page size of 64 bytes defined as SPI_FLASH_PAGE_SIZE in SPI.h
- The SPI Flash has a total size of 4096 bytes defined as SPI_FLASH_SZ in SPI.h
- A SPI Flash must be written to in pages, not bytes
- A SPI Flash will return an error if you attemp to write to a page that is not erased
- The SPI Flash expects the MOSI data to look like the following
| Flash Command | Page Number | Data 1 | Data 2 | ... | Data 64 |
- The available flash commands area (SPI.h):
  - SPI_FLASH_CMD_WRITE
  - SPI_FLASH_CMD_READ
  - SPI_FLASH_CMD_ERASE
 
Detect when a full page is ready
```
if (g_page_buffer_count >= SPI_FLASH_DATA_PTS_PER_PAGE) {
    write_page_to_spi_flash();
}
```

Write a full page to SPI flash
```
static void write_page_to_spi_flash() {
    if (g_page_buffer_count < SPI_FLASH_DATA_PTS_PER_PAGE) {
        return;
    }

    uint8_t erase_buf[2];
    erase_buf[0] = SPI_FLASH_CMD_ERASE;
    erase_buf[1] = g_current_flash_page;

    SPI_xmit_t erase_xmit;
    erase_xmit.cmd = SPI_FLASH_CMD_ERASE;
    erase_xmit.len = 2;
    erase_xmit.data = erase_buf;

    spi_write_data(&erase_xmit, SPI_CS_1);

    uint8_t write_buf[2 + SPI_FLASH_PAGE_SIZE];
    write_buf[0] = SPI_FLASH_CMD_WRITE;
    write_buf[1] = g_current_flash_page;

    std::memcpy(&write_buf[2], g_page_buffer, SPI_FLASH_PAGE_SIZE);

    SPI_xmit_t write_xmit;
    write_xmit.cmd = SPI_FLASH_CMD_WRITE;
    write_xmit.len = 2 + SPI_FLASH_PAGE_SIZE;
    write_xmit.data = write_buf;

    spi_write_data(&write_xmit, SPI_CS_1);

    g_current_flash_page++;
    g_page_buffer_count = 0;
}
```

## Built With

* C++17 - Core programming language
* CMake - Build system
* Custom hardware simulation layer (I2C, LIN, CAN, SPI)

## License
This project is part of a Udacity course and is intended for educational purposes only.

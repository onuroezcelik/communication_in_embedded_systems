#include "LIN.h"
#include "CAN.h"
#include <cstdio>
#include <cstdint>
#include <chrono>
#include <thread>

// 1. Configure the LIN controller with Baud 9600, 1 Start bit, 1 Stop bit, 8 Data bits, No flow control, Master mode
// 2. Configure the CAN controller with Baud 100K, 11 bit format
// 3. Every 500ms get the latest avg temperature from the temperature sensor module on the LIN bus
// 4. Every 500ms get the latest current temperature from the temperature sensor module on the LIN bus
// 5. Save the current timestamp for each request on the LIN bus
// -- 5.5 Use TIME_NOW_S() to get the current time in seconds
// 6. Respond the CAN RTR frames with DATA frames


#define SLEEP_MS(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))
#define TIME_NOW_S() std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()

// REMEMBER:
// 1. CAN RTR frames are request for data from another CAN device
// 2. CAN DATA frames are responses to a CAN RTR frames
// 3. The CAN IDs we are supporting are:
//    3.1 CAN_AVG_TEMPERATURE_11_SENSOR_ID
//    3.2 CAN_CURRENT_TEMP_11_SENSOR_ID
//    3.3 CAN_TIME_11_SENSOR_ID

static uint8_t g_avg_temp = 0;
static uint8_t g_current_temp = 0;
static uint32_t g_timestamp = 0;

static bool g_avg_ready = false;
static bool g_current_ready = false;

void can_new_packet_isr(uint32_t id, CAN_FRAME_TYPES type, uint8_t *data, uint8_t len) {
       
    if(id == CAN_AVG_TEMPERATURE_11_SENSOR_ID && type == CAN_RTR_FRAME) {
        printf("DEBUG: g_avg_temp=%d\n", g_avg_temp);
        if (!g_avg_ready) return;

        uint8_t avg_data[1] = { g_avg_temp };

        printf("Average temperature data: %d\n", g_avg_temp);
        // Setup CAN DATA frame to send the avg temperature data
        can_send_new_packet(CAN_AVG_TEMPERATURE_11_SENSOR_ID, CAN_DATA_FRAME, avg_data, 1);

    } else if (id == CAN_CURRENT_TEMP_11_SENSOR_ID && type == CAN_RTR_FRAME) {
        if (!g_current_ready) return;
        uint8_t current_data[1] = { g_current_temp };

        printf("Current temperature data: %d\n", g_current_temp);

        // Setup CAN DATA frame to send the current temperature data
        can_send_new_packet(CAN_CURRENT_TEMP_11_SENSOR_ID, CAN_DATA_FRAME, current_data, 1);

    } else if (id == CAN_TIME_11_SENSOR_ID) {
        uint8_t time_data[4];
        time_data[0] = static_cast<uint8_t>(g_timestamp & 0xFF);
        time_data[1] = static_cast<uint8_t>((g_timestamp >> 8) & 0xFF);
        time_data[2] = static_cast<uint8_t>((g_timestamp >> 16) & 0xFF);
        time_data[3] = static_cast<uint8_t>((g_timestamp >> 24) & 0xFF);

        printf("Current timestamp: %u\n", g_timestamp);

        // Setup CAN DATA frame to send the current time data
        can_send_new_packet(CAN_TIME_11_SENSOR_ID, CAN_DATA_FRAME, time_data, 4);
    }
    // Clear the can interrupt before exit isr:
    can_clear_rx_packet_interrupt();

}

void lin_rx_isr(uint8_t id, uint8_t *data, uint8_t len) {
    if(id == LIN_AVG_TEMP_SENSOR_ID && len >= 1) {
        g_avg_temp = data[0];
        g_avg_ready = true;

        printf("Average Temperature data: %d\n", g_avg_temp);
    } else if(id == LIN_CURRENT_TEMP_SENSOR_ID && len >= 1) {
        g_current_temp = data[0];
        g_current_ready = true;

        printf("Current Temperature data: %d\n", g_current_temp);
    }

    // Clear the lin interrupt before exit isr:
    lin_clear_frame_resp_interrupt();
}


int main(int argc, char **argv) {
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

    // Configure the CAN controller
    uint32_t can_config =
        CAN_BAUD_RATE_100K |
        CAN_FORMAT_11BIT;

    can_write_config(CAN_HARDWARE_REGISTER, can_config);

    can_add_filter(0, 0x7FF, CAN_AVG_TEMPERATURE_11_SENSOR_ID);
    can_add_filter(1, 0x7FF, CAN_CURRENT_TEMP_11_SENSOR_ID);
    can_add_filter(2, 0x7FF, CAN_TIME_11_SENSOR_ID);

    //Add the LIN frame response ISR
    // This ISR fires when a slave node responds to a master node request, In this case the temperature sensor module
    lin_add_frame_resp_interrupt(lin_rx_isr);

    // Add the CAN RX ISR
    can_add_rx_packet_interrupt(can_new_packet_isr);

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
    return 0;
}

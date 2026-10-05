#pragma once

#include <Arduino.h>
#include <IPAddress.h>

#ifndef ENABLE_TELEMETRY
#define ENABLE_TELEMETRY 0
#endif

#define FIRMWARE_VERSION "0.2.0"

#define SERIAL_BAUD 115200
#define TELEMETRY_BAUD 115200
#define TELEMETRY_SERIAL Serial4
#define TELEMETRY_SEND_INTERVAL_MS (10UL * 60UL * 1000UL)
#define TELEMETRY_LINES_TO_SEND 5
#define TELEMETRY_LINE_BUFFER_SIZE UDP_BUFFER_SIZE
#define TELEMETRY_CONNECT_COMMAND "^"
#define TELEMETRY_SHUTDOWN_COMMAND "0"
#define TELEMETRY_CONNECT_POLL_MS 100
#define TELEMETRY_SESSION_TIMEOUT_MS (60UL * 1000UL)
#define TELEMETRY_MAX_SEND_RETRIES 3

#define LED_PIN 13
#define LED_NORMAL_ON_MS 100   // brief blink every 2 s: running, SD logging OK
#define LED_NORMAL_OFF_MS 1900
#define LED_ERROR_ON_MS 100    // fast flash: SD fault or other error
#define LED_ERROR_OFF_MS 100
#define MIFI_WAKE_PIN 34
#define MIFI_POWER_PIN 35
#define GPS_POWER_PIN 33
#define TELEMETRY_POWER_PIN 36
#define SD_CHIP_SELECT BUILTIN_SDCARD

#define LOG_ROTATE_HOURS 4
#define LOG_FLUSH_INTERVAL_MS 1000
#define SD_FAULT_WARN_INTERVAL_MS 10000

#define BATTERY_SAMPLE_INTERVAL_MS 1000
#define BATTERY_REPORT_INTERVAL_MS 10000
#define BATTERY_LOW_VOLTAGE_THRESHOLD 23.0f // volts; tune for the installed battery pack
#define BATTERY_LOW_VOLTAGE_REPORTS 3        // consecutive low reports before shutdown
#define BATTERY_MAX_VALID_VOLTAGE 40.0f     // volts; readings above this are I2C faults
#define BATTERY_RETRY_MS (60UL * 1000UL)    // retry interval when the INA260 is missing

#define UDP_BUFFER_SIZE 256
#define SERIAL_COMMAND_BUFFER_SIZE 256

#define LANDER_LOCAL_PORT 8002
#define LANDER_REMOTE_PORT 8000
#define ETHERNET_BEGIN_RETRY_MS 2000
#define ETHERNET_LINK_TIMEOUT_MS 30000
#define LANDER_STATUS_RETRY_MS 1000
#define LANDER_CONNECT_TIMEOUT_MS 30000
#define LANDER_LINK_LOST_MS (60UL * 1000UL) // no packets for this long -> resume probing
#define LANDER_MAX_PACKETS_PER_UPDATE 8

// Network settings for the surface controller and lander. The surface uses
// the Teensy's factory MAC address.
extern IPAddress SURFACE_IP;
extern IPAddress SURFACE_NETMASK;
extern IPAddress SURFACE_GATEWAY;
extern IPAddress LANDER_IP;

#define TIME_HEADER "T"
#define LANDER_POWER_OFF_COMMAND "OFF"
#define LANDER_OFF_ACK "ACK,OFF"
#define LANDER_OFF_DONE "DONE,OFF"
#define LANDER_OFF_RETRY_MS (30UL * 1000UL)
#define LANDER_OFF_DONE_TIMEOUT_MS (5UL * 60UL * 1000UL) // resend OFF if DONE never arrives; tune to lander power-down time

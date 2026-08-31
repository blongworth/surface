# Surface Controller Firmware

PlatformIO / Arduino firmware for a Teensy 4.1 surface controller. The controller communicates with a lander over UDP Ethernet, logs communication to the onboard SD card, and provides a USB serial console for command entry and monitoring.

Optional telemetry support can be enabled at compile time for a serial-connected radio/GSM/satellite module.

## Hardware target

- Teensy 4.1
- Native Ethernet
- Built-in SD card slot
- USB serial console
- Optional serial telemetry module on `Serial4`

## Main features

- Sends commands from the surface controller to the lander over UDP
- Receives lander replies/data over UDP
- Prints all communication to the USB serial console
- Logs lander data and system events to rotated SD card files
- Relays unknown serial console commands directly to the lander
- Supports MTP SD card access when log files are closed
- Optional telemetry mode sends recent lander data lines at a configured interval
- Monitors battery voltage/current via an I2C INA260 and logs 10s averages
- Sends an OFF command to the lander if the averaged battery voltage drops too low

## Project layout

```text
include/
  Battery.h      INA260 battery voltage/current monitor
  Config.h       Build/runtime configuration constants
  Console.h      USB serial command console
  LanderUdp.h    UDP transport for lander communication
  Logger.h       SD card logging and log rotation
  Message.h      Message direction labels
  Telemetry.h    Optional telemetry module interface

src/
  main.cpp       Application setup/loop and command routing
  Battery.cpp
  Config.cpp     Network address definitions
  Console.cpp
  LanderUdp.cpp
  Logger.cpp
  Message.cpp
  Telemetry.cpp

lib/GSMCom/      Existing framed serial helper used by telemetry
```

## Configuration

Most configuration is in `include/Config.h`.

Network addresses are defined in `src/Config.cpp`:

```cpp
byte SURFACE_MAC[] = {0x04, 0xE9, 0xE5, 0x0B, 0xFC, 0xD1};
IPAddress SURFACE_IP(111, 111, 111, 222);
IPAddress LANDER_IP(111, 111, 111, 111);
```

UDP ports are configured in `include/Config.h`:

```cpp
#define LANDER_LOCAL_PORT 8002
#define LANDER_REMOTE_PORT 8000
```

## Building

Default build, telemetry disabled:

```sh
pio run -e teensy41
```

Telemetry-enabled build:

```sh
pio run -e teensy41_telemetry
```

Upload default build:

```sh
pio run -e teensy41 -t upload
```

Open serial monitor:

```sh
pio device monitor -b 115200
```

## Serial console commands

The console is line based. Type a command and press Enter.

Local commands:

```text
help        show local command help
rotate-log  close current log files and open new files
start-log   alias for rotate-log; useful after MTP access
close-log   close current log files; enables MTP servicing
status      send a lander status request
time-sync   send current surface time to the lander
mtp-reset   send an MTP device reset event
```

Any other line is relayed directly to the lander over UDP.

For example, typing:

```text
?
```

sends `?\r` to the lander.

## Logging

Logs are rotated every `LOG_ROTATE_HOURS` hours. Rotation and flush intervals are configured in `include/Config.h`:

```cpp
#define LOG_ROTATE_HOURS 4
#define LOG_FLUSH_INTERVAL_MS 1000
```

Each rotation creates two files on the SD card.

### Lander data log

```text
surface_YYYY-MM-DD-HH-MM_lander.log
```

Contains only data received from the lander:

```text
# surface-lander-log-v1
# fields: iso8601 payload
2026-08-11T12:00:00Z DATA,12.3,45.6,78.9
```

Format:

```text
ISO8601 payload
```

### Event log

```text
surface_YYYY-MM-DD-HH-MM_events.log
```

Contains commands, transmitted messages, telemetry events, and system messages:

```text
# surface-event-log-v1
# fields: iso8601 direction payload
2026-08-11T12:00:00Z SYSTEM startup complete
2026-08-11T12:00:01Z TX_LANDER ?
2026-08-11T12:00:02Z RX_CONSOLE status
```

Format:

```text
ISO8601 direction payload
```

## MTP SD card access

The default build includes MTP support using:

```ini
-D USB_MTPDISK_SERIAL
```

MTP is serviced only when the log files are closed. To browse the SD card over USB MTP, use the serial console command:

```text
close-log
```

After MTP access, restart logging with:

```text
start-log
```

## Optional telemetry

Telemetry is disabled by default. Enable it by building the `teensy41_telemetry` environment.

```sh
pio run -e teensy41_telemetry
```

Telemetry configuration is in `include/Config.h`:

```cpp
#define TELEMETRY_SERIAL Serial4
#define TELEMETRY_BAUD 115200
#define TELEMETRY_SEND_INTERVAL_MS (10UL * 60UL * 1000UL)
#define TELEMETRY_LINES_TO_SEND 5
#define TELEMETRY_LINE_BUFFER_SIZE UDP_BUFFER_SIZE
```

When enabled, the firmware stores the most recent lander data lines in a circular buffer. At each telemetry interval, it starts a telemetry session and sends the latest lines in chronological order as framed serial commands:

```text
DATA:<lander-line>
```

The telemetry module currently mimics the original modem procedure:

- initializes modem/GPS/MiFi power-control pins low
- clears the telemetry serial port before a session
- repeatedly sends `TELEMETRY_CONNECT_COMMAND`, currently `^`
- treats response `1` as connected and `0` as not connected
- treats response `Da` as data-send success
- retries failed data sends up to `TELEMETRY_MAX_SEND_RETRIES`
- sends `TELEMETRY_SHUTDOWN_COMMAND`, currently `0`, when finished or timed out

Telemetry pin configuration:

```cpp
#define MIFI_WAKE_PIN 34
#define MIFI_POWER_PIN 35
#define GPS_POWER_PIN 33
#define TELEMETRY_POWER_PIN 36
```

## Battery monitor

An INA260 on the default I2C bus (`Wire`) is sampled at 1 Hz. Every
`BATTERY_REPORT_INTERVAL_MS` the accumulated readings are averaged, combined
with a fresh reading from the Teensy's internal temperature sensor, logged to
the event log as a system message, and checked against a low-voltage
threshold:

```cpp
#define BATTERY_SAMPLE_INTERVAL_MS 1000
#define BATTERY_REPORT_INTERVAL_MS 10000
#define BATTERY_LOW_VOLTAGE_THRESHOLD 11.0f // volts; tune for the installed battery pack
```

If the 10s averaged voltage drops below `BATTERY_LOW_VOLTAGE_THRESHOLD`, the
controller starts a shutdown handshake with the lander:

```cpp
#define LANDER_OFF_ACK "ACK,OFF"
#define LANDER_OFF_DONE "DONE,OFF"
#define LANDER_OFF_RETRY_MS (30UL * 1000UL)
```

1. sends `LANDER_POWER_OFF_COMMAND` (`OFF` by default) to the lander
2. resends it every `LANDER_OFF_RETRY_MS` until the lander replies `ACK,OFF`
3. waits for the lander to reply `DONE,OFF`
4. logs the low-voltage shutdown and closes the SD log files

If the INA260 is not detected at startup, battery monitoring is disabled for
that run and a message is printed to the serial console.

## Communication behavior

At startup the controller:

1. initializes serial, MTP, RTC, SD logging, Ethernet, optional telemetry, and the battery monitor
2. opens SD log files
3. sends the current time to the lander
4. requests lander status
5. prints console help

During operation:

- UDP messages received from the lander are printed and written to the lander log
- UDP messages sent to the lander are printed and written to the event log
- serial console input is logged and either handled locally or relayed to the lander
- optional telemetry periodically sends recent lander data

## Notes

- Lander UDP messages are expected to be terminated with carriage return (`\r`).
- The serial console uses 115200 baud by default.
- The RTC should be set for meaningful ISO-8601 timestamps.

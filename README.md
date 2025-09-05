# Arduino Nano Data Logger with PT1000 Temperature Sensor

A precision temperature data logger using Arduino Nano with DEEK-ROBOT Data Logging Shield (Product ID: 8105) and PT1000 RTD sensor via MAX31865 ADC.

## Project Overview

This project creates a standalone data logger that:

- Records temperature data from a 4-wire PT1000 RTD sensor
- Logs data with precise timestamps to SD card in CSV format
- Provides visual feedback via LED indicator
- Supports time synchronization via USB/Serial connection
- Operates standalone without USB connection

## Hardware Components

### Main Components

- **Arduino Nano** (ATmega328P based)
- **DEEK-ROBOT Nano Data Logging Shield** (Product ID: 8105)
  - Includes DS1307 Real-Time Clock (RTC)
  - MicroSD card slot
  - Prototyping area
  - Power management
- **MAX31865 RTD-to-Digital Converter** breakout board
- **4-wire PT1000 RTD temperature sensor**
- **4.3kΩ reference resistor** (usually included with MAX31865)

### DEEK-ROBOT Shield Features

The DEEK-ROBOT Nano Data Logging Shield (8105) provides:

- **DS1307 RTC** with battery backup
- **MicroSD card slot** for data storage
- **Prototyping area** for additional components
- **Power regulation** for stable operation
- **Compact form factor** that stacks on Arduino Nano

## Pin Connections

### MAX31865 Connections (Hardware SPI)

```
MAX31865    →    Arduino Nano
VIN         →    3.3V or 5V
GND         →    GND
CS          →    Pin 8 (configurable)
SDI (MOSI)  →    Pin 11 (D11)
SDO (MISO)  →    Pin 12 (D12)
CLK (SCK)   →    Pin 13 (D13)
```

### PT1000 4-Wire Connection to MAX31865

```
PT1000      →    MAX31865
RTD+        →    Force+ (F+)
RTD+        →    Sense+ (S+) [wire link on breakout]
RTD-        →    Force- (F-)
RTD-        →    Sense- (S-) [wire link on breakout]

Reference Resistor: 4.3kΩ between RREF+ and RREF-
```

### DEEK-ROBOT Shield Connections

The shield connects automatically when stacked:

```
DS1307 RTC     →    I2C (A4/SDA, A5/SCL)
SD Card        →    SPI (Pins 10, 11, 12, 13)
LED Indicator  →    Pin 9 (modified from Pin 13 to avoid SPI conflict)
```

## Software Setup

### Required Arduino Libraries

Install these libraries via Arduino IDE Library Manager:

1. **Adafruit MAX31865**

   ```
   Library Manager → Search "Adafruit MAX31865" → Install
   ```

2. **RTClib** (for DS1307 RTC)

   ```
   Library Manager → Search "RTClib" → Install
   ```

3. **SdFat** (for SD card operations)
   ```
   Library Manager → Search "SdFat" → Install
   ```

### Project Files

- `datalogger.ino` - Main Arduino sketch
- `sync_time.py` - Python script for time synchronization
- `sync_time_precise.py` - High-precision time sync script
- `requirements.txt` - Python dependencies

## Features

### Data Logging

- **Temperature measurement** via PT1000 RTD sensor
- **1-second logging interval** (configurable)
- **CSV format** with ISO 8601 timestamps
- **Automatic file naming** based on startup time
- **Fault detection** for sensor errors

### Real-Time Clock

- **DS1307 RTC** for accurate timestamps
- **Battery backup** maintains time during power loss
- **Serial time synchronization** for precision setting

### Data Storage

- **MicroSD card** storage (FAT16/FAT32)
- **Automatic file creation** with timestamps
- **CSV format** for easy data analysis
- **File header** with column names

### Visual Feedback

- **LED indicator** flashes on successful data recording
- **500ms flash duration** every second
- **Status indication** for system health

### Standalone Operation

- **No USB dependency** for normal operation
- **Automatic startup** when powered
- **Battery or wall adapter** power options

## File Output Format

Data is logged in CSV format with the following structure:

```csv
date,temp
2025-09-05T14:30:00Z,23.45
2025-09-05T14:30:01Z,23.47
2025-09-05T14:30:02Z,23.44
```

### File Naming Convention

```
log_YYYYMMDD_HHMMSS.csv
```

Example: `log_20250905_143022.csv`

## Configuration

### Logging Interval

Change the logging frequency by modifying:

```cpp
const unsigned int logIntervalSeconds = 1; // 1 second intervals
```

### Sensor Configuration

PT1000 specific settings:

```cpp
const float RREF = 4300.0;     // 4.3kΩ reference resistor
const float RNOMINAL = 1000.0; // PT1000 nominal resistance at 0°C
```

### Pin Assignments

Modify pin assignments if needed:

```cpp
const int sdCardPin = 10;      // SD card CS pin (shield default)
const int LED_PIN = 9;         // LED indicator pin
const int MAX31865_CS = 8;     // MAX31865 chip select
```

## Time Synchronization

### Precise Synchronization

```bash
python sync_time_precise.py --port /dev/ttyACM0
```

### Auto-Detection

```bash
python sync_time_precise.py
```

### Manual Time Setting

Send via serial terminal:

```
SYNC_TIME:2025,09,05,14,30,00
```

## Serial Commands

### Time Synchronization

```
SYNC_TIME:YYYY,MM,DD,HH,MM,SS
```

### Expected Responses

```
TIME_SYNCED:OK              # Success
TIME_SYNCED:ERR_RANGE       # Invalid date/time values
TIME_SYNCED:ERR_FMT         # Invalid format
```

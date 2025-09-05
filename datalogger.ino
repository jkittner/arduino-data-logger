#include <Adafruit_MAX31865.h>
#include <RTClib.h>
#include <SdFat.h>

/* configuration */
char filename[32]; // Dynamic filename based on startup time
const char *fileHeader = "date,temp";
const unsigned int logIntervalSeconds = 1;

const int sdCardPin = 10;
const int LED_PIN = 9; // Changed from 13 to avoid SPI conflict

// MAX31865 configuration (using hardware SPI)
const int MAX31865_CS = 8; // Chip select pin for MAX31865
// Hardware SPI pins (on most Arduino boards):
// MOSI = 11, MISO = 12, SCK = 13

// PT1000 configuration
const float RREF = 4300.0;     // Reference resistor value (4.3k for PT1000)
const float RNOMINAL = 1000.0; // PT1000 nominal resistance at 0°C

SdFat SD;
RTC_DS1307 RTC;

// Initialize MAX31865 sensor (using hardware SPI)
Adafruit_MAX31865 max31865 = Adafruit_MAX31865(MAX31865_CS);
float temp;
char timeString[] = "0000-00-00T00:00:00Z";
unsigned long currentMillis = 0;
unsigned long offsetMillis = 0;

// LED flash variables
const unsigned int LED_FLASH_DURATION = 250; // flash duration in ms
unsigned long ledFlashStartTime = 0;
bool ledFlashing = false;

/*derived constants */
unsigned long logIntervalMillis = logIntervalSeconds * 1000;

// Callback function to set file timestamps from RTC
void dateTime(uint16_t *date, uint16_t *time) {
  DateTime now = RTC.now();

  // Return date using FAT_DATE macro to pack date into 16 bits
  *date = FAT_DATE(now.year(), now.month(), now.day());

  // Return time using FAT_TIME macro to pack time into 16 bits
  *time = FAT_TIME(now.hour(), now.minute(), now.second());
}

void startLEDFlash() {
  digitalWrite(LED_PIN, HIGH);
  ledFlashStartTime = millis();
  ledFlashing = true;
}

void updateLEDFlash() {
  if (ledFlashing && (millis() - ledFlashStartTime >= LED_FLASH_DURATION)) {
    digitalWrite(LED_PIN, LOW);
    ledFlashing = false;
  }
}

void writeFileHeader(File &dataFile) {
  // Move to end of file for appending
  dataFile.seek(dataFile.size());
  dataFile.println(fileHeader);
  dataFile.flush(); // Ensure data is written to SD card
  Serial.println("HDR OK");
}

void syncClock() {
  char command[64];
  memset(command, 0, sizeof(command)); // Clear buffer to prevent garbage data

  int bytesRead = Serial.readBytesUntil('\n', command, sizeof(command) - 1);
  if (bytesRead <= 0) {
    return; // No data received
  }

  command[bytesRead] = '\0'; // Null terminate

  // Clear any remaining data in serial buffer
  while (Serial.available()) {
    Serial.read();
  }

  if (strncmp(command, "SYNC_TIME:", 10) == 0) {
    // Expected format: SYNC_TIME:YYYY,MM,DD,HH,MM,SS
    char *timeData = command + 10; // Remove "SYNC_TIME:" prefix

    int year, month, day, hour, minute, second;
    if (sscanf(timeData, "%d,%d,%d,%d,%d,%d", &year, &month, &day, &hour,
               &minute, &second) == 6) {

      // Validate date/time ranges before using
      if (year >= 2000 && year <= 2100 && month >= 1 && month <= 12 &&
          day >= 1 && day <= 31 && hour >= 0 && hour <= 23 && minute >= 0 &&
          minute <= 59 && second >= 0 && second <= 59) {

        RTC.adjust(DateTime(year, month, day, hour, minute, second));
        Serial.println("TIME_SYNCED:OK");
        Serial.print("RTC: ");
        DateTime now = RTC.now();
        snprintf(timeString, sizeof(timeString),
                 "%04d-%02d-%02dT%02d:%02d:%02dZ", now.year(), now.month(),
                 now.day(), now.hour(), now.minute(), now.second());
        Serial.println(timeString);
      } else {
        Serial.println("TIME_SYNCED:ERR_RANGE");
      }
    } else {
      Serial.println("TIME_SYNCED:ERR_FMT");
    }
  }
}

void setup() {
  Serial.begin(9600);

  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Ensure LED starts off

  /* 1. initialize the RTC clock */
  if (!RTC.begin()) {
    Serial.println("RTC FAIL");
    while (1)
      ;
  }

  /* 1.5. initialize the MAX31865 sensor */
  max31865.begin(MAX31865_4WIRE); // Use 4-wire configuration for PT1000
  Serial.println("MAX31865 OK");

  /* 2. setup the SD card */
  if (!SD.begin(sdCardPin)) {
    Serial.println("SD FAIL");
    while (1)
      ;
  }
  Serial.println("SD OK");

  // Set callback for file timestamps
  SdFile::dateTimeCallback(dateTime);

  /* 3. Create new data file with startup timestamp */
  DateTime startupTime = RTC.now();
  snprintf(filename, sizeof(filename), "log_%04d%02d%02d_%02d%02d%02d.csv",
           startupTime.year(), startupTime.month(), startupTime.day(),
           startupTime.hour(), startupTime.minute(), startupTime.second());

  Serial.print("FILE: ");
  Serial.println(filename);

  File dataFile = SD.open(filename, FILE_WRITE);
  if (dataFile) {
    writeFileHeader(dataFile);
    dataFile.close();
    Serial.println("LOG START");
  } else {
    Serial.println("FILE ERR");
    while (1)
      ;
  }

  Serial.println("READY");
}

float readSensor() {
  // Read temperature from MAX31865 with PT1000 sensor
  uint16_t rtd = max31865.readRTD();

  // Check for faults
  uint8_t fault = max31865.readFault();
  if (fault) {
    Serial.println("SENSOR FAIL");
    max31865.clearFault(); // Clear the fault
    return NAN;            // Return NaN for invalid reading
  }

  // Calculate temperature using the library's built-in function
  float temperature = max31865.temperature(RNOMINAL, RREF);

  return temperature;
}

void loop() {
  currentMillis = millis();
  // Handle LED flash timing (non-blocking)
  updateLEDFlash();

  // Check for time sync command from serial
  if (Serial.available()) {
    syncClock();
  }

  /* open the file to log data to*/
  File dataFile = SD.open(filename, FILE_WRITE);
  /* start logging the data */
  DateTime now = RTC.now();
  snprintf(timeString, sizeof(timeString), "%04d-%02d-%02dT%02d:%02d:%02dZ",
           now.year(), now.month(), now.day(), now.hour(), now.minute(),
           now.second());
  temp = readSensor();
  if (dataFile) {
    dataFile.print(timeString);
    dataFile.print(",");
    // the last value can use println to add newline
    dataFile.println(temp, 2);
    dataFile.flush();
    dataFile.close();

    // Flash LED to indicate successful data recording
    startLEDFlash();
  } else {
    Serial.println("LOG ERR");
  }
  offsetMillis = millis() - currentMillis;
  Serial.println(timeString);
  // Adjust delay to maintain consistent interval (prevent underflow)
  if (offsetMillis < logIntervalMillis) {
    delay(logIntervalMillis - offsetMillis);
  } else {
    delay(10); // Minimum delay if processing took too long
  }
}

#include <Adafruit_MAX31865.h>
#include <LowPower.h>
#include <RTClib.h>
#include <SdFat.h>

/* configuration */
char filename[32]; // Dynamic filename based on startup time
const char *fileHeader = "ID,date,temp";
const unsigned int logIntervalSeconds = 1;
const char *ID = "02";
const int sdCardPin = 10;

// MAX31865 configuration (using software SPI to avoid conflicts with SD card)
const int MAX31865_CS = 8;  // Chip select pin for MAX31865
const int MAX31865_DI = 7;  // Data in (MOSI) - use different pin from SD card
const int MAX31865_DO = 6;  // Data out (MISO) - use different pin from SD card
const int MAX31865_CLK = 5; // Clock - use different pin from SD card

// PT1000 configuration
const float RREF = 4300.0;     // Reference resistor value (4.3k for PT1000)
const float RNOMINAL = 1000.0; // PT1000 nominal resistance at 0°C

SdFat SD;
RTC_DS1307 RTC;

// Initialize MAX31865 sensor (using software SPI)
Adafruit_MAX31865 max31865 =
    Adafruit_MAX31865(MAX31865_CS, MAX31865_DI, MAX31865_DO, MAX31865_CLK);
float temp;
char timeString[] = "0000-00-00T00:00:00Z";

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
  // Re-enable serial communication after wake-up to check for commands
  Serial.begin(9600);

  // Give serial a moment to initialize
  delay(50);

  // Send wake-up signal to indicate Arduino is ready for time sync
  Serial.println("WAKE_UP");
  Serial.flush();

  // Wait briefly for time sync command (50ms window)
  unsigned long syncWindow = millis() + 50;
  while (millis() < syncWindow) {
    if (Serial.available()) {
      syncClock();
      break;
    }
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
    dataFile.print(ID);
    dataFile.print(",");
    dataFile.print(timeString);
    dataFile.print(",");
    // the last value can use println to add newline
    dataFile.println(temp, 2);
    dataFile.flush();
    dataFile.close();
  } else {
    Serial.println("LOG ERR");
  }
  Serial.println(timeString);
  Serial.println(temp);
  Serial.flush();

  // Disable serial communication before sleep for maximum power savings
  Serial.end();

  for (unsigned int i = 0; i < logIntervalSeconds; i++) {
    LowPower.idle(SLEEP_1S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF,
                  SPI_OFF, USART0_OFF, TWI_OFF);
  }
}

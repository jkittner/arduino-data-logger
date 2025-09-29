#include <LowPower.h>
#include <RTClib.h>
#include <SdFat.h>
#include <SoftwareSerial.h>

/* configuration */
char filename[32]; // Dynamic filename based on startup time
const unsigned int logIntervalSeconds = 1;
/* read timeout for serial device */
const unsigned int TIMEOUT = 1000; // milliseconds
const int sdCardPin = 10;

// SoftwareSerial(rxPin, txPin)
SoftwareSerial rs485(2, 3); // RX=D2, TX=D3
const int DE_RE_PIN = 4;    // MAX485 DE/RE control pin
const char *SENSOR_ADDRESS = "#105#";
char outputBuffer[32];

SdFat SD;
RTC_DS1307 RTC;

char timeString[] = "0000-00-00T00:00:00Z";

// Callback function to set file timestamps from RTC
void dateTime(uint16_t *date, uint16_t *time) {
  DateTime now = RTC.now();

  // Return date using FAT_DATE macro to pack date into 16 bits
  *date = FAT_DATE(now.year(), now.month(), now.day());

  // Return time using FAT_TIME macro to pack time into 16 bits
  *time = FAT_TIME(now.hour(), now.minute(), now.second());
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
  rs485.begin(9600);

  /* 1. initialize the RTC clock */
  if (!RTC.begin()) {
    Serial.println("RTC FAIL");
    while (1)
      ;
  }

  /* 1.5. initialize the MAX31865 sensor */
  pinMode(DE_RE_PIN, OUTPUT);
  // start in receive mode
  digitalWrite(DE_RE_PIN, LOW);

  Serial.println("RS485 STARTED");

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
    dataFile.close();
    Serial.println("LOG START");
  } else {
    Serial.println("FILE ERR");
    while (1)
      ;
  }

  Serial.println("READY");
}

char *readSensor() {
  // Read the METEK sonic sensor via RS-485
  // transmit mode
  digitalWrite(DE_RE_PIN, HIGH);
  // allow driver to stabilize
  delay(1);
  /* send the command. In RS-485 half duplex mode it only works if the sensor
     has an address associated with it */
  rs485.print(SENSOR_ADDRESS);
  rs485.print("\r\n");
  rs485.flush();
  delay(1);
  /* switch to receiver mode */
  digitalWrite(DE_RE_PIN, LOW);
  unsigned long start = millis();
  memset(outputBuffer, 0, sizeof(outputBuffer));
  int index = 0;
  while (millis() - start < TIMEOUT) { // read up to 1 second
    while (rs485.available()) {
      // we need to read the full buffer even if it exceeds our buffer size
      // so empty it
      char c = rs485.read();
      if (index < sizeof(outputBuffer) - 1) {
        outputBuffer[index++] = c;
      }
    }
  }
  return outputBuffer;
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
  char *sonic_log = readSensor();
  if (dataFile) {
    dataFile.print(timeString);
    dataFile.print(",");
    // the last value can use println to add newline
    dataFile.println(sonic_log);
    dataFile.flush();
    dataFile.close();
  } else {
    Serial.println("LOG ERR");
  }
  Serial.println(timeString);
  Serial.println(sonic_log);
  Serial.flush();

  // Disable serial communication before sleep for maximum power savings
  Serial.end();

  for (unsigned int i = 0; i < logIntervalSeconds; i++) {
    LowPower.idle(SLEEP_1S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF,
                  SPI_OFF, USART0_OFF, TWI_OFF);
  }
}

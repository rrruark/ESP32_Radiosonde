#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SHT4x.h>
#include <Adafruit_BMP280.h>
#include <RadioLib.h>

// Configuration
#define WIFI_SSID "starlink"
#define WIFI_PASSWORD "redmondwa"
#define DEVICE_NAME "Radiosonde"

// Radio and Sensors
#define RADIO_CS 5
#define RADIO_SCK 18
#define RADIO_MOSI 23
#define RADIO_MISO 19
#define RADIO_RESET 14
#define RADIO_BUSY 34
#define RADIO_DIO1 35
#define LED1 12
#define SDA_1 4
#define SCL_1 15
#define GPS_RX 16
#define GPS_TX 17

TwoWire I2C_1 = TwoWire(1);

LLCC68 radio = new Module(RADIO_CS, RADIO_DIO1, RADIO_RESET, RADIO_BUSY);

struct __attribute__((packed)) SensorData
{
  float latitude_deg;
  float longitude_deg;
  float height_ft;
  float pressure_altitude_ft;
  int8_t temperature_c;
  uint8_t relative_humidity_pct;
  uint16_t battery_voltage_max10;
};

SensorData sensorData;

void checkState(int16_t state)
{
  if (state != RADIOLIB_ERR_NONE)
  {
    Serial.printf("[LLCC68] Configuration failed with code %d\n", state);
    while (true)
    {
    }
  }
}

void setupWiFi()
{
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected.");
}

void setupOTA()
{
  ArduinoOTA.setHostname(DEVICE_NAME);
  ArduinoOTA.onStart([]()
  {
    Serial.println("OTA Update Started");
  });
  ArduinoOTA.onEnd([]()
  {
    Serial.println("\nOTA Update Completed");
  });
  ArduinoOTA.onError([](ota_error_t error)
  {
    Serial.printf("OTA Error[%u]: ", error);
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready.");
}

void transmitData()
{
  if (sensorData.height_ft <= 5000)
  {
    digitalWrite(LED1, HIGH);
  }
  else
  {
    digitalWrite(LED1, LOW);
  }

  int16_t state = radio.transmit((uint8_t *)&sensorData, sizeof(sensorData));
  digitalWrite(LED1, LOW);

  if (state == RADIOLIB_ERR_NONE)
  {
    Serial.println("[radio] Transmission success.");
  }
  else
  {
    Serial.printf("[radio] Transmission failed with code %d\n", state);
  }
}

void setup()
{
  pinMode(LED1, OUTPUT);
  digitalWrite(LED1, LOW);

  Serial.begin(115200);


  I2C_1.begin(SDA_1, SCL_1);
  SPI.begin(RADIO_SCK, RADIO_MISO, RADIO_MOSI, RADIO_CS);

 
  // Initialize Radio
  checkState(radio.begin(915.0, 250.0, 9, 7, 0x2B, 8, 16, 0, 1));

  // Initialize WiFi and OTA
  setupWiFi();
  setupOTA();
}

void loop()
{
  ArduinoOTA.handle();
  
  int state = radio.receive((uint8_t*)&sensorData,sizeof(SensorData));
  if (state == RADIOLIB_ERR_NONE)
  {
    Serial.println("[radio] Received data.");
    Serial.printf("Latitude: %f\n", sensorData.latitude_deg);
    Serial.printf("Longitude: %f\n", sensorData.longitude_deg);
    Serial.printf("Height: %f\n", sensorData.height_ft);
    Serial.printf("Pressure Altitude: %f\n", sensorData.pressure_altitude_ft);
    Serial.printf("Temperature: %d\n", sensorData.temperature_c);
    Serial.printf("Relative Humidity: %d\n", sensorData.relative_humidity_pct);
    Serial.printf("Battery Voltage: %d\n", sensorData.battery_voltage_max10);
  }
  else
  {
    //Serial.printf("[radio] Receive failed with code %d\n", state);
  }
  //delay(500);
  //deepSleepSchedule();
}

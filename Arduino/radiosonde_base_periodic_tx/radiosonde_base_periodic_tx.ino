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
#define RADIO_RXE 2
#define LED1 12
#define LED2 13
#define SDA_1 4
#define SCL_1 15
#define GPS_RX 16
#define GPS_TX 17

TwoWire I2C_1 = TwoWire(1);

SX1262 radio = new Module(RADIO_CS, RADIO_DIO1, RADIO_RESET, RADIO_BUSY);

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
    Serial.printf("[SX1262] Configuration failed with code %d\n", state);
    while (true)
    {
    }
  }
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
  //pinMode(RADIO_RXE, OUTPUT);
  //digitalWrite(RADIO_RXE, HIGH);
  Serial.begin(115200);


  I2C_1.begin(SDA_1, SCL_1);
  SPI.begin(RADIO_SCK, RADIO_MISO, RADIO_MOSI, RADIO_CS);

 
  // Initialize Radio
  checkState(radio.begin(906.875, 250.0, 11, 5, 0x2B, 8, 16, 0, 1)); //freq, bw, sf, cr, sync, pwr, preamble, tcxo_v, use_ldo
  radio.setRfSwitchPins(RADIO_RXE,LED1);

}

void loop()
{
  digitalWrite(LED1, HIGH);

  radio.transmit((uint8_t *)&sensorData, sizeof(sensorData));
  delay(500);

  Serial.println("tx done");
  //deepSleepSchedule();
}

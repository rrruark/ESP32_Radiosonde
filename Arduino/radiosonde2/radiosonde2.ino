//Radiosonde
//2024 Robert Ruark

//Select "ESP32 Dev Module" in Arduino. Why their abstraction addicted devs 
//don't allow one to select the actual module name is beyond me.

#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SHT4x.h>
#include <Adafruit_BMP280.h>
#include <RadioLib.h>
#include <WiFi.h>
#include <esp_wifi.h>

// Configuration
#define TX_INTERVAL_SECONDS 2

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
Adafruit_BMP280 bmp(&I2C_1);
Adafruit_SHT4x sht40 = Adafruit_SHT4x();
LLCC68 radio = new Module(RADIO_CS, RADIO_DIO1, RADIO_RESET, RADIO_BUSY);

struct __attribute__((packed)) SensorData
{
  float latitude_deg;
  float longitude_deg;
  float height_ft;
  float pressure_altitude_ft;
  int8_t temperature_c;
  float time;
  uint8_t relative_humidity_pct;
  //uint16_t battery_voltage_max10;
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

void readSensors()
{
  // Read SHT40
  sensors_event_t humidity, temp;
  sht40.getEvent(&humidity, &temp);

  if (isnan(temp.temperature))
  {
    sensorData.temperature_c = INT8_MAX;
  }
  else
  {
    sensorData.temperature_c = (int8_t)temp.temperature;
  }

  if (isnan(humidity.relative_humidity))
  {
    sensorData.relative_humidity_pct = UINT8_MAX;
  }
  else
  {
    sensorData.relative_humidity_pct = (uint8_t)humidity.relative_humidity;
  }

  // Read BMP280
  sensorData.pressure_altitude_ft = bmp.readAltitude(1013.25) * 3.28084;

  if (isnan(sensorData.pressure_altitude_ft))
  {
    sensorData.pressure_altitude_ft = INT16_MAX;
  }

}

void readGPS()
{
  static String gnggaMessage = "";
  static bool gnggaStarted = false;

  while (Serial2.available())
  {
    char data = Serial2.read();

    if (data == '$')
    {
      gnggaStarted = true;
      gnggaMessage = "";
    }

    if (gnggaStarted)
    {
      gnggaMessage += data;
    }

    if (data == '\n' && gnggaStarted)
    {
      if (gnggaMessage.startsWith("$GNGGA"))
      {
        parseGNGGA(gnggaMessage);
      }
      gnggaStarted = false;
    }
  }
}

uint8_t readID()
{
  uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK) 
  {
    return baseMac[5];
  } else 
  {
    Serial.println("Failed to read MAC address");
    return 255;
  } 
}

void parseGNGGA(String gngga)
{
  int commaIndex = 0;
  int fieldIndex = 0;
  String fields[15];

  while (commaIndex >= 0)
  {
    commaIndex = gngga.indexOf(',');
    fields[fieldIndex] = gngga.substring(0, commaIndex);
    gngga = gngga.substring(commaIndex + 1);
    
    fieldIndex++;
  }
  //Serial.println(gngga);
  // Extract latitude, longitude, and altitude
  if (fields[2].length() > 0 && fields[4].length() > 0)
  {
    sensorData.time = fields[1].toFloat();
    sensorData.latitude_deg = fields[2].toFloat() / 100.0;  // Convert to decimal degrees
    sensorData.longitude_deg = fields[4].toFloat() / 100.0;
    sensorData.height_ft = fields[9].toFloat() * 3.28084;  // Convert meters to feet
  }
  else
  {
    sensorData.latitude_deg = NAN;
    sensorData.longitude_deg = NAN;
    sensorData.height_ft = NAN;
  }
}

void transmitData()
{
  digitalWrite(LED1, HIGH);
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
  Serial2.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  I2C_1.begin(SDA_1, SCL_1);
  SPI.begin(RADIO_SCK, RADIO_MISO, RADIO_MOSI, RADIO_CS);

  // Initialize BMP280
  if (!bmp.begin(0x76))
  {
    Serial.println("BMP280 not found.");
    while (1)
    {
    }
  }

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL);

  // Initialize SHT40
  if (!sht40.begin(&I2C_1))
  {
    Serial.println("SHT40 not found.");
    while (1)
    {
    }
  }

  // Initialize Radio
  checkState(radio.begin(915.0, 250.0, 9, 7, 0x2B, 22, 16, 0, 1));

  // Initialize WiFi and OTA
  //setupWiFi();
  //setupOTA();
  //Power savings
  WiFi.mode(WIFI_OFF);
  btStop();
}

int counter=0;
void loop()
{
  
  readGPS();
  readSensors();
  //transmitData();
  counter++;
  if(counter>(TX_INTERVAL_SECONDS*2-1))
  {
    transmitData();
    counter=0;
  }

  delay(500);
  //esp_sleep_enable_timer_wakeup(500 * 1000);
  //esp_light_sleep_start();
}

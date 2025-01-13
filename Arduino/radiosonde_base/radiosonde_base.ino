//interrupt driven rx
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

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

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

// flag to indicate that a packet was received
volatile bool receivedFlag = false;

// this function is called when a complete packet
// is received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  // we got a packet, set the flag
  receivedFlag = true;
}

struct __attribute__((packed)) SensorData
{
  float latitude_deg;
  float longitude_deg;
  float height_ft;
  float pressure_altitude_ft;
  int8_t temperature_c;
  uint16_t index;
  uint8_t relative_humidity_pct;
  //uint16_t battery_voltage_max10;
};

float rssi;
float snr;
float frequency_error;

SensorData sensorData;

uint16_t packet_counter=0;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_1, -1);

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

void radio_rx(int verbosity)
{
  if(receivedFlag) 
  {
    // reset flag
    receivedFlag = false;

    // you can read received data as an Arduino String
    int state = radio.readData((uint8_t*)&sensorData,sizeof(SensorData));

    // you can also read received data as byte array
    /*
      byte byteArr[8];
      int numBytes = radio.getPacketLength();
      int state = radio.readData(byteArr, numBytes);
    */

    if (state == RADIOLIB_ERR_NONE) {
      // packet was successfully received
      packet_counter++;
      rssi = radio.getRSSI();
      snr = radio.getSNR();
      frequency_error = radio.getFrequencyError();

      //local display
      OLED_display();
      if(verbosity==0) print_csv();
      else print_verbose();

      print_thermal();

    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      // packet was received, but is malformed
      Serial.println(F("CRC error!"));
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 0);
      display.println("Radiosonde GS");
      display.setCursor(0, 8);
      display.print("CRC error!");

    } else {
      // some other error occurred
      Serial.print(F("failed, code "));
      Serial.println(state);

    }
  } 
}

void print_csv()
{
  Serial.print(sensorData.index);
  Serial.print(",");
  Serial.print(sensorData.latitude_deg);
  Serial.print(",");
  Serial.print(sensorData.longitude_deg);
  Serial.print(",");    
  Serial.print(sensorData.height_ft);
  Serial.print(",");  
  Serial.print(sensorData.pressure_altitude_ft);
  Serial.print(",");  
  Serial.print(sensorData.temperature_c);
  Serial.print(",");        
  Serial.print(sensorData.relative_humidity_pct);
  Serial.print(",");   
  Serial.print(rssi);
  Serial.print(",");    
  Serial.print(snr);
  Serial.print(",");  
  Serial.println(frequency_error);    
}

void print_thermal()
{
  Serial2.print(sensorData.index);
  Serial2.print(",");
  Serial2.print(sensorData.latitude_deg,4);
  Serial2.print(",");
  Serial2.print(sensorData.longitude_deg,4);
  Serial2.print(",");    
  Serial2.print(sensorData.height_ft);
  Serial2.print(",");  
  Serial2.print(sensorData.pressure_altitude_ft);
  Serial2.print(",");  
  Serial2.print(sensorData.temperature_c);
  Serial2.print(",");        
  Serial2.print(sensorData.relative_humidity_pct);
  Serial2.print(",");   
  Serial2.println(rssi);
  //Serial.print(",");    
  //Serial.print(snr);
  //Serial.print(",");  
  //Serial.println(frequency_error);    
}

void print_thermal_header()
{
  Serial2.println("Radiosonde Ground Station");
  Serial2.print("index");
  Serial2.print(",");
  Serial2.print("lat");
  Serial2.print(",");
  Serial2.print("lon");
  Serial2.print(",");    
  Serial2.print("height");
  Serial2.print(",");  
  Serial2.print("pressure_alt");
  Serial2.print(",");  
  Serial2.print("temp");
  Serial2.print(",");        
  Serial2.print("humidity");
  Serial2.print(",");   
  Serial2.println("rssi");
  //Serial.print(",");    
  //Serial.print(snr);
  //Serial.print(",");  
  //Serial.println(frequency_error);    
}

void print_verbose()
{
  Serial.print(F("[SX1262] Received packet: "));
  Serial.println(packet_counter);
  // print data of the packet
  //Serial.println("[radio] Received data.");
  Serial.printf("Latitude: %f\n", sensorData.latitude_deg);
  Serial.printf("Longitude: %f\n", sensorData.longitude_deg);
  Serial.printf("Height: %f\n", sensorData.height_ft);
  Serial.printf("Pressure Altitude: %f\n", sensorData.pressure_altitude_ft);
  Serial.printf("Temperature: %d\n", sensorData.temperature_c);
  Serial.printf("Relative Humidity: %d\n", sensorData.relative_humidity_pct);
  //Serial.printf("Battery Voltage: %d\n", sensorData.battery_voltage_max10);

  // print RSSI (Received Signal Strength Indicator)
  Serial.print(F("[SX1262] RSSI:\t\t"));
  Serial.print(rssi);
  Serial.println(F(" dBm"));

  // print SNR (Signal-to-Noise Ratio)
  Serial.print(F("[SX1262] SNR:\t\t"));
  Serial.print(snr);
  Serial.println(F(" dB"));
  // print frequency error
  Serial.print(F("[SX1262] Frequency error:\t"));
  Serial.print(frequency_error);
  Serial.println(F(" Hz"));
}
void OLED_display()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Radiosonde GS");
  display.setCursor(0, 8);
  display.print("Packet Index: ");
  display.print(sensorData.index);
  display.setCursor(0, 16);
  display.print("Pos: ");
  display.print(sensorData.latitude_deg,4);
  display.print(",");
  display.println(sensorData.longitude_deg,4);
  display.setCursor(0, 24);
  display.print("GPS Alt: ");
  display.print(sensorData.height_ft,0);
  display.print("ft.");
  display.setCursor(0, 32);
  display.print("P Alt:   ");
  display.print(sensorData.pressure_altitude_ft,0);
  display.print("ft.");  
  display.setCursor(0, 40);
  display.print("Temp: ");
  display.print(sensorData.temperature_c);
  display.print("C, RH: ");   
  display.print(sensorData.relative_humidity_pct);
  display.print("%");   
  display.setCursor(0, 48);  
  display.print("SNR: ");
  display.print(snr,1);
  display.print("dB"); 
  display.setCursor(0, 56);
  display.print("RSSI: "); 
  display.print(rssi,1);
  display.print("dBm"); 
  display.display(); 
}

void setup()
{
  pinMode(LED1, OUTPUT);
  digitalWrite(LED1, LOW);

  Serial.begin(115200);


  I2C_1.begin(SDA_1, SCL_1);
  SPI.begin(RADIO_SCK, RADIO_MISO, RADIO_MOSI, RADIO_CS);

   //OLED Stuff
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
  { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(500);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  // Display static text
  display.println("Radiosonde GS");
  display.display(); 

  // Initialize Radio
  checkState(radio.begin(915.0, 250.0, 9, 7, 0x2B, 8, 16, 0, 1));
  radio.setRfSwitchPins(RADIO_RXE,LED1);
  radio.setPacketReceivedAction(setFlag);
  checkState(radio.startReceive());

  // Initialize UART2 with TX on pin 17 and RX on pin 16
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  print_thermal_header();
}

void loop()
{
  radio_rx(0); // 0=csv, 1=verbose

  //Serial.println("no rx");

  //delay(500);
  //deepSleepSchedule();
}

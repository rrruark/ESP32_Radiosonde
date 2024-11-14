#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SHT4x.h>
#include  <Adafruit_BMP280.h>
#include <EEPROM.h>
#include <RadioLib.h>

#define radio_CS 5
#define radio_SCK 18
#define radio_MOSI 23
#define radio_MISO 19
#define radio_RESET 14
#define radio_BUSY 34
#define radio_DIO1 35
#define LED1 12
#define LED2 13
#define radio_DIO2_AS_RF_SWITCH 1

//I2C Bus
#define SDA_1 4
#define SCL_1 15

//TwoWire I2C_0 = TwoWire(0);
TwoWire I2C_1 = TwoWire(1);
//bmp280
Adafruit_BMP280 bmp(&I2C_1);
// SHT40 sensor
Adafruit_SHT4x sht40 = Adafruit_SHT4x();

//radio
LLCC68 radio = new Module(radio_CS, radio_DIO1, radio_RESET, radio_BUSY);

void checkState(int16_t state) {
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print(F("[LLCC68] configuration failed with code "));
    Serial.println(state);
    digitalWrite(LED1,1);

    while (true) {}
  }
}

// the setup function runs once when you press reset or power the board
void setup() 
{
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED1,OUTPUT);
  pinMode(LED2,OUTPUT);
  digitalWrite(LED1,0);
  digitalWrite(LED2,0);

  // Initialize UART1 (default pins: TX=1, RX=3)
  Serial.begin(115200);
  delay(500);
  Serial.println("initialized!");

  //init radio
  SPI.begin(radio_SCK, radio_MISO, radio_MOSI, radio_CS);
  delay(1000);
  Serial.printf("SPI.begin(SCK=%d, MISO=%d, MOSI=%d, CS=%d)\n", radio_SCK, radio_MISO, radio_MOSI, radio_CS);
  SPI.setFrequency(100000);

  Serial.println(F("[LLCC68] Initialising ... "));

  checkState(radio.begin(915.0, 250.0, 9, 7, 0x2B, 8, 16, 0,1)); //868.0, 250.0, 11, 5, 0x2B, 8, 16, 1.8, false
  checkState(radio.setFrequency(915.0, false)); // you can remove this line and just put 869.525 instead of 868.0 in the line above once a release with the calibration range has been released.
  checkState(radio.setCurrentLimit(140));
  checkState(radio.setCRC(true));
  checkState(radio.setDio2AsRfSwitch(radio_DIO2_AS_RF_SWITCH));
  //radio.setRfSwitchPins(radio_RXEN, radio_TXEN); // void
  checkState(radio.setRxBoostedGainMode(true));
  Serial.println(F("[radio] All settings succesfully changed!"));

  // Initialize UART2 with TX on pin 17 and RX on pin 16
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  //init I2C
  I2C_1.begin(SDA_1, SCL_1, 100000);

  // Initialize SHT40 sensor
  if (!sht40.begin(&I2C_1)) {
    Serial.println("Couldn't find SHT40");
    while (1) delay(1);
  }
  sht40.setPrecision(SHT4X_HIGH_PRECISION);

  //init BMP280
  if  (!bmp.begin(0x76)) 
  {
    Serial.println(F("Could not find a valid BMP280 sensor,  check wiring!"));
    while (1);
  }
  /* Default settings from datasheet.  */
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500);  /* Standby time. */

}

// the loop function runs over and over again forever
void loop() 
{
  read_sensors();
  read_gps();
  sendThing();
  delay(1000);
}

void read_sensors()
{
  // Read temperature and humidity from SHT40
  sensors_event_t humidity, temp;
  sht40.getEvent(&humidity, &temp);
  Serial.print("SHT40 temperature:");
  Serial.println(temp.temperature);
  Serial.print("SHT40 humidity:");
  Serial.println(humidity.relative_humidity);
  Serial.print("BMP280 temperature:");  
  Serial.println(bmp.readTemperature());
  Serial.print("BMP280 pressure (hpa):");  
  Serial.println(bmp.readPressure()/100);  //displaying the Pressure in hPa, you can change the unit  
}

void read_gps()
{
  static String gnggaMessage = "";
  static bool gnggaStarted = false;

  // Check if data is available on UART2
  while (Serial2.available()) {
    // Read one character from UART2
    char data = Serial2.read();
    
    // Check if the character is the start of a GNGGA message
    if (data == '$') {
      gnggaStarted = true;
      gnggaMessage = "";
    }

    // If a GNGGA message has started, append the character to the message
    if (gnggaStarted) {
      gnggaMessage += data;
    }

    // Check if the character is the end of a line
    if (data == '\n' && gnggaStarted) {
      // Check if the message is a GNGGA message
      if (gnggaMessage.startsWith("$GNGGA")) {
        // Print the GNGGA message to UART1
        //Serial.print(gnggaMessage);
        parseGNGGA(gnggaMessage);
      }
      gnggaStarted = false;
    }
  }
}

void parseGNGGA(String gngga) {
  int commaIndex = 0;
  int fieldIndex = 0;
  String fields[15];

  // Split the GNGGA message into fields
  while (commaIndex >= 0) {
    commaIndex = gngga.indexOf(',');
    fields[fieldIndex] = gngga.substring(0, commaIndex);
    gngga = gngga.substring(commaIndex + 1);
    fieldIndex++;
  }

  // Extract latitude, longitude, and altitude
  String latitude = fields[2];
  String latitudeDir = fields[3];
  String longitude = fields[4];
  String longitudeDir = fields[5];
  String altitude = fields[9];

  // Print the extracted values
  Serial.print("Latitude: ");
  Serial.print(latitude);
  Serial.print(" ");
  Serial.println(latitudeDir);
  Serial.print("Longitude: ");
  Serial.print(longitude);
  Serial.print(" ");
  Serial.println(longitudeDir);
  Serial.print("Altitude: ");
  Serial.print(altitude);
  Serial.println(" meters");
}
int count = 0;
void sendThing()
{
    Serial.print(F("[radio] Transmitting packet ... "));

  // you can transmit C-string or Arduino string up to
  // 256 characters long
  String str = "Hello World! #" + String(count++);
  digitalWrite(LED1,1);
  int state = radio.transmit(str);

  // you can also transmit byte array up to 256 bytes long
  /*
    byte byteArr[] = {0x01, 0x23, 0x45, 0x56, 0x78, 0xAB, 0xCD, 0xEF};
    int state = radio.transmit(byteArr, 8);
  */

  if (state == RADIOLIB_ERR_NONE) {
    // the packet was successfully transmitted
    Serial.println(F("success!"));

    // print measured data rate
    //Serial.print(F("[radio] Datarate:\t"));
    //Serial.print(radio.getDataRate());
    //Serial.println(F(" bps"));

  } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
    // the supplied packet was longer than 256 bytes
    Serial.println(F("too long!"));

  } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
    // timeout occured while transmitting packet
    Serial.println(F("timeout!"));

  } else {
    // some other error occurred
    Serial.print(F("failed, code "));
    Serial.println(state);

  }
  digitalWrite(LED1,0);
}
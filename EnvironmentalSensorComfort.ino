 /*****************************************************************************
    Arduino Environmental Sensor Comfort V1.0
    Collects temperature, humidity, and pressure data. A DHT22 sensor acquires
    data about ambient relative humidity and temperature. A BMP180 sensor 
    acquires pressure data. Temperature, relative humidity, humidex comfort 
    index, atmospheric pressure, and barometric altitude are displayed on a 
    0.96" OLED screen.

    Copyright (C) 2022 Danilo Ciliberti dancili@gmail.com
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program. If not, see <https://www.gnu.org/licenses/>
 ******************************************************************************/

// Load libraries
#include <Arduino.h>
#include <U8g2lib.h>
#include <DHT.h>
#include <SFE_BMP180.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

// Create the display object using the I2C protocol
// u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);   // All Boards without Reset of the Display
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Define some useful constants
#define TEMP 0
#define HUMIDITY 1
#define HUMIDEX 2

// Create the DHT object (which reads temperature and humidity)
DHT dhtSensor(2, DHT22);

// Create the BMP180 object (which reads temperature and pressure)
SFE_BMP180 bmpSensor;
// #define ALTITUDE 125.0 // My altitude in meters

// Variables declaration
char status;
double temperature, pressure, height, humidity, humidex;


// Define functions to draw weather graphics
void drawSymbol(u8g2_uint_t x, u8g2_uint_t y, uint8_t symbol)
{
  // fonts used:
  // encoding values, see: https://github.com/olikraus/u8g2/wiki/fntgrpiconic
  
  switch(symbol)
  {
    case TEMP:
      u8g2.setFont(u8g2_font_open_iconic_human_6x_t);
      u8g2.drawGlyph(x, y, 65);  
      break;
    case HUMIDITY:
      u8g2.setFont(u8g2_font_open_iconic_thing_6x_t);
      u8g2.drawGlyph(x, y, 72);
      break;
    case HUMIDEX:
      u8g2.setFont(u8g2_font_open_iconic_human_6x_t);
      u8g2.drawGlyph(x, y, 66);
      break;
  }
}


/*
  Draw a string with specified pixel offset. 
  The offset can be negative.
  Limitation: The monochrome font with 8 pixel per glyph
*/
void drawScrollString(int16_t offset, const char *s)
{
  static char buf[36];  // should for screen with up to 256 pixel width 
  size_t len;
  size_t char_offset = 0;
  u8g2_uint_t dx = 0;
  size_t visible = 0;
  len = strlen(s);
  if ( offset < 0 )
  {
    char_offset = (-offset)/8;
    dx = offset + char_offset*8;
    if ( char_offset >= u8g2.getDisplayWidth()/8 )
      return;
    visible = u8g2.getDisplayWidth()/8-char_offset+1;
    strncpy(buf, s, visible);
    buf[visible] = '\0';
    u8g2.setFont(u8g2_font_9x18B_mf);
    u8g2.drawStr(char_offset*8-dx, 62, buf);
  }
  else
  {
    char_offset = offset / 8;
    if ( char_offset >= len )
      return; // nothing visible
    dx = offset - char_offset*8;
    visible = len - char_offset;
    if ( visible > u8g2.getDisplayWidth()/8+1 )
      visible = u8g2.getDisplayWidth()/8+1;
    strncpy(buf, s+char_offset, visible);
    buf[visible] = '\0';
    u8g2.setFont(u8g2_font_9x18B_mf);
    u8g2.drawStr(-dx, 62, buf);
  }
  
}


void draw(const char *s, uint8_t symbol, int value)
{
  int16_t offset = -(int16_t)u8g2.getDisplayWidth();
  int16_t len = strlen(s);
  for(;;)
  {
    u8g2.firstPage();
    do {
      drawSymbol(0, 48, symbol);
      u8g2.setFont(u8g2_font_logisoso32_tf);
      u8g2.setCursor(48+3, 42);
      u8g2.print(value);
      // Print percentage char if humidity is shown
      // otherwise print Celsius degree chars
      if (symbol == 1)
      {
        u8g2.print("%");   // requires enableUTF8Print()
      }
      else
      {
        u8g2.print("°C");   // requires enableUTF8Print()
      }
      drawScrollString(offset, s);
    } while ( u8g2.nextPage() );
    delay(20);
    offset+=5;
    if ( offset > len*8+1 )
      break;
  }
}

void write(const char *s, int value, const char *unit)
{
  int16_t offset = -(int16_t)u8g2.getDisplayWidth();
  int16_t len = strlen(s);
  for(;;)
  {
    u8g2.firstPage();
    do {
      u8g2.setFont(u8g2_font_logisoso32_tf);
      u8g2.setCursor(6, 42);
      u8g2.print(value);
      u8g2.print(unit);
      drawScrollString(offset, s);
    } while ( u8g2.nextPage() );
    delay(20);
    offset+=5;
    if ( offset > len*8+1 )
      break;
  }
}


// The program core
void setup() {
  
  Serial.begin(9600);

  // Initialize the temperature sensor
  dhtSensor.begin();

  // Initialize the bmpSensor sensor
  bmpSensor.begin();
  Serial.println(F("REBOOT"));
  if (bmpSensor.begin()){
    Serial.println(F("BMP180 init success"));
  }
  else
  {
    // Oops, something went wrong, this is usually a connection problem,
    // see the comments at the top of this sketch for the proper connections.
    Serial.println(F("BMP180 init fail\n\n"));
    while(1); // Pause forever
  }

  u8g2.begin();  
  u8g2.enableUTF8Print();

}

void loop() {

  // Start a temperature measurement:
  // If request is successful, the number of ms to wait is returned.
  // If request is unsuccessful, 0 is returned.
  status = bmpSensor.startTemperature();
  if (status != 0)
  {
    // Wait for the measurement to complete:
    delay(status);

    // Retrieve the completed temperature measurement:
    // Note that the measurement is stored in the variable T.
    // Function returns 1 if successful, 0 if failure.

    Serial.println(F(" ")); // void line
    Serial.println(F("BMP180 readings"));

    status = bmpSensor.getTemperature(temperature);
    if (status != 0)
    {
      // Print out the measurement:
      Serial.print(F("Temperature: "));
      Serial.print(temperature,2);
      Serial.print(F(" deg C, "));
      Serial.print((9.0/5.0)*temperature+32.0,2);
      Serial.println(F(" deg F"));
      
      // Start a bmpSensor measurement:
      // The parameter is the oversampling setting, from 0 to 3 (highest res, longest wait).
      // If request is successful, the number of ms to wait is returned.
      // If request is unsuccessful, 0 is returned.

      status = bmpSensor.startPressure(3);
      if (status != 0)
      {
        // Wait for the measurement to complete:
        delay(status);

        // Retrieve the completed bmpSensor measurement:
        // Note that the measurement is stored in the variable P.
        // Note also that the function requires the previous temperature measurement (T).
        // (If temperature is stable, you can do one temperature measurement for a number of bmpSensor measurements.)
        // Function returns 1 if successful, 0 if failure.

        status = bmpSensor.getPressure(pressure,temperature);
        if (status != 0)
        {
//           Print out the measurement:
          Serial.print(F("Absolute pressure: "));
          Serial.print(pressure,2);
          Serial.print(F(" mb, "));
          Serial.print(pressure*0.0295333727,2);
          Serial.println(F(" inHg"));

          // The bmpSensor sensor returns abolute bmpSensor, which varies with altitude.
          // To remove the effects of altitude, use the sealevel function and your current altitude.
          // This number is commonly used in weather reports.
          // Parameters: P = absolute bmpSensor in mb, ALTITUDE = current altitude in m.
          // Result: P = sea-level compensated bmpSensor in mb

//          pressure = bmpSensor.sealevel(pressure,ALTITUDE); // we're at 50 meters (San Paolo Bel Sito, NA)
//          Serial.print("relative (sea-level) pressure: ");
//          Serial.print(pressure,2);
//          Serial.print(" mb, ");
//          Serial.print(pressure*0.0295333727,2);
//          Serial.println(" inHg");

          // On the other hand, if you want to determine your altitude from the bmpSensor reading,
          // use the altitude function along with a baseline bmpSensor (sea-level or other).
          // Parameters: P = absolute bmpSensor in mb, P = baseline bmpSensor in mb.
          // Result: a = altitude in m.

          height = bmpSensor.altitude(pressure,1013.25);
          Serial.print(F("Barometric altitude: "));
          Serial.print(height,0);
          Serial.print(F(" meters, "));
          Serial.print(height*3.28084,0);
          Serial.println(F(" feet"));
        }
        else Serial.println(F("error retrieving bmpSensor measurement\n"));
      }
      else Serial.println(F("error starting bmpSensor measurement\n"));
    }
    else Serial.println(F("error retrieving temperature measurement\n"));
  }
  else Serial.println(F("error starting temperature measurement\n"));

  // Read humidity and temperature
  Serial.println(F(" ")); // void line
  Serial.println(F("DHT22 readings"));
  
  humidity = dhtSensor.readHumidity();
  Serial.print(F("Humidity: "));
  Serial.print(humidity,1);
  Serial.println(F("%\t"));

  temperature = dhtSensor.readTemperature();
  Serial.print(F("Temperature: "));
  Serial.print(temperature,1);
  Serial.println(F("°C\t"));

  // Calculates humidex index and display its status
  humidex = temperature + (0.5555 * (0.06 * humidity * pow(10,0.03*temperature) -10));
  Serial.print(F("Humidex: "));
  Serial.print(humidex);
  Serial.print(F("\t"));

  // Humidex category
  if (humidex < 20){
    Serial.println(F("No index"));
  }
  else if (humidex >= 20 && humidex < 27){
    Serial.println(F("Comfort"));
  }
  else if (humidex >= 27 && humidex < 30){
    Serial.println(F("Caution"));
  }
  else if (humidex >= 30 && humidex < 40){
    Serial.println(F("Extreme caution"));
  }
  else if (humidex >= 40 && humidex < 55){
    Serial.println(F("Danger"));
  }
  else {
    Serial.println(F("Extreme danger"));
  }


  // Display values on screen
  draw("Temperatura ambiente", TEMP, temperature);
  draw("Umidita relativa", HUMIDITY, humidity);

  if (humidex < 20){
    draw("Temperatura percepita", HUMIDEX, humidex);
  }
  else if (humidex >= 20 && humidex < 27){
    draw("Temperatura percepita: comfort", HUMIDEX, humidex);
  }
  else if (humidex >= 27 && humidex < 30){
    draw("Temperatura percepita: attenzione", HUMIDEX, humidex);
  }
  else if (humidex >= 30 && humidex < 40){
    draw("Temperatura percepita: fare molta attenzione", HUMIDEX, humidex);
  }
  else if (humidex >= 40 && humidex < 55){
    draw("Temperatura percepita: pericolo!", HUMIDEX, humidex);
  }
  else {
    draw("Temperatura percepita: pericolo estremo!", HUMIDEX, humidex);
  }
  
  write("Pressione atmosferica (mbar)",pressure,"");
  write("Quota barometrica",height," m");

}

#include<Wire.h>
#include<LiquidCrystal_I2C.h>     
//Download ได้จาก https://drive.google.com/file/d/1hkmODw9q8CdxQx99Bp1Eu2foACQ-C4Sx/view?usp=sharing
LiquidCrystal_I2C lcd(0x27,20,4);

//dht11,dht22
#include <DHT.h>                  //https://github.com/adafruit/DHT-sensor-library
#define DHTPIN 15
#define DHTTYPE DHT11
//#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(9600);
  dht.begin(); 
}

void loop() {
  float h = dht.readHumidity();
  float t = dht.readTemperature(); // or dht.readTemperature(true) for Fahrenheit

  Serial.print("Temperature:");         
  Serial.println(t);
  Serial.print("Humidity:");
  Serial.println(h);
  delay(1000);

  //LCD
      lcd.begin();
      lcd.backlight();                                   
      lcd.setCursor(1, 0); 
      lcd.print("TEMP =  "); 
      lcd.print(t);
      lcd.print("  C ");
      
      lcd.setCursor(1, 1);
      lcd.print("HUMI =  "); 
      lcd.print(h);
      lcd.print("  % ");
}

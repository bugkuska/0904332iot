#include<Wire.h>
#include<LiquidCrystal_I2C.h>      //https://github.com/bugkuska/esp32/raw/main/basic/lcd/LiquidCrystal_i2c.zip
LiquidCrystal_I2C lcd(0x27,20,4);

#define relay1        4
#define relay2        5
#define relay3        18 
#define relay4        19

//dht11,dht22
#include <DHT.h>                  //https://github.com/adafruit/DHT-sensor-library
#define DHTPIN 15
#define DHTTYPE DHT11
//#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(9600);
  dht.begin(); 

  pinMode(relay1,OUTPUT);
  pinMode(relay2,OUTPUT);
  pinMode(relay3,OUTPUT);
  pinMode(relay4,OUTPUT);

  digitalWrite(relay1,LOW);
  digitalWrite(relay2,LOW);
  digitalWrite(relay3,LOW);
  digitalWrite(relay4,LOW);
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

  digitalWrite(relay1,LOW);
  delay(1000);
  digitalWrite(relay1,HIGH);
  delay(1000);

  digitalWrite(relay2,LOW);
  delay(1000);
  digitalWrite(relay2,HIGH);
  delay(1000);

  digitalWrite(relay3,LOW);
  delay(1000);
  digitalWrite(relay3,HIGH);
  delay(1000);

  digitalWrite(relay4,LOW);
  delay(1000);
  digitalWrite(relay4,HIGH);
  delay(1000);

  digitalWrite(relay1,LOW);
  digitalWrite(relay2,LOW);
  digitalWrite(relay3,LOW);
  digitalWrite(relay4,LOW);
  delay(1000);

  digitalWrite(relay1,HIGH);
  digitalWrite(relay2,HIGH);
  digitalWrite(relay3,HIGH);
  digitalWrite(relay4,HIGH);
  delay(1000);
}

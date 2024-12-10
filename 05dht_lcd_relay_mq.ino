#include <Wire.h>
#include <LiquidCrystal_I2C.h>  //https://github.com/bugkuska/esp32/raw/main/basic/lcd/LiquidCrystal_i2c.zip
LiquidCrystal_I2C lcd(0x27, 20, 4);

#define MQ3_AOUT 34  // ขา AOUT เชื่อมต่อกับขา ADC (Analog) ของ ESP32
#define VCC 3.3      // แรงดันไฟฟ้าของระบบ (ESP32)
#define RL 10000.0   // ความต้านทานโหลด (10 kΩ)
// ค่า R0 ที่ได้จากการสอบเทียบ
float R0 = 10.0;

#define relay1 4
#define relay2 5
#define relay3 18
#define relay4 19

//dht11,dht22
#include <DHT.h>  //https://github.com/adafruit/DHT-sensor-library
#define DHTPIN 15
#define DHTTYPE DHT11
//#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(9600);
  dht.begin();

  pinMode(MQ3_AOUT, INPUT);  // ตั้งขา AOUT เป็น Input

  // กำหนดขา IO4, IO5, IO18, IO19 เป็น OUTPUT
  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);
  pinMode(relay3, OUTPUT);
  pinMode(relay4, OUTPUT);

  digitalWrite(relay1, LOW);
  digitalWrite(relay2, LOW);
  digitalWrite(relay3, LOW);
  digitalWrite(relay4, LOW);
}

void loop() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();  // or dht.readTemperature(true) for Fahrenheit

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

  digitalWrite(relay1, LOW);
  delay(1000);
  digitalWrite(relay1, HIGH);
  delay(1000);

  digitalWrite(relay2, LOW);
  delay(1000);
  digitalWrite(relay2, HIGH);
  delay(1000);

  digitalWrite(relay3, LOW);
  delay(1000);
  digitalWrite(relay3, HIGH);
  delay(1000);

  digitalWrite(relay4, LOW);
  delay(1000);
  digitalWrite(relay4, HIGH);
  delay(1000);

  digitalWrite(relay1, LOW);
  digitalWrite(relay2, LOW);
  digitalWrite(relay3, LOW);
  digitalWrite(relay4, LOW);
  delay(1000);

  digitalWrite(relay1, HIGH);
  digitalWrite(relay2, HIGH);
  digitalWrite(relay3, HIGH);
  digitalWrite(relay4, HIGH);
  delay(1000);

  // อ่านค่าจาก AOUT (สัญญาณ Analog)
  int analogValue = analogRead(MQ3_AOUT);
  float voltage = (analogValue / 4095.0) * VCC;  // แปลงค่า ADC เป็นแรงดันไฟฟ้า

  // คำนวณค่า Rs
  float Rs = RL * ((VCC - voltage) / voltage);

  // คำนวณอัตราส่วน Rs/R0
  float ratio = Rs / R0;

  // คำนวณ ppm (สมการตัวอย่าง)
  float ppm = pow(10, (log10(ratio) - 0.2) / -0.3);

  // คำนวณ %BAC
  float BAC = ppm * 0.00005;

  // แสดงผลใน Serial Monitor
  Serial.print("Analog Value: ");
  Serial.print(analogValue);
  Serial.print(" | Voltage: ");
  Serial.print(voltage, 2);
  Serial.print(" V | Rs: ");
  Serial.print(Rs, 2);
  Serial.print(" | ppm: ");
  Serial.print(ppm, 2);
  Serial.print(" | %BAC: ");
  Serial.println(BAC, 4);

  delay(1000);  // หน่วงเวลา 1 วินาที
}

#include <Wire.h>                         // ไลบรารีสื่อสาร I2C (ใช้กับ LCD I2C และอุปกรณ์ I2C อื่น ๆ)
#include <LiquidCrystal_I2C.h>            // ไลบรารีควบคุมจอ LCD ผ่าน I2C
//https://github.com/bugkuska/esp32/raw/main/basic/lcd/LiquidCrystal_i2c.zip
LiquidCrystal_I2C lcd(0x27, 16, 2);       // สร้างออบเจกต์ LCD ที่แอดเดรส 0x27 ขนาด 16 คอลัมน์ 2 แถว (บางจออาจเป็น 0x3F)

#define SOIL_PIN 35                       // กำหนดขา ADC ของ ESP32 สำหรับอ่านค่าความชื้นดิน (GPIO35 เป็น input-only)
#define relay1 4                          // ขารีเลย์ 1 (GPIO4)
#define relay2 5                          // ขารีเลย์ 2 (GPIO5)
#define relay3 18                         // ขารีเลย์ 3 (GPIO18)
#define relay4 19                         // ขารีเลย์ 4 (GPIO19)

#include <DHT.h>                          // ไลบรารี Adafruit DHT สำหรับอ่านอุณหภูมิ/ความชื้นอากาศ
#define DHTPIN 15                         // ขาที่ต่อ DATA ของ DHT11 (GPIO15)
#define DHTTYPE DHT11                     // ประเภทเซ็นเซอร์ DHT ที่ใช้ (DHT11)
DHT dht(DHTPIN, DHTTYPE);                 // สร้างออบเจกต์ dht เพื่อเรียกใช้งาน readTemperature/readHumidity

const int SOIL_DRY_ADC = 3000;            // ค่าดิบ ADC เมื่อดินแห้ง (ต้องคาลิเบรตจากอุปกรณ์จริง)
const int SOIL_WET_ADC = 1200;            // ค่าดิบ ADC เมื่อดินเปียกมาก (ต้องคาลิเบรตจากอุปกรณ์จริง)

void setup() {
  Serial.begin(9600);                     // เปิด Serial Monitor ที่ความเร็ว 9600 bps สำหรับดีบัก/แสดงค่า
  dht.begin();                            // เริ่มต้นการทำงานของ DHT11

  pinMode(SOIL_PIN, INPUT);               // ตั้งโหมดขาอ่านความชื้นดินเป็น INPUT

  pinMode(relay1, OUTPUT);                // ตั้งขารีเลย์ 1 เป็น OUTPUT
  pinMode(relay2, OUTPUT);                // ตั้งขารีเลย์ 2 เป็น OUTPUT
  pinMode(relay3, OUTPUT);                // ตั้งขารีเลย์ 3 เป็น OUTPUT
  pinMode(relay4, OUTPUT);                // ตั้งขารีเลย์ 4 เป็น OUTPUT

  digitalWrite(relay1, LOW);              // กำหนดให้รีเลย์ 1 ปิดเริ่มต้น (ขึ้นอยู่กับโมดูลว่า Active-LOW/Active-HIGH)
  digitalWrite(relay2, LOW);              // กำหนดให้รีเลย์ 2 ปิดเริ่มต้น
  digitalWrite(relay3, LOW);              // กำหนดให้รีเลย์ 3 ปิดเริ่มต้น
  digitalWrite(relay4, LOW);              // กำหนดให้รีเลย์ 4 ปิดเริ่มต้น

  lcd.begin();                            // เริ่มต้นการทำงานของ LCD (สำหรับไลบรารีนี้ใช้ begin() แบบไม่ใส่พารามิเตอร์)
  lcd.backlight();                        // เปิดไฟพื้นหลัง LCD
  lcd.clear();                            // ล้างหน้าจอ
  lcd.setCursor(0, 0);                    // ย้ายเคอร์เซอร์ไปคอลัมน์ 0 แถว 0 (บรรทัดบน)
  lcd.print("System Booting...");         // แสดงข้อความบูตระบบ
  delay(500);                             // หน่วงเล็กน้อยให้เห็นข้อความเริ่มต้น
}

void loop() {
  float h = dht.readHumidity();           // อ่านค่าความชื้นอากาศ (%RH) จาก DHT11
  float t = dht.readTemperature();        // อ่านค่าอุณหภูมิ (°C) จาก DHT11

  int soilRaw = analogRead(SOIL_PIN);     // อ่านค่า ADC (0–4095) จากเซ็นเซอร์ความชื้นดิน
  int span = SOIL_DRY_ADC - SOIL_WET_ADC; // คำนวณช่วงระหว่างค่าดินแห้งกับดินเปียก
  if (span <= 0) span = 1;                // ป้องกันหารด้วยศูนย์กรณีคาลิเบรตผิด
  int soilPct = (int)(((float)(SOIL_DRY_ADC - soilRaw) * 100.0f) / (float)span); // map ค่า ADC → % ความชื้นดิน
  soilPct = constrain(soilPct, 0, 100);   // จำกัดค่าให้อยู่ช่วง 0–100%

  Serial.print("Temperature: ");          // พิมพ์หัวข้ออุณหภูมิ
  Serial.print(t); Serial.print(" C | Humidity: ");   // แสดงค่าอุณหภูมิและหน่วย
  Serial.print(h); Serial.print(" % | Soil RAW: ");   // แสดงค่าความชื้นอากาศและต่อด้วยค่า ADC ดิบ
  Serial.print(soilRaw); Serial.print(" | Soil: ");   // แสดงค่า ADC แล้วคั่นด้วย "Soil:"
  Serial.print(soilPct); Serial.println(" %");        // แสดง % ความชื้นดินและขึ้นบรรทัดใหม่
  delay(200);                              // หน่วงเล็กน้อยเพื่อลดภาระการพิมพ์/ความถี่การอ่าน

  // บรรทัดแรก: TEMP + HUMI (ย่อข้อความให้พอดีกับ 16 ตัวอักษร)
  lcd.setCursor(0, 0);                    // เคอร์เซอร์ไปต้นบรรทัดแรก
  lcd.print("T:");                        // ป้ายกำกับอุณหภูมิ
  if (isnan(t)) lcd.print("--.-");        // ถ้าอ่านไม่สำเร็จ แสดง --.-
  else           lcd.print(t, 1);         // แสดงทศนิยม 1 ตำแหน่ง (อ่านง่าย ไม่สั่นมาก)
  lcd.print((char)223); lcd.print("C ");  // แสดงสัญลักษณ์องศา (°) แล้วตามด้วย C และช่องว่าง
  lcd.print("H:");                        // ป้ายกำกับความชื้นอากาศ
  if (isnan(h)) lcd.print("--.-");        // ถ้าอ่านไม่สำเร็จ แสดง --.-
  else           lcd.print(h, 0);         // แสดงเป็นจำนวนเต็ม (0 ตำแหน่ง) ให้พอดี 16 ช่อง
  lcd.print("%");                         // หน่วย %

  // บรรทัดสอง: SOIL
  lcd.setCursor(0, 1);                    // เคอร์เซอร์ไปต้นบรรทัดที่สอง
  lcd.print("Soil:");                     // ป้ายกำกับความชื้นดิน
  lcd.print(soilPct);                     // แสดง % ความชื้นดิน
  lcd.print("%     ");                    // เติมช่องว่างท้ายบรรทัดเพื่อลบเศษตัวอักษรรอบก่อน

  // ทดสอบรีเลย์ (โค้ดเดิม)
  digitalWrite(relay1, LOW);  delay(500); // รีเลย์ 1 → LOW (ขึ้นกับ Active-LOW/HIGH ของบอร์ด)
  digitalWrite(relay1, HIGH); delay(500); // รีเลย์ 1 → HIGH

  digitalWrite(relay2, LOW);  delay(500); // รีเลย์ 2 → LOW
  digitalWrite(relay2, HIGH); delay(500); // รีเลย์ 2 → HIGH

  digitalWrite(relay3, LOW);  delay(500); // รีเลย์ 3 → LOW
  digitalWrite(relay3, HIGH); delay(500); // รีเลย์ 3 → HIGH

  digitalWrite(relay4, LOW);  delay(500); // รีเลย์ 4 → LOW
  digitalWrite(relay4, HIGH); delay(500); // รีเลย์ 4 → HIGH

  digitalWrite(relay1, LOW);              // ปิดรีเลย์ทั้งหมด (หรือเปิด ถ้า Active-LOW)
  digitalWrite(relay2, LOW);
  digitalWrite(relay3, LOW);
  digitalWrite(relay4, LOW);
  delay(500);                             // เว้นระยะให้ดูการเปลี่ยนสถานะได้

  digitalWrite(relay1, HIGH);             // เปิดรีเลย์ทั้งหมด (หรือปิด ถ้า Active-LOW)
  digitalWrite(relay2, HIGH);
  digitalWrite(relay3, HIGH);
  digitalWrite(relay4, HIGH);
  delay(500);                             // เว้นระยะก่อนวนรอบใหม่
}

#include <Wire.h>                          // ไลบรารีสำหรับสื่อสาร I2C ระหว่าง ESP32 กับอุปกรณ์ I2C (เช่น LCD)
#include <LiquidCrystal_I2C.h>             // ไลบรารีควบคุมจอ LCD ผ่าน I2C
//Download ได้จาก https://drive.google.com/file/d/1hkmODw9q8CdxQx99Bp1Eu2foACQ-C4Sx/view?usp=sharing
LiquidCrystal_I2C lcd(0x27, 16, 2);       // สร้างออบเจกต์ LCD ที่แอดเดรส 0x27 ขนาด 16 คอลัมน์ 2 แถว (บางจออาจเป็น 0x3F)

//dht11,dht22
#include <DHT.h>                           // ไลบรารี Adafruit DHT สำหรับอ่านอุณหภูมิ/ความชื้นอากาศ
#define DHTPIN 15                          // ขา DATA ของ DHT ต่อกับ GPIO15 ของ ESP32
#define DHTTYPE DHT11                      // เลือกชนิดเซ็นเซอร์เป็น DHT11 (หากใช้ DHT22 ให้สลับบรรทัดที่คอมเมนต์ด้านล่าง)
/// #define DHTTYPE DHT22                   // (ตัวเลือก) ใช้เมื่อเปลี่ยนเซ็นเซอร์เป็น DHT22
DHT dht(DHTPIN, DHTTYPE);                  // สร้างอ็อบเจ็กต์ dht เพื่อเรียก readHumidity()/readTemperature()

void setup() {
  Serial.begin(9600);                      // เริ่ม Serial Monitor ที่ 9600 bps สำหรับดีบัก/แสดงค่า
  dht.begin();                             // เริ่มทำงานเซ็นเซอร์ DHT (ต้องเรียกก่อนอ่านค่า)

  lcd.begin();                             // เริ่มต้นการทำงานของจอ LCD (สำหรับไลบรารีนี้ใช้ begin() แบบไม่ใส่พารามิเตอร์)
  lcd.backlight();                         // เปิดไฟพื้นหลังของ LCD
  lcd.clear();                             // ล้างหน้าจอให้สะอาด
  lcd.setCursor(0, 0);                     // ย้ายเคอร์เซอร์ไปต้นบรรทัดแรก (คอลัมน์ 0 แถว 0)
  lcd.print("System Ready");               // แสดงข้อความสถานะเริ่มต้น
  lcd.setCursor(0, 1);                     // ย้ายเคอร์เซอร์ไปบรรทัดที่สอง
  lcd.print("DHT11+LCD1602");            // แสดงข้อความอธิบายอุปกรณ์
  delay(500);                              // หน่วงเล็กน้อยเพื่อให้เห็นข้อความบูต
}

void loop() {
  float h = dht.readHumidity();            // อ่านค่าความชื้นอากาศ (%RH) จาก DHT
  float t = dht.readTemperature();         // อ่านค่าอุณหภูมิ (°C) จาก DHT (ถ้าต้องการ °F ใช้ readTemperature(true))

  if (isnan(h) || isnan(t)) {              // ตรวจสอบหากอ่านค่าไม่ได้ (จะได้ค่า NaN)
    Serial.println("ไม่สามารถอ่านค่าเซ็นเซอร์ได้"); // แจ้งเตือนทาง Serial
    // แสดงข้อความผิดพลาดบนจอ เพื่อให้เห็นสถานะที่อุปกรณ์
    lcd.setCursor(0, 0);                   // ตำแหน่งบรรทัดแรก
    lcd.print("DHT Read Error   ");        // พิมพ์ข้อความและเติมช่องว่างท้ายบรรทัดเพื่อเคลียร์เศษตัวอักษร
    lcd.setCursor(0, 1);                   // ตำแหน่งบรรทัดสอง
    lcd.print("Check wiring!    ");        // แจ้งเช็กการต่อสาย/ไฟเลี้ยง
    delay(1000);                           // หน่วงสั้น ๆ แล้วค่อยวนอ่านใหม่
    return;                                // ออกจาก loop รอบนี้
  }

  // ----- แสดงผลทาง Serial -----
  Serial.print("Temperature: ");           // พิมพ์หัวข้ออุณหภูมิ
  Serial.print(t, 1);                      // แสดงค่าอุณหภูมิทศนิยม 1 ตำแหน่ง
  Serial.print(" *C  |  Humidity: ");      // หน่วยองศาเซลเซียส และคั่นด้วย |
  Serial.print(h, 1);                      // แสดงค่าความชื้นทศนิยม 1 ตำแหน่ง
  Serial.println(" %");                    // หน่วย %

  // ----- แสดงผลบน LCD 16x2 -----
  lcd.setCursor(0, 0);                     // ตั้งเคอร์เซอร์บรรทัดแรก
  lcd.print("TEMP: ");                     // ป้ายกำกับอุณหภูมิ
  lcd.print(t, 1);                         // แสดงอุณหภูมิทศนิยม 1 ตำแหน่ง
  lcd.print((char)223);                    // สัญลักษณ์องศา (°) สำหรับจอ HD44780 (ASCII 223)
  lcd.print("C   ");                       // พิมพ์ 'C' และช่องว่างเพื่อลบเศษตัวอักษรเดิม

  lcd.setCursor(0, 1);                     // ตั้งเคอร์เซอร์บรรทัดที่สอง
  lcd.print("HUMI: ");                     // ป้ายกำกับความชื้น
  lcd.print(h, 1);                         // แสดงค่าความชื้นทศนิยม 1 ตำแหน่ง
  lcd.print("%   ");                       // แสดง '%' และช่องว่างเพื่อลบเศษตัวอักษรเดิม

  delay(1000);                             // หน่วง 1 วินาที (DHT11 ควรอ่านไม่ถี่กว่า ~1–2 วินาที)
}

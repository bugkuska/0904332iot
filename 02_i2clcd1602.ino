#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// สร้างอ็อบเจ็กต์ LCD: (I2C Address, Columns, Rows)
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  lcd.begin();              // เริ่มการทำงานของจอ LCD
  lcd.backlight();          // เปิดไฟแบ็คไลท์

  lcd.setCursor(0, 0);      // ตั้งตำแหน่งเริ่มต้น (คอลัมน์, แถว)
  lcd.print("Hello ESP32!"); // แสดงข้อความในแถวที่ 1

  lcd.setCursor(0, 1);      // ย้ายไปแถวที่ 2
  lcd.print("LCD 1602 I2C");
}

void loop() {
  // เพิ่มโค้ดเพิ่มเติมหากต้องการใน loop
}

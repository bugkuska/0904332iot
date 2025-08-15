#include <Wire.h>                 // ไลบรารีสำหรับสื่อสารแบบ I2C (ใช้กับ LCD และอุปกรณ์ I2C อื่น ๆ)
#include <LiquidCrystal_I2C.h>    // ไลบรารีสำหรับควบคุมจอ LCD ที่ต่อผ่านโมดูล I2C
//Download ได้จาก https://drive.google.com/file/d/1hkmODw9q8CdxQx99Bp1Eu2foACQ-C4Sx/view?usp=sharing

// สร้างอ็อบเจ็กต์ LCD: (I2C Address, Columns, Rows)
LiquidCrystal_I2C lcd(0x27, 16, 2); // สร้างตัวแปร lcd โดยกำหนดแอดเดรส 0x27, ขนาดจอ 16 คอลัมน์ 2 แถว
                                    // (แอดเดรส I2C อาจเปลี่ยนเป็น 0x3F ในบางโมดูล)

void setup() {
  lcd.begin();              // เริ่มต้นการทำงานของจอ LCD (ตั้งค่าการสื่อสารและโหมดภายใน)
  lcd.backlight();          // เปิดไฟพื้นหลัง (Backlight) ของจอ LCD

  lcd.setCursor(0, 0);      // กำหนดตำแหน่งเคอร์เซอร์ไปที่คอลัมน์ 0 แถว 0 (มุมซ้ายบน)
  lcd.print("Hello ESP32!"); // พิมพ์ข้อความ "Hello ESP32!" ลงบรรทัดแรก

  lcd.setCursor(0, 1);      // ย้ายตำแหน่งเคอร์เซอร์ไปที่คอลัมน์ 0 แถว 1 (บรรทัดที่ 2)
  lcd.print("LCD 1602 I2C"); // พิมพ์ข้อความ "LCD 1602 I2C" ลงบรรทัดที่ 2
}

void loop() {
  // วนลูปทำงานซ้ำ ๆ หลังจาก setup() จบ
  // (ในโค้ดนี้ไม่มีคำสั่งใน loop ดังนั้นจอ LCD จะแสดงข้อความเดิมค้างไว้)
}

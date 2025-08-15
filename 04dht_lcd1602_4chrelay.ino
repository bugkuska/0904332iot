#include <Wire.h>                         // ไลบรารีสำหรับการสื่อสารผ่าน I2C
#include <LiquidCrystal_I2C.h>            // ไลบรารีควบคุมจอ LCD แบบ I2C
// ดาวน์โหลดไลบรารีได้จาก: https://drive.google.com/file/d/1hkmODw9q8CdxQx99Bp1Eu2foACQ-C4Sx/view?usp=sharing

LiquidCrystal_I2C lcd(0x27, 16, 2);       // กำหนดออบเจกต์ lcd ที่ Address I2C = 0x27, จอขนาด 16 คอลัมน์ 2 แถว

#define relay1 4                          // ขารีเลย์ 1 (GPIO4)
#define relay2 5                          // ขารีเลย์ 2 (GPIO5)
#define relay3 18                         // ขารีเลย์ 3 (GPIO18)
#define relay4 19                         // ขารีเลย์ 4 (GPIO19)

#include <DHT.h>                          // ไลบรารี DHT สำหรับอ่านอุณหภูมิและความชื้น
#define DHTPIN 15                         // ขา DATA ของ DHT11 (GPIO15)
#define DHTTYPE DHT11                     // ประเภทของเซ็นเซอร์ (DHT11)
DHT dht(DHTPIN, DHTTYPE);                 // สร้างออบเจกต์ dht สำหรับใช้งาน

void setup() {
  Serial.begin(9600);                     // เปิด Serial Monitor ที่ความเร็ว 9600 bps
  dht.begin();                            // เริ่มการทำงานของเซ็นเซอร์ DHT11

  // กำหนดขารีเลย์เป็น Output
  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);
  pinMode(relay3, OUTPUT);
  pinMode(relay4, OUTPUT);

  // ตั้งค่ารีเลย์ให้ปิดตอนเริ่มต้น (ขึ้นอยู่กับ Active-LOW หรือ Active-HIGH ของโมดูล)
  digitalWrite(relay1, LOW);
  digitalWrite(relay2, LOW);
  digitalWrite(relay3, LOW);
  digitalWrite(relay4, LOW);

  // เริ่มต้นใช้งานจอ LCD
  lcd.begin();
  lcd.backlight();                        // เปิดไฟพื้นหลัง
  lcd.clear();                            // ล้างหน้าจอ
  lcd.setCursor(0, 0);                    // วางเคอร์เซอร์ที่บรรทัดแรก ตำแหน่งแรก
  lcd.print("System Booting...");         // แสดงข้อความบูตระบบ
  delay(500);                             // หน่วงเวลาให้เห็นข้อความ
}

void loop() {
  // อ่านค่าจากเซ็นเซอร์ DHT11
  float h = dht.readHumidity();           // อ่านความชื้นอากาศ (%RH)
  float t = dht.readTemperature();        // อ่านอุณหภูมิ (องศา C)

  // แสดงข้อมูลผ่าน Serial Monitor
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" C | Humidity: ");
  Serial.print(h);
  Serial.println(" %");
  delay(200);                              // หน่วงเล็กน้อยเพื่อลดภาระการอ่านค่า

  // แสดงบรรทัดแรกของ LCD → อุณหภูมิ + ความชื้น
  lcd.setCursor(0, 0);                    // ไปตำแหน่งแรกของบรรทัดบน
  lcd.print("T:");
  if (isnan(t)) lcd.print("--.-");        // ถ้าอ่านค่าไม่ได้ให้แสดง "--.-"
  else lcd.print(t, 1);                   // แสดงอุณหภูมิทศนิยม 1 ตำแหน่ง
  lcd.print((char)223); lcd.print("C ");  // แสดงสัญลักษณ์องศาและตัว C

  lcd.print("H:");
  if (isnan(h)) lcd.print("--.-");        // ถ้าอ่านค่าไม่ได้ให้แสดง "--.-"
  else lcd.print(h, 0);                   // แสดงความชื้นแบบจำนวนเต็ม
  lcd.print("%");

  // บรรทัดที่สอง สามารถใช้แสดงข้อความสถานะรีเลย์
  lcd.setCursor(0, 1);
  lcd.print("Relays Test...  ");

  // ทดสอบการเปิด/ปิดรีเลย์ทั้งหมดทีละตัว
  digitalWrite(relay1, LOW);  delay(500);
  digitalWrite(relay1, HIGH); delay(500);

  digitalWrite(relay2, LOW);  delay(500);
  digitalWrite(relay2, HIGH); delay(500);

  digitalWrite(relay3, LOW);  delay(500);
  digitalWrite(relay3, HIGH); delay(500);

  digitalWrite(relay4, LOW);  delay(500);
  digitalWrite(relay4, HIGH); delay(500);

  // ปิดรีเลย์ทั้งหมด
  digitalWrite(relay1, LOW);
  digitalWrite(relay2, LOW);
  digitalWrite(relay3, LOW);
  digitalWrite(relay4, LOW);
  delay(500);

  // เปิดรีเลย์ทั้งหมด
  digitalWrite(relay1, HIGH);
  digitalWrite(relay2, HIGH);
  digitalWrite(relay3, HIGH);
  digitalWrite(relay4, HIGH);
  delay(500);
}

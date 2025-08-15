#include <DHT.h>                 // ใช้ไลบรารี Adafruit DHT

// ------------------ ตั้งค่าเซ็นเซอร์ ------------------
#define DHTPIN   15              // ขาที่เชื่อมต่อ DATA ของ DHT11
#define DHTTYPE  DHT11           // ประเภทเซ็นเซอร์ (เปลี่ยนเป็น DHT22 ได้หากใช้รุ่นนั้น)
DHT dht(DHTPIN, DHTTYPE);        // สร้างอ็อบเจกต์เซ็นเซอร์

// ระยะห่างการอ่าน (มิลลิวินาที) — DHT11 แนะนำ ≥ 2000 ms
const unsigned long READ_INTERVAL_MS = 2000;

void setup() {
  Serial.begin(9600);                          // ความเร็ว Serial สูงขึ้น อ่านง่าย ลื่นกว่า
  dht.begin();                                    // เริ่มทำงานเซ็นเซอร์
  Serial.println(F("เริ่มต้น DHT... พร้อมอ่านค่าทุก 2 วินาที"));
}

void loop() {
  // รอเวลาให้ถึงรอบอ่านถัดไป (บล็อกแบบง่าย ๆ)
  delay(READ_INTERVAL_MS);

  // อ่านค่า
  float h = dht.readHumidity();                   // ความชื้นสัมพัทธ์ (%RH)
  float t = dht.readTemperature();                // อุณหภูมิองศาเซลเซียส
  // float f = dht.readTemperature(true);         // (ตัวเลือก) องศาฟาเรนไฮต์

  // ตรวจสอบค่าที่อ่านได้
  if (isnan(h) || isnan(t)) {
    Serial.println(F("อ่านค่าเซ็นเซอร์ไม่สำเร็จ (NaN) — ตรวจสาย/ไฟเลี้ยง/ค่า DHTTYPE"));
    return;                                       // ออกจาก loop รอบนี้ แล้วรอรอบถัดไป
  }

  // แสดงผล
  Serial.print(F("อุณหภูมิ: "));
  Serial.print(t, 1);                             // ทศนิยม 1 ตำแหน่ง อ่านง่าย
  Serial.print(F(" \xC2\xB0""C"));                // สัญลักษณ์องศา °C (พิมพ์ได้ใน Serial)
  Serial.print(F("  |  ความชื้น: "));
  Serial.print(h, 1);
  Serial.println(F(" %RH"));
}

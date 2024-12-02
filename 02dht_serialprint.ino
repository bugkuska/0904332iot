#include "DHT.h"

#define DHTPIN 4        // กำหนดขาที่เชื่อมต่อกับเซ็นเซอร์ DHT11/DHT22
#define DHTTYPE DHT11   // เลือกประเภทเซ็นเซอร์ DHT11 หรือ DHT22 (ใช้ DHT11 ในที่นี้)

DHT dht(DHTPIN, DHTTYPE); // สร้างอ็อบเจ็กต์ DHT

void setup() {
  Serial.begin(115200);    // เริ่มต้นการสื่อสารผ่าน Serial Monitor
  dht.begin();             // เริ่มการทำงานของเซ็นเซอร์ DHT
}

void loop() {
  // รอเวลาให้เซ็นเซอร์พร้อม
  delay(2000);

  // อ่านค่าอุณหภูมิและความชื้น
  float h = dht.readHumidity();        // อ่านค่าความชื้น
  float t = dht.readTemperature();     // อ่านค่าอุณหภูมิ (ในเซลเซียส)

  // ตรวจสอบค่าที่อ่านได้
  if (isnan(h) || isnan(t)) {
    Serial.println("ไม่สามารถอ่านค่าเซ็นเซอร์ได้");
    return;
  }

  // แสดงค่าที่อ่านได้
  Serial.print("อุณหภูมิ: ");
  Serial.print(t);
  Serial.print(" *C");
  Serial.print("  ความชื้น: ");
  Serial.print(h);
  Serial.println(" %");
}

#include <Wire.h> // ไลบรารีสำหรับการสื่อสาร I2C

void setup() {
  Wire.begin();             // เริ่มต้นการสื่อสาร I2C (ESP32 จะใช้ SDA=21, SCL=22 เป็นค่าเริ่มต้น)
  Serial.begin(9600);     // เริ่มการสื่อสารผ่าน Serial Monitor
  Serial.println("\nI2C Scanner");
}

void loop() {
  byte error, address;      // ตัวแปรเก็บรหัสข้อผิดพลาดและที่อยู่ I2C
  int count = 0;            // ตัวนับจำนวนอุปกรณ์ที่พบ

  Serial.println("Scanning...");

  for (address = 1; address < 127; address++) { // ไล่ตรวจสอบที่อยู่ตั้งแต่ 1 ถึง 126
    Wire.beginTransmission(address);            // เริ่มส่งข้อมูลไปยังที่อยู่
    error = Wire.endTransmission();             // จบการส่งและรับค่าผลลัพธ์

    if (error == 0) { // ถ้าไม่มีข้อผิดพลาด แสดงว่ามีอุปกรณ์ตอบสนอง
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0"); // ให้พิมพ์เลข 0 ด้านหน้า ถ้าต่ำกว่า 0x10
      Serial.print(address, HEX);          // แสดงที่อยู่ในรูปแบบเลขฐาน 16
      Serial.println(" !");
      count++;
    }
    else if (error == 4) { // error 4 หมายถึงมีปัญหาในการติดต่อ
      Serial.print("Unknown error at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }

  if (count == 0) Serial.println("No I2C devices found\n");
  else Serial.println("Done\n");

  delay(5000); // รอสัก 5 วินาทีก่อนสแกนใหม่
}

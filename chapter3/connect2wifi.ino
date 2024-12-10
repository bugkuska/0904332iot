#include <WiFi.h>

// กำหนดข้อมูล Wi-Fi
const char* ssid = "Sirindhorn_Floor_2-4";      // ชื่อ Wi-Fi
const char* password = "";        // รหัสผ่าน Wi-Fi

// กำหนดหมายเลขขาของ GPIO
const int pinIO2 = 2; // GPIO2

void setup() {
  // เปิด Serial Monitor
  Serial.begin(9600);
  Serial.println("เริ่มต้นการเชื่อมต่อ Wi-Fi");

  // ตั้งค่า GPIO2 เป็น Output
  pinMode(pinIO2, OUTPUT);
  digitalWrite(pinIO2, LOW); // ปิด IO2 เริ่มต้น

  // เชื่อมต่อ Wi-Fi
  WiFi.begin(ssid, password);

  // รอการเชื่อมต่อ
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("กำลังเชื่อมต่อ...");
  }

  // แสดงข้อมูลเมื่อเชื่อมต่อสำเร็จ
  Serial.println("เชื่อมต่อสำเร็จ!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP()); // แสดง IP Address ที่ได้รับ

  // เปิด GPIO2 หลังจากเชื่อมต่อสำเร็จ
  digitalWrite(pinIO2, HIGH);
  Serial.println("เปิด IO2 แล้ว");
}

void loop() {
  // โค้ดหลัก (ถ้ามี)
}

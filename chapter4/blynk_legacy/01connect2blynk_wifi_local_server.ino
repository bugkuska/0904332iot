#define BLYNK_PRINT Serial                 // ให้ Blynk ส่งข้อความ Debug ผ่าน Serial Monitor
#include <WiFi.h>                          // ไลบรารีสำหรับการเชื่อมต่อ Wi-Fi (ESP32)
#include <WiFiClient.h>                    // ไลบรารีสำหรับการสร้าง client เชื่อมต่อ TCP/IP
#include <BlynkSimpleEsp32.h>               // ไลบรารี Blynk สำหรับ ESP32 (ต้องใช้เวอร์ชัน 0.6.x เท่านั้น)

// ข้อมูล Wi-Fi และ Blynk
const char ssid[] = "Your_SSID";           // กำหนดชื่อ Wi-Fi ที่จะเชื่อมต่อ
const char pass[] = "Your_PASSWORD";       // กำหนดรหัสผ่าน Wi-Fi
const char auth[] = "Your_AUTH_TOKEN";     // Token จากแอป Blynk (ระบุใน Blynk Project)

// กำหนดขา GPIO สำหรับ LED
#define LED_PIN 2                           // ขา GPIO2 ใช้ควบคุม LED

// ตัวแปร flag ใช้เพื่อสั่งดึงค่าจาก Blynk ครั้งแรกเมื่อเชื่อมต่อ
bool fetchBlynkState = true;

// ตัวตั้งเวลาใน Blynk สำหรับทำงานตามรอบเวลา
BlynkTimer timer;

// ประกาศฟังก์ชันล่วงหน้า
void checkConnections();                    // ฟังก์ชันตรวจสอบการเชื่อมต่อ Wi-Fi และ Blynk

void setup() {
  Serial.begin(9600);                       // เริ่ม Serial Monitor ที่ความเร็ว 9600 bps

  pinMode(LED_PIN, OUTPUT);                  // ตั้งค่า LED_PIN เป็น Output
  digitalWrite(LED_PIN, LOW);                // ปิด LED เริ่มต้น

  Serial.print("Connecting to Wi-Fi: ");     // แสดงข้อความกำลังเชื่อมต่อ Wi-Fi
  Serial.println(ssid);                      // แสดงชื่อ Wi-Fi
  WiFi.begin(ssid, pass);                     // เริ่มเชื่อมต่อ Wi-Fi

  while (WiFi.status() != WL_CONNECTED) {    // รอจนกว่าจะเชื่อมต่อสำเร็จ
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");         // แจ้งว่าเชื่อมต่อสำเร็จ
  Serial.print("IP Address: ");               // แสดง IP ที่ได้รับจาก Router
  Serial.println(WiFi.localIP());

  WiFi.setAutoReconnect(true);                // ตั้งค่าให้เชื่อมต่อ Wi-Fi ใหม่อัตโนมัติเมื่อหลุด
  WiFi.persistent(true);                      // บันทึกค่า Wi-Fi ลงหน่วยความจำถาวร

  Serial.println("Connecting to Blynk server...");  
  Blynk.begin(auth, ssid, pass, "ip-address-blynk-localserver", 8080); // เชื่อมต่อ Blynk Server (Local Server)

  timer.setInterval(5000L, checkConnections); // สั่งให้ตรวจสอบการเชื่อมต่อทุก 5 วินาที
}

// ฟังก์ชัน callback เมื่อเชื่อมต่อ Blynk สำเร็จ
BLYNK_CONNECTED() {
  Serial.println("Blynk connected!");
  
  if (fetchBlynkState) {                     // ถ้าเป็นการเชื่อมต่อครั้งแรก
    digitalWrite(LED_PIN, HIGH);              // เปิด LED
    Blynk.syncAll();                          // ดึงค่าล่าสุดของทุก Virtual Pin จาก Server
  }
}

// ฟังก์ชัน callback เมื่อมีข้อมูลจาก Blynk มาที่ Virtual Pin V0
BLYNK_WRITE(V0) {
  int ledState = param.asInt();               // อ่านค่าที่ส่งมาจากแอป (0 หรือ 1)
  digitalWrite(LED_PIN, ledState);            // สั่งเปิด/ปิด LED ตามค่าที่รับมา
  Serial.print("LED state set to: ");         
  Serial.println(ledState ? "ON" : "OFF");    // แสดงข้อความสถานะ LED
}

// ฟังก์ชันตรวจสอบการเชื่อมต่อ Wi-Fi และ Blynk
void checkConnections() {
  if (WiFi.status() != WL_CONNECTED) {        // ถ้า Wi-Fi หลุด
    Serial.println("Wi-Fi disconnected! Reconnecting...");
    WiFi.begin(ssid, pass);                   // พยายามเชื่อมต่อใหม่
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\nWi-Fi reconnected");    // แจ้งว่าเชื่อมต่อ Wi-Fi กลับมาแล้ว
  }

  if (!Blynk.connected()) {                   // ถ้า Blynk หลุด
    Serial.println("Blynk disconnected! Reconnecting...");
    Blynk.connect();                          // พยายามเชื่อมต่อ Blynk ใหม่
  }
}

void loop() {
  if (Blynk.connected()) {                    // ถ้าเชื่อมต่อ Blynk อยู่
    Blynk.run();                              // ให้ Blynk ทำงาน
  }
  timer.run();                                // ให้ Timer ทำงานตามรอบเวลา
}

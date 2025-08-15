#define BLYNK_PRINT Serial                               // ให้ไลบรารี Blynk พิมพ์ข้อความดีบักออก Serial
#include <WiFi.h>                                        // ไลบรารี Wi-Fi สำหรับ ESP32
#include <WiFiClient.h>                                  // ไลบรารี client TCP/IP (ถูกใช้โดย Blynk)
#include <BlynkSimpleEsp32.h>                            // ไลบรารี Blynk สำหรับ ESP32 (แนะนำใช้เวอร์ชัน 0.6.x)
#include <DHT.h>                                         // ไลบรารีเซ็นเซอร์ DHT (Adafruit)

// -------- Wi-Fi และ Blynk credentials --------
const char ssid[] = "";                                  // ชื่อเครือข่าย Wi-Fi (ใส่ของคุณ)
const char pass[] = "";                                  // รหัสผ่าน Wi-Fi (ใส่ของคุณ)
const char auth[] = "";                                  // Auth Token จากแอป Blynk (โครงการของคุณ)

// -------- กำหนดขา GPIO --------
#define LED_PIN 2                                        // ใช้ GPIO2 ควบคุม LED (หรือ LED บนบอร์ด ESP32)
#define DHTPIN 15                                        // GPIO15 ต่อกับขา DATA ของ DHT
#define DHTTYPE DHT11                                    // ประเภทเซ็นเซอร์ DHT (เปลี่ยนเป็น DHT22 ได้หากใช้รุ่นนั้น)

// -------- สร้างอินสแตนซ์ DHT --------
DHT dht(DHTPIN, DHTTYPE);                                // สร้างออบเจกต์ DHT เพื่อนำไปอ่านอุณหภูมิ/ความชื้น

// -------- ตัวตั้งเวลา (ภายในไลบรารี Blynk) --------
BlynkTimer timer;                                        // ใช้ตั้งงานทำซ้ำตามช่วงเวลา (ไม่บล็อก loop)

// -------- ประกาศต้นแบบฟังก์ชัน --------
void checkConnections();                                  // เช็กสถานะการเชื่อมต่อ Wi-Fi และ Blynk
void readDHTSensor();                                     // อ่านค่าจากเซ็นเซอร์ DHT แล้วส่งไป Blynk

void setup() {
  Serial.begin(9600);                                     // เปิด Serial Monitor ที่ 9600 bps สำหรับดีบัก

  pinMode(LED_PIN, OUTPUT);                               // ตั้งโหมดขา LED เป็นเอาต์พุต
  digitalWrite(LED_PIN, LOW);                             // ปิด LED เป็นค่าเริ่มต้น

  dht.begin();                                            // เริ่มต้นการทำงานของเซ็นเซอร์ DHT

  Serial.print("Connecting to Wi-Fi: ");                  // แสดงว่ากำลังเชื่อมต่อ Wi-Fi
  Serial.println(ssid);                                   // พิมพ์ชื่อ Wi-Fi ที่จะเชื่อม
  WiFi.begin(ssid, pass);                                 // เริ่มเชื่อมต่อ Wi-Fi ด้วย SSID/PASSWORD

  while (WiFi.status() != WL_CONNECTED) {                 // รอจนกว่าจะเชื่อมต่อสำเร็จ
    delay(500);                                           // หน่วงสั้น ๆ ระหว่างรอ
    Serial.print(".");                                    // พิมพ์จุดแสดงความคืบหน้า
  }
  Serial.println("\nWi-Fi connected");                    // แจ้งว่าเชื่อมต่อ Wi-Fi แล้ว
  Serial.print("IP Address: ");                           // พิมพ์หัวข้อ IP
  Serial.println(WiFi.localIP());                         // แสดง IP ที่ได้รับจากเราเตอร์

  WiFi.setAutoReconnect(true);                            // ให้เชื่อมต่อ Wi-Fi ใหม่อัตโนมัติเมื่อหลุด
  WiFi.persistent(true);                                  // บันทึกค่าการเชื่อมต่อไว้ในหน่วยความจำถาวร

  Serial.println("Connecting to Blynk server...");        // แจ้งว่ากำลังเชื่อมต่อ Blynk Server (โลคัล)
  Blynk.begin(auth, ssid, pass, "ip-address", 8080);     // เชื่อมต่อ Blynk ด้วย Token/SSID/PASS/เซิร์ฟเวอร์/พอร์ต

  timer.setInterval(2000L, readDHTSensor);                // ตั้งให้อ่าน DHT ทุก ๆ 2000 ms (เหมาะกับ DHT11)
  timer.setInterval(5000L, checkConnections);             // ตั้งให้ตรวจการเชื่อมต่อทุก 5000 ms
}

// -------- ถูกเรียกอัตโนมัติเมื่อเชื่อม Blynk สำเร็จ --------
BLYNK_CONNECTED() {
  Serial.println("Blynk connected!");                     // แจ้งสถานะเชื่อมต่อ Blynk แล้ว
  Blynk.syncAll();                                        // ขอซิงค์สถานะ Virtual Pins ทั้งหมดจากเซิร์ฟเวอร์

  digitalWrite(LED_PIN, HIGH);                            // ตัวอย่าง: บังคับเปิด LED หลังเชื่อมต่อสำเร็จ
  Serial.println("LED state set to: ON (forced after connection)"); // พิมพ์สถานะ LED
}

// -------- คอลแบ็กเมื่อมีการเขียนค่าไปยัง Virtual Pin V0 --------
BLYNK_WRITE(V0) {
  int ledState = param.asInt();                           // อ่านค่าจากแอป Blynk (0 = OFF, 1 = ON)
  digitalWrite(LED_PIN, ledState);                        // สั่ง LED ตามค่าที่ได้รับ
  Serial.print("LED state set to: ");                     // พิมพ์สถานะ LED
  Serial.println(ledState ? "ON" : "OFF");                // แสดงเป็นข้อความอ่านง่าย
}

// -------- อ่านค่าจาก DHT แล้วส่งขึ้น Blynk --------
void readDHTSensor() {
  float temperature = dht.readTemperature();              // อ่านอุณหภูมิ (°C)
  float humidity    = dht.readHumidity();                 // อ่านความชื้นสัมพัทธ์ (%RH)

  if (isnan(temperature) || isnan(humidity)) {            // ถ้าอ่านไม่ได้ (NaN) ให้แจ้งเตือน
    Serial.println("Failed to read from DHT sensor!");    // พิมพ์แจ้งข้อผิดพลาด
    return;                                               // ยกเลิกรอบนี้ ไม่ส่งค่าขึ้น Blynk
  }

  Serial.print("Temperature: ");                          // พิมพ์หัวข้ออุณหภูมิ
  Serial.print(temperature);                              // พิมพ์ค่าอุณหภูมิ
  Serial.print("°C, Humidity: ");                         // หน่วย °C และหัวข้อความชื้น
  Serial.print(humidity);                                 // พิมพ์ค่าความชื้น
  Serial.println("%");                                    // หน่วย %

  Blynk.virtualWrite(V1, temperature);                    // ส่งค่าอุณหภูมิไปยัง Virtual Pin V1
  Blynk.virtualWrite(V2, humidity);                       // ส่งค่าความชื้นไปยัง Virtual Pin V2
}

// -------- ตรวจสอบการเชื่อมต่อ Wi-Fi / Blynk และกู้คืนเมื่อหลุด --------
void checkConnections() {
  if (WiFi.status() != WL_CONNECTED) {                    // ถ้า Wi-Fi หลุด
    Serial.println("Wi-Fi disconnected! Reconnecting...");// พิมพ์แจ้งเตือน
    WiFi.begin(ssid, pass);                               // เริ่มเชื่อมต่อใหม่
    while (WiFi.status() != WL_CONNECTED) {               // รอจนกว่าจะเชื่อมต่อสำเร็จ
      delay(500);                                         // หน่วงสั้น ๆ
      Serial.print(".");                                  // พิมพ์จุดแสดงความคืบหน้า
    }
    Serial.println("\nWi-Fi reconnected");                // แจ้งว่าเชื่อม Wi-Fi กลับมาแล้ว
  }

  if (!Blynk.connected()) {                               // ถ้า Blynk หลุด
    Serial.println("Blynk disconnected! Reconnecting...");// แจ้งเตือน
    Blynk.connect();                                      // ขอเชื่อมต่อ Blynk ใหม่
  }
}

void loop() {
  if (Blynk.connected()) {                                // ทำงาน Blynk เฉพาะเมื่อเชื่อมต่ออยู่
    Blynk.run();                                          // ประมวลผลงานภายในของ Blynk (คอลแบ็ก/ซิงค์)
  }
  timer.run();                                            // ให้ตัวตั้งเวลาทำงาน (เรียกฟังก์ชันตามกำหนด)
}

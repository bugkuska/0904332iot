#include <Wire.h>              // ใช้สำหรับการสื่อสาร I2C
#include <WiFi.h>              // ใช้สำหรับการเชื่อมต่อ Wi-Fi
#include <WiFiClient.h>        // ใช้สำหรับการเชื่อมต่อ Wi-Fi Client
#include <BlynkSimpleEsp32.h>  // ใช้สำหรับการเชื่อมต่อกับ Blynk Server *ให้ใช้ Blynk version 0.60 เท่านั้น
#include <InfluxDbClient.h>    // ใช้ส่งข้อมูลไปยัง InfluxDB
#include <InfluxDbCloud.h>     // ไลบรารีช่วยเชื่อมต่อ InfluxDB Cloud
#include <DHT.h>               // ไลบรารีสำหรับเซ็นเซอร์ DHT (ตรวจวัดอุณหภูมิและความชื้น)

// Wi-Fi and Blynk credentials
const char ssid[] = "";                         // ชื่อ Wi-Fi
const char pass[] = "";                        // รหัสผ่าน Wi-Fi
const char auth[] = ";  // Auth Token ของ Blynk

// InfluxDB Configuration
// URL ของ InfluxDB Server ที่ใช้เก็บข้อมูลเซ็นเซอร์
#define INFLUXDB_URL "http://แก้ ip address:8086"
// Token สำหรับการตรวจสอบสิทธิ์ในการเข้าถึง InfluxDB (ควรเก็บเป็นความลับ)
#define INFLUXDB_TOKEN ""
// ชื่อองค์กร (Organization) ที่กำหนดใน InfluxDB
#define INFLUXDB_ORG ""
// ชื่อ Bucket ที่ใช้เก็บข้อมูลใน InfluxDB (เป็นที่เก็บข้อมูลเซ็นเซอร์ DHT)
#define INFLUXDB_BUCKET ""
// สร้าง Client สำหรับเชื่อมต่อกับ InfluxDB โดยใช้ข้อมูลการตั้งค่าด้านบน
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point sensor("environment");  // สร้าง Measurement ชื่อ "environment"

// GPIO Configuration
#define LED_PIN 2      // ขาที่ใช้ควบคุม LED
#define DHTPIN 15      // ขาที่เชื่อมต่อกับเซ็นเซอร์ DHT
#define DHTTYPE DHT22  // กำหนดประเภทเซ็นเซอร์ DHT เป็น DHT11
#define RELAY1_PIN 26  // GPIO สำหรับรีเลย์ 1
#define RELAY2_PIN 25  // GPIO สำหรับรีเลย์ 2
#define RELAY3_PIN 33  // GPIO สำหรับรีเลย์ 3
#define RELAY4_PIN 32  // GPIO สำหรับรีเลย์ 4

// DHT Initialization
DHT dht(DHTPIN, DHTTYPE);  // กำหนดเซ็นเซอร์ DHT

// Timer and Relay States
BlynkTimer timer;                        // ตัวจับเวลาสำหรับฟังก์ชันใน Blynk
int relay1State = LOW;                   // สถานะเริ่มต้นของรีเลย์ 1
int relay2State = LOW;                   // สถานะเริ่มต้นของรีเลย์ 2
int relay3State = LOW;                   // สถานะเริ่มต้นของรีเลย์ 3
int relay4State = LOW;                   // สถานะเริ่มต้นของรีเลย์ 4
unsigned long lastReconnectAttempt = 0;  // เวลาที่พยายามเชื่อมต่อครั้งล่าสุด

// Function Prototypes (ประกาศฟังก์ชันล่วงหน้า เพื่อให้สามารถเรียกใช้งานได้ในโค้ดหลัก)
// ฟังก์ชันสำหรับอ่านค่าจากเซ็นเซอร์ DHT (อุณหภูมิและความชื้น)
void readDHTSensor();
// ฟังก์ชันสำหรับเชื่อมต่อ WiFi อีกครั้ง หากการเชื่อมต่อหลุด
void reconnectWiFi();
// ฟังก์ชันสำหรับเชื่อมต่อ Blynk อีกครั้ง หากการเชื่อมต่อขาดหาย
void reconnectBlynk();

// ฟั่งชั่น setup
void setup() {
  Serial.begin(9600);  // เริ่ม Serial Monitor ที่ความเร็ว 9600 bps
  dht.begin();         // เริ่มต้นเซ็นเซอร์ DHT

  pinMode(LED_PIN, OUTPUT);     // ตั้ง LED เป็น output
  pinMode(RELAY1_PIN, OUTPUT);  // ตั้งรีเลย์ 1 เป็น output
  pinMode(RELAY2_PIN, OUTPUT);  // ตั้งรีเลย์ 2 เป็น output
  pinMode(RELAY3_PIN, OUTPUT);  // ตั้งรีเลย์ 3 เป็น output
  pinMode(RELAY4_PIN, OUTPUT);  // ตั้งรีเลย์ 4 เป็น output

  reconnectWiFi();  // เชื่อมต่อ Wi-Fi ครั้งแรก
  //Blynk.config(auth, "ip-address-blynk-local-server", 8080);  // ตั้งค่า Blynk Server
  Blynk.begin(auth, ssid, pass, "iotservices.thddns.net", 5535);
  if (!client.validateConnection()) {  // ตรวจสอบการเชื่อมต่อ InfluxDB
    Serial.println("InfluxDB connection failed!");
  }

  timer.setInterval(10000L, readDHTSensor);   // อ่านค่า DHT ทุก 10 วินาที
  timer.setInterval(5000L, reconnectWiFi);    // ตรวจสอบ Wi-Fi ทุก 5 วินาที
  timer.setInterval(10000L, reconnectBlynk);  // ตรวจสอบ Blynk ทุก 10 วินาที
}

// ฟังก์ชันที่ทำงานเมื่อ Blynk เชื่อมต่อสำเร็จ
BLYNK_CONNECTED() {
  Serial.println("Blynk connected!");  // แสดงข้อความใน Serial Monitor
  Blynk.syncAll();                     // ซิงค์สถานะของปุ่มและค่าต่างๆ จากเซิร์ฟเวอร์ Blynk
  digitalWrite(LED_PIN, HIGH);         // เปิด LED แสดงสถานะว่าเชื่อมต่อ Blynk แล้ว
}

// ฟังก์ชันที่ทำงานเมื่อ Blynk ขาดการเชื่อมต่อ
BLYNK_DISCONNECTED() {
  digitalWrite(LED_PIN, LOW);  // ปิด LED เพื่อแจ้งว่าการเชื่อมต่อกับ Blynk หายไป
}

// Callback to control Relay 1
BLYNK_WRITE(V10) {
  relay1State = param.asInt();
  digitalWrite(RELAY1_PIN, relay1State);
  Serial.print("Relay 1: ");
  Serial.println(relay1State ? "ON" : "OFF");
}

// Callback to control Relay 2
BLYNK_WRITE(V11) {
  relay2State = param.asInt();
  digitalWrite(RELAY2_PIN, relay2State);
  Serial.print("Relay 2: ");
  Serial.println(relay2State ? "ON" : "OFF");
}

// Callback to control Relay 3
BLYNK_WRITE(V12) {
  relay3State = param.asInt();
  digitalWrite(RELAY3_PIN, relay3State);
  Serial.print("Relay 3: ");
  Serial.println(relay3State ? "ON" : "OFF");
}

// Callback to control Relay 4
BLYNK_WRITE(V13) {
  relay4State = param.asInt();
  digitalWrite(RELAY4_PIN, relay4State);
  Serial.print("Relay 4: ");
  Serial.println(relay4State ? "ON" : "OFF");
}

// อ่านค่าอุณหภูมิและความชื้นจาก DHT
void readDHTSensor() {
  static float lastTemperature = 0.0;  // เก็บค่าอุณหภูมิครั้งก่อน
  static float lastHumidity = 0.0;     // เก็บค่าความชื้นครั้งก่อน

  float temperature = dht.readTemperature();  // อ่านค่าอุณหภูมิจาก DHT
  float humidity = dht.readHumidity();        // อ่านค่าความชื้นจาก DHT

  if (!isnan(temperature) && !isnan(humidity)) {  // ตรวจสอบว่าค่าไม่เป็น NaN
    Serial.print("Temp: ");
    Serial.print(temperature);
    Serial.print("C, Humidity: ");
    Serial.println(humidity);
    // ส่งค่าอุณหภูมิและความชื้นไปยัง Blynk
    Blynk.virtualWrite(V1, temperature);  // ส่งค่าอุณหภูมิไปยัง Virtual Pin V1
    Blynk.virtualWrite(V2, humidity);     // ส่งค่าความชื้นไปยัง Virtual Pin V2
  } else {
    Serial.println("Failed to read from DHT sensor!");  // แสดงข้อความเมื่ออ่านค่าไม่ได้
  }
}

// เชื่อมต่อ Wi-Fi หากหลุด
void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected! Reconnecting...");
    WiFi.begin(ssid, pass);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Wi-Fi reconnected!");
    } else {
      Serial.println("Wi-Fi reconnect failed.");
    }
  }
}

// เชื่อมต่อ Blynk หากหลุด
void reconnectBlynk() {
  unsigned long now = millis();
  if (!Blynk.connected() && (now - lastReconnectAttempt > 10000)) {
    Serial.println("Blynk disconnected! Attempting to reconnect...");
    if (Blynk.connect()) {
      Serial.println("Blynk reconnected!");
    } else {
      Serial.println("Blynk reconnect failed.");
    }
    lastReconnectAttempt = now;
  }
}

// รัน Blynk และ Timer
void loop() {
  Blynk.run();
  timer.run();
}

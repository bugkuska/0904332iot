#include <Wire.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <HardwareSerial.h>

// === Wi-Fi และ Blynk ===
const char ssid[] = "";
const char pass[] = "";
const char auth[] = "";

// === GPIO Configuration ===
#define LED_PIN      2
#define DHTPIN       4  // แก้ซ้ำซ้อน
#define DHTTYPE      DHT11
#define RELAY1_PIN   26
#define RELAY2_PIN   25
#define RELAY3_PIN   33
#define RELAY4_PIN   32
#define PMS_RX       16  // TX ของ PMS7003
#define PMS_TX       17  // RX ของ PMS7003

// === เซนเซอร์ ===
DHT dht(DHTPIN, DHTTYPE);
HardwareSerial pmsSerial(2);  // ใช้ UART2 สำหรับ PMS7003
uint8_t buffer[32];

// === Timer ===
BlynkTimer timer;
unsigned long lastReconnectAttempt = 0;

// === ฟังก์ชันต้นแบบ ===
void readDHTSensor();
void readPM25();
void reconnectWiFi();
void reconnectBlynk();

void setup() {
  Serial.begin(115200);
  dht.begin();
  pmsSerial.begin(9600, SERIAL_8N1, PMS_RX, PMS_TX);  // PMS7003

  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);

  reconnectWiFi();
  Blynk.begin(auth, ssid, pass, "iotservices.thddns.net", 5535);

  timer.setInterval(10000L, readDHTSensor);   // ทุก 10 วินาที
  timer.setInterval(15000L, readPM25);        // ทุก 15 วินาที
  timer.setInterval(5000L, reconnectWiFi);
  timer.setInterval(10000L, reconnectBlynk);
}

BLYNK_CONNECTED() {
  Serial.println("✅ Blynk connected!");
  Blynk.syncAll();
  digitalWrite(LED_PIN, HIGH);
}

BLYNK_DISCONNECTED() {
  digitalWrite(LED_PIN, LOW);
}

// === อ่าน DHT11 ===
void readDHTSensor() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("❌ ไม่สามารถอ่านค่า DHT11 ได้");
  } else {
    Serial.print("🌡️ อุณหภูมิ: ");
    Serial.print(temperature);
    Serial.print(" °C, 💧 ความชื้น: ");
    Serial.print(humidity);
    Serial.println(" %");

    // ส่งขึ้น Blynk
    Blynk.virtualWrite(V1, temperature);
    Blynk.virtualWrite(V2, humidity);
  }
}

// === อ่านค่า PM2.5 จาก PMS7003 ===
void readPM25() {
  while (pmsSerial.available() > 0) pmsSerial.read();  // ล้างบัฟเฟอร์ก่อนอ่าน
  delay(100);  // รอข้อมูลเข้า

  if (pmsSerial.available() >= 32) {
    if (pmsSerial.read() == 0x42 && pmsSerial.read() == 0x4D) {
      buffer[0] = 0x42;
      buffer[1] = 0x4D;
      pmsSerial.readBytes(&buffer[2], 30);

      // ตรวจ checksum
      uint16_t sum = 0;
      for (int i = 0; i < 30; i++) sum += buffer[i];
      uint16_t checksum = (buffer[30] << 8) | buffer[31];

      if (sum == checksum) {
        uint16_t pm1_0 = (buffer[4] << 8) | buffer[5];
        uint16_t pm2_5 = (buffer[6] << 8) | buffer[7];
        uint16_t pm10  = (buffer[8] << 8) | buffer[9];

        Serial.printf("🌫️ PM1.0: %d | PM2.5: %d | PM10: %d µg/m³\n", pm1_0, pm2_5, pm10);

        // ส่งค่า PM2.5 ไป Blynk
        Blynk.virtualWrite(V3, pm2_5);
      } else {
        Serial.println("⚠️ Checksum ผิดพลาด ข้ามข้อมูล");
      }
    }
  }
}

// === รีเชื่อมต่อ Wi-Fi ===
void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("🔌 Wi-Fi disconnected! Trying to reconnect...");
    WiFi.begin(ssid, pass);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✅ Wi-Fi reconnected!");
    } else {
      Serial.println("\n❌ Wi-Fi reconnect failed.");
    }
  }
}

// === รีเชื่อมต่อ Blynk ===
void reconnectBlynk() {
  unsigned long now = millis();
  if (!Blynk.connected() && (now - lastReconnectAttempt > 10000)) {
    Serial.println("🔄 Blynk disconnected! Attempting to reconnect...");
    if (Blynk.connect()) {
      Serial.println("✅ Blynk reconnected!");
    } else {
      Serial.println("❌ Blynk reconnect failed.");
    }
    lastReconnectAttempt = now;
  }
}

// === Loop หลัก ===
void loop() {
  Blynk.run();
  timer.run();
}

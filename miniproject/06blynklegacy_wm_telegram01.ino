#include <Wire.h>              // ใช้สำหรับการสื่อสาร I2C
#include <WiFi.h>              // ใช้สำหรับการเชื่อมต่อ Wi-Fi
#include <WiFiClient.h>        // ใช้สำหรับการเชื่อมต่อ Wi-Fi Client
#include <BlynkSimpleEsp32.h>  // ใช้สำหรับการเชื่อมต่อกับ Blynk Server *ให้ใช้ Blynk version 0.60 เท่านั้น
#include <DHT.h>               // ไลบรารีสำหรับเซ็นเซอร์ DHT (ตรวจวัดอุณหภูมิและความชื้น)
#include <HTTPClient.h>        // ใช้สำหรับส่ง HTTP Request
#include <WiFiManager.h>       // 🔹 เพิ่ม WiFiManager

// 🔹 กำหนด Token และ Chat ID หรือ Chat ID ของกลุ่ม
const char* botToken = "";  //Telegram HTTP API Token
const char* chatID = "";                                       //Chat ID หรือ Chat ID ของกลุ่ม

float tempThreshold = 30.0;      // ค่าอุณหภูมิที่แจ้งเตือน (เริ่มต้น 30°C)
float humidityThreshold = 40.0;  // ค่าความชื้นที่แจ้งเตือน (เริ่มต้น 40%)

// Blynk credentials
const char auth[] = "";        //Blynk Token

// GPIO Configuration
#define LED_PIN 2      // ขาที่ใช้ควบคุม LED
#define DHTPIN 15      // ขาที่เชื่อมต่อกับเซ็นเซอร์ DHT
#define DHTTYPE DHT11  // กำหนดประเภทเซ็นเซอร์ DHT เป็น DHT11
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

// 🔹 ฟังก์ชันสำหรับตั้งค่า Wi-Fi อัตโนมัติ
void setupWiFi() {
  WiFiManager wifiManager;
  //ให้ตั้งชื่อ Hotspot เป็น ชื่อภาษาอังกฤษ เช่น Sompoch_MiniProject
  wifiManager.autoConnect("AJSompoch_MiniProject");  // เปิด Hotspot ให้ตั้งค่า Wi-Fi เองหากไม่มีการเชื่อมต่อ Wi-Fi ที่เคยเชื่อมต่อได้
  Serial.println("✅ Wi-Fi Connected!");
}

// ฟั่งชั่น setup
void setup() {
  Serial.begin(9600);
  dht.begin();

  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);

  setupWiFi();  // 🔹 เชื่อมต่อ Wi-Fi อัตโนมัติ
  Blynk.begin(auth, WiFi.SSID().c_str(), WiFi.psk().c_str(), "iotservices.thddns.net", 5535);

  timer.setInterval(10000L, readDHTSensor);
  timer.setInterval(5000L, reconnectWiFi);
  timer.setInterval(10000L, reconnectBlynk);
}

// ฟังก์ชันที่ทำงานเมื่อ Blynk เชื่อมต่อสำเร็จ
BLYNK_CONNECTED() {
  Serial.println("Blynk connected!");
  Blynk.syncAll();
  digitalWrite(LED_PIN, HIGH);
}

// ฟังก์ชันที่ทำงานเมื่อ Blynk ขาดการเชื่อมต่อ
BLYNK_DISCONNECTED() {
  digitalWrite(LED_PIN, LOW);
}

// Slider ปรับค่าแจ้งเตือนอุณหภูมิ
BLYNK_WRITE(V3) {
  tempThreshold = param.asFloat();
  Serial.print("🛠 ปรับค่าแจ้งเตือนอุณหภูมิเป็น: ");
  Serial.print(tempThreshold);
  Serial.println("°C");
}

// Slider ปรับค่าแจ้งเตือนความชื้น
BLYNK_WRITE(V4) {
  humidityThreshold = param.asFloat();
  Serial.print("🛠 ปรับค่าแจ้งเตือนความชื้นเป็น: ");
  Serial.print(humidityThreshold);
  Serial.println("%");
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
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (!isnan(temperature) && !isnan(humidity)) {
    Serial.printf("🌡 Temp: %.2f°C, 💧 Humidity: %.2f%%\n", temperature, humidity);
    Blynk.virtualWrite(V1, temperature);
    Blynk.virtualWrite(V2, humidity);

    if (temperature > tempThreshold) {
      String msg = "🔥 แจ้งเตือน! อุณหภูมิสูงเกินกำหนด (" + String(tempThreshold) + "°C): " + String(temperature) + "°C";
      sendTelegramMessage(msg);
    }
    if (humidity < humidityThreshold) {
      String msg = "💧 แจ้งเตือน! ความชื้นต่ำกว่ากำหนด (" + String(humidityThreshold) + "%): " + String(humidity) + "%";
      sendTelegramMessage(msg);
    }
  } else {
    Serial.println("❌ Failed to read from DHT sensor!");
  }
}

// 🔹 ปรับ reconnectWiFi() ให้ใช้ WiFiManager
void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("🔄 Reconnecting Wi-Fi...");
    WiFiManager wifiManager;
    wifiManager.autoConnect("ESP32_Config");  // เปิด AP ให้ตั้งค่า Wi-Fi ใหม่ถ้าหลุด
    Serial.println("✅ Wi-Fi Reconnected!");
  }
}

// เชื่อมต่อ Blynk หากหลุด
void reconnectBlynk() {
  unsigned long now = millis();
  if (!Blynk.connected() && (now - lastReconnectAttempt > 10000)) {
    Serial.println("Blynk disconnected! Attempting to reconnect...");
    if (Blynk.connect()) {
      Serial.println("✅ Blynk Reconnected!");
    } else {
      Serial.println("❌ Blynk reconnect failed.");
    }
    lastReconnectAttempt = now;
  }
}

// ส่งข้อความไปยัง Telegram
void sendTelegramMessage(String message) {
  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(botToken) + "/sendMessage?chat_id=" + String(chatID) + "&text=" + message;

  http.begin(url);
  int httpResponseCode = http.GET();

  Serial.print("📡 HTTP Response Code: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
    Serial.println("✅ ส่งข้อความไปยัง Telegram สำเร็จ!");
    String response = http.getString();
    Serial.println("📨 Telegram Response: " + response);
  } else {
    Serial.print("❌ ส่งข้อความไปยัง Telegram ล้มเหลว: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

// รัน Blynk และ Timer
void loop() {
  Blynk.run();
  timer.run();
}

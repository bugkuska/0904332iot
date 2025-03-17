#include <Wire.h>              // ใช้สำหรับการสื่อสาร I2C
#include <WiFi.h>              // ใช้สำหรับการเชื่อมต่อ Wi-Fi
#include <WiFiClient.h>        // ใช้สำหรับการเชื่อมต่อ Wi-Fi Client
#include <BlynkSimpleEsp32.h>  // ใช้สำหรับการเชื่อมต่อกับ Blynk Server *ให้ใช้ Blynk version 0.60 เท่านั้น
#include <DHT.h>               // ไลบรารีสำหรับเซ็นเซอร์ DHT (ตรวจวัดอุณหภูมิและความชื้น)

//การแจ้งเตือนผ่าน Telegram
#include <HTTPClient.h> // ใช้สำหรับส่ง HTTP Request
// 🔹 กำหนด Token และ Chat ID หรือ Chat ID ของกลุ่ม
const char* botToken = ""; //Telegram HTTP API Token
const char* chatID = ""; //Chat ID หรือ Chat ID ของกลุ่ม

float tempThreshold = 30.0;  // ค่าอุณหภูมิที่แจ้งเตือน (เริ่มต้น 30°C)
float humidityThreshold = 40.0; // ค่าความชื้นที่แจ้งเตือน (เริ่มต้น 40%)

// Wi-Fi and Blynk credentials
const char ssid[] = "";                         // ชื่อ Wi-Fi
const char pass[] = "";                        // รหัสผ่าน Wi-Fi
const char auth[] = "";  // Auth Token ของ Blynk

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
  Blynk.begin(auth, ssid, pass, "iotservices.thddns.net", 5535);  //ตัวอย่าง Blynk Public Server 
  
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

// Slider ปรับค่าอุณหภูมิที่ต้องแจ้งเตือนเมื่ออุณหภูมิสูงกว่าระดับที่กำหนด
BLYNK_WRITE(V3) {  
    tempThreshold = param.asFloat();
    Serial.print("🛠 ปรับค่าแจ้งเตือนอุณหภูมิเป็น: ");
    Serial.print(tempThreshold);
    Serial.println("°C");
}

// Slider ปรับค่าความชื้นที่ต้องแจ้งเตือนเมื่อความชื้นสูงกว่าระดับที่กำหนด
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

        // 🔴 **ใช้ค่า Slider แทนค่าคงที่**
        if (temperature > tempThreshold) {  // ถ้าอุณหภูมิสูงกว่าเกณฑ์
            String msg = "🔥 แจ้งเตือน! อุณหภูมิสูงเกินกำหนด (" + String(tempThreshold) + "°C): " + String(temperature) + "°C";
            sendTelegramMessage(msg);
        }
        if (humidity < humidityThreshold) {  // ถ้าความชื้นต่ำกว่าเกณฑ์
            String msg = "💧 แจ้งเตือน! ความชื้นต่ำกว่ากำหนด (" + String(humidityThreshold) + "%): " + String(humidity) + "%";
            sendTelegramMessage(msg);
        }

    } else {
        Serial.println("❌ Failed to read from DHT sensor!");
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

//Function สำหรับส่งการแจ้งเตือนผ่าน Telegram
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

// Function Loop Blynk และ Timer
void loop() {
  Blynk.run();
  timer.run();
}

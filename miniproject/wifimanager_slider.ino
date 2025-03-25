#include <Wire.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <WiFiManager.h>

// 🔹 Token และ Chat ID ของ Telegram
const char* botToken = "YOUR_BOT_TOKEN";
const char* chatID = "YOUR_CHAT_ID";

// 🔹 ค่าตั้งต้น
float tempThreshold = 30.0;
float humidityThreshold = 40.0;
float soilMoistureThreshold = 30.0;  // ค่าแจ้งเตือนความชื้นในดิน

// 🔹 Blynk Token
const char auth[] = "YOUR_BLYNK_TOKEN";

// 🔹 GPIO Configuration
#define LED_PIN 2
#define DHTPIN 15
#define DHTTYPE DHT11
#define RELAY1_PIN 26
#define RELAY2_PIN 25
#define RELAY3_PIN 33
#define RELAY4_PIN 32
#define SOIL_SENSOR_PIN 34  // เซ็นเซอร์ความชื้นในดิน (analog)

// 🔹 Sensor และ Relay States
DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;
int relay1State = LOW;
int relay2State = LOW;
int relay3State = LOW;
int relay4State = LOW;
unsigned long lastReconnectAttempt = 0;

// 🔹 Wi-Fi Setup
void setupWiFi() {
  WiFiManager wifiManager;
  wifiManager.autoConnect("AJSompoch_MiniProject");
  Serial.println("✅ Wi-Fi Connected!");
}

// 🔹 Setup
void setup() {
  Serial.begin(9600);
  dht.begin();

  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);
  pinMode(SOIL_SENSOR_PIN, INPUT);

  setupWiFi();
  Blynk.begin(auth, WiFi.SSID().c_str(), WiFi.psk().c_str(), "iotservices.thddns.net", 5535);

  timer.setInterval(10000L, readDHTSensor);
  timer.setInterval(15000L, readSoilMoisture);
  timer.setInterval(5000L, reconnectWiFi);
  timer.setInterval(10000L, reconnectBlynk);
}

// 🔹 Blynk Connected
BLYNK_CONNECTED() {
  Serial.println("Blynk connected!");
  Blynk.syncAll();
  digitalWrite(LED_PIN, HIGH);
}

BLYNK_DISCONNECTED() {
  digitalWrite(LED_PIN, LOW);
}

// 🔹 Slider ปรับค่าต่างๆ
BLYNK_WRITE(V3) {
  tempThreshold = param.asFloat();
  Serial.printf("🛠 ปรับค่าแจ้งเตือนอุณหภูมิเป็น: %.2f°C\n", tempThreshold);
}

BLYNK_WRITE(V4) {
  humidityThreshold = param.asFloat();
  Serial.printf("🛠 ปรับค่าแจ้งเตือนความชื้นเป็น: %.2f%%\n", humidityThreshold);
}

BLYNK_WRITE(V5) {
  soilMoistureThreshold = param.asFloat();
  Serial.printf("🛠 ปรับค่าแจ้งเตือนความชื้นดินเป็น: %.2f%%\n", soilMoistureThreshold);
}

// 🔹 Relay Control
BLYNK_WRITE(V10) {
  relay1State = param.asInt();
  digitalWrite(RELAY1_PIN, relay1State);
}

BLYNK_WRITE(V11) {
  relay2State = param.asInt();
  digitalWrite(RELAY2_PIN, relay2State);
}

BLYNK_WRITE(V12) {
  relay3State = param.asInt();
  digitalWrite(RELAY3_PIN, relay3State);
}

BLYNK_WRITE(V13) {
  relay4State = param.asInt();
  digitalWrite(RELAY4_PIN, relay4State);
}

// 🔹 อ่านอุณหภูมิและความชื้น
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

// 🔹 อ่านค่าความชื้นในดิน
void readSoilMoisture() {
  int raw = analogRead(SOIL_SENSOR_PIN); // 0-4095
  float soilMoisture = map(raw, 4095, 0, 0, 100); // ยิ่งดินแห้ง ค่ายิ่งมาก
  Serial.printf("🌱 Soil Moisture: %.2f%%\n", soilMoisture);
  Blynk.virtualWrite(V6, soilMoisture);

  if (soilMoisture < soilMoistureThreshold) {
    sendTelegramMessage("🌱 แจ้งเตือน! ความชื้นในดินต่ำกว่ากำหนด (" + 
      String(soilMoistureThreshold) + "%): " + String(soilMoisture, 1) + "%");
  }
}

// 🔹 Reconnect Wi-Fi
void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("🔄 Reconnecting Wi-Fi...");
    WiFiManager wifiManager;
    wifiManager.autoConnect("ESP32_Config");
    Serial.println("✅ Wi-Fi Reconnected!");
  }
}

// 🔹 Reconnect Blynk
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

// 🔹 ส่งข้อความไปยัง Telegram
void sendTelegramMessage(String message) {
  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(botToken) +
               "/sendMessage?chat_id=" + String(chatID) +
               "&text=" + message;

  http.begin(url);
  int httpResponseCode = http.GET();

  Serial.print("📡 HTTP Response Code: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
    Serial.println("✅ ส่งข้อความ Telegram สำเร็จ!");
    String response = http.getString();
    Serial.println("📨 Response: " + response);
  } else {
    Serial.print("❌ ส่งข้อความ Telegram ล้มเหลว: ");
    Serial.println(httpResponseCode);
  }
  http.end();
}

// 🔹 Loop
void loop() {
  Blynk.run();
  timer.run();
}

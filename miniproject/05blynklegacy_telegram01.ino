/******************** Includes ********************/
#define BLYNK_PRINT Serial                                   // ให้ Blynk พิมพ์ข้อความดีบักออก Serial Monitor
#include <Wire.h>                                            // ไลบรารี I2C (ใช้กับ LCD)
#include <LiquidCrystal_I2C.h>                               // ไลบรารีควบคุม LCD1602 ผ่าน I2C
#include <DHT.h>                                             // ไลบรารีเซ็นเซอร์ DHT (Adafruit)
#include <WiFi.h>                                            // ไลบรารี Wi-Fi สำหรับ ESP32
#include <WiFiClient.h>                                      // ไลบรารี TCP client
#include <BlynkSimpleEsp32.h>                                // ไลบรารี Blynk 0.6.x สำหรับ ESP32
#include <HTTPClient.h>                                      // ← เพิ่ม: ใช้ส่ง HTTP ไปยัง Telegram Bot API

/******************** LCD ********************/
LiquidCrystal_I2C lcd(0x27, 16, 2);                          // LCD I2C address 0x27 ขนาด 16x2 (บางบอร์ดอาจเป็น 0x3F)

/******************** Pins ********************/
#define SOIL_PIN     35                                      // ADC1 ช่อง GPIO35 (input-only) สำหรับ Soil
#define RELAY1_PIN    4                                      // รีเลย์ช่อง 1
#define RELAY2_PIN    5                                      // รีเลย์ช่อง 2
#define RELAY3_PIN   18                                      // รีเลย์ช่อง 3
#define RELAY4_PIN   19                                      // รีเลย์ช่อง 4
#define DHTPIN       15                                      // ขา DATA ของ DHT
#define DHTTYPE  DHT11                                       // ประเภท DHT: DHT11 (เปลี่ยนเป็น DHT22 ได้)
#define STATUS_LED    2                                      // GPIO2: เปิดเมื่อเชื่อม Blynk สำเร็จ (ไฟสถานะ)

/******************** Sensors ********************/
DHT dht(DHTPIN, DHTTYPE);                                    // ออบเจกต์ DHT สำหรับอ่านอุณหภูมิ/ความชื้นอากาศ

/******************** Soil Calibration ********************/
const int SOIL_DRY_ADC = 3000;                               // ค่า ADC เมื่อดิน “แห้งมาก” (คาลิเบรตจากของจริง)
const int SOIL_WET_ADC = 1200;                               // ค่า ADC เมื่อดิน “เปียกมาก” (คาลิเบรตจากของจริง)

/******************** Blynk Credentials ********************/
const char ssid[] = "";                                      // ชื่อ Wi-Fi (ใส่ของคุณ)
const char pass[] = "";                                      // รหัสผ่าน Wi-Fi (ใส่ของคุณ)
const char auth[] = "";                                      // Blynk Auth Token (จากแอป Blynk)
const char BLYNK_SERVER_IP[] = "";                           // IP ของ Blynk Local Server
const uint16_t BLYNK_SERVER_PORT = 8080;                     // พอร์ตเซิร์ฟเวอร์ (ปกติ 8080)

/******************** Timers ********************/
BlynkTimer timer;                                            // ตัวตั้งเวลา non-blocking ของ Blynk
const unsigned long DHT_INTERVAL_MS   = 2000;                // รอบเวลาอ่าน DHT (ms) — DHT11 แนะนำ ≥ 2000ms
const unsigned long SOIL_INTERVAL_MS  = 1000;                // รอบเวลาอ่าน Soil (ms)
const unsigned long RECONN_INTERVAL_MS = 5000;               // รอบเวลาลองเชื่อมต่อ Wi-Fi/Blynk ใหม่ (ms)

/******************** Forward Declarations ********************/
void taskReadAndDisplay();                                    // อ่าน DHT/Soil แล้วอัปเดต Serial + LCD (ทำเสมอ)
void taskSendToBlynk();                                       // ส่งค่าขึ้น Blynk เมื่อเชื่อมต่อแล้ว
void taskReconnect();                                         // พยายามเชื่อมต่อ Wi-Fi/Blynk เป็นระยะ
void showNetStatus();                                         // แสดงสถานะเครือข่ายบน LCD แถวล่าง

/************* Telegram Settings (เพิ่ม) *************/
const char TELEGRAM_BOT_TOKEN[] = "";                         // ← ใส่ Token ของบอท Telegram
const char TELEGRAM_CHAT_ID[]  = "";                          // ← ใส่ Chat ID หรือ Group ID ที่จะส่งข้อความ
float tempThreshold = 30.0;                                   // ← เกณฑ์แจ้งเตือน: อุณหภูมิสูงกว่า (°C)
float humidityThreshold = 40.0;                               // ← เกณฑ์แจ้งเตือน: ความชื้นต่ำกว่า (%RH)
float soilThreshold = 30.0;                  // ← เกณฑ์แจ้งเตือน: ความชื้นดิน "ต่ำกว่า" ค่านี้จะถือว่าแห้ง (%)

const unsigned long ALERT_COOLDOWN_MS = 60000UL;              // ← กันสแปม: ส่งเตือนซ้ำขั้นต่ำทุก 60 วินาที/เงื่อนไข
unsigned long lastTempAlertMs = 0;                            // ← เวลาเตือนอุณหภูมิล่าสุด (ms)
unsigned long lastHumiAlertMs = 0;                            // ← เวลาเตือนความชื้นล่าสุด (ms)
unsigned long lastSoilAlertMs = 0;                  // เวลาเตือน Soil ล่าสุด (ใช้ร่วมกับ ALERT_COOLDOWN_MS)

/******************** Setup ********************/
void setup() {
  Serial.begin(9600);                                         // เปิด Serial Monitor ที่ 9600 bps
  dht.begin();                                                // เริ่มเซ็นเซอร์ DHT

  pinMode(SOIL_PIN, INPUT);                                   // ตั้งขา Soil เป็นอินพุต
  pinMode(RELAY1_PIN, OUTPUT);                                // ตั้งรีเลย์ 1 เป็นเอาต์พุต
  pinMode(RELAY2_PIN, OUTPUT);                                // ตั้งรีเลย์ 2 เป็นเอาต์พุต
  pinMode(RELAY3_PIN, OUTPUT);                                // ตั้งรีเลย์ 3 เป็นเอาต์พุต
  pinMode(RELAY4_PIN, OUTPUT);                                // ตั้งรีเลย์ 4 เป็นเอาต์พุต
  digitalWrite(RELAY1_PIN, LOW);                              // ปิดรีเลย์ 1 ตอนเริ่ม (ปรับตาม Active-LOW/HIGH ของบอร์ดจริง)
  digitalWrite(RELAY2_PIN, LOW);                              // ปิดรีเลย์ 2 ตอนเริ่ม
  digitalWrite(RELAY3_PIN, LOW);                              // ปิดรีเลย์ 3 ตอนเริ่ม
  digitalWrite(RELAY4_PIN, LOW);                              // ปิดรีเลย์ 4 ตอนเริ่ม

  pinMode(STATUS_LED, OUTPUT);                                // ตั้ง GPIO2 เป็นเอาต์พุต
  digitalWrite(STATUS_LED, LOW);                              // ดับไฟสถานะไว้ก่อน (ยังไม่ออนไลน์ Blynk)

  lcd.begin();                                                // เริ่มต้นจอ LCD (อย่าเรียกใน loop)
  lcd.backlight();                                            // เปิดไฟพื้นหลัง
  lcd.clear();                                                // เคลียร์หน้าจอ
  lcd.setCursor(0, 0);                                        // ไปแถว 1 คอลัมน์ 0
  lcd.print("Booting...");                                    // ข้อความบูต
  lcd.setCursor(0, 1);                                        // ไปแถว 2 คอลัมน์ 0
  lcd.print("WiFi: connecting");                              // แจ้งกำลังเชื่อม Wi-Fi

  WiFi.begin(ssid, pass);                                     // เริ่มเชื่อมต่อ Wi-Fi (ไม่บล็อกงานหลัก)
  WiFi.setAutoReconnect(true);                                // ให้ต่อ Wi-Fi ใหม่อัตโนมัติเมื่อหลุด
  WiFi.persistent(true);                                      // บันทึก config เครือข่ายไว้ถาวร

  Blynk.config(auth, BLYNK_SERVER_IP, BLYNK_SERVER_PORT);     // กำหนดปลายทาง Blynk (ยังไม่ connect ทันที)
  // ใช้ Blynk.config + Blynk.connect(timeout) เพื่อไม่ให้โปรแกรมค้างถ้าเน็ตล่ม

  timer.setInterval(DHT_INTERVAL_MS,  taskReadAndDisplay);    // อ่าน/แสดงผลเซ็นเซอร์ทุก 2 วินาที (ทำเสมอ)
  timer.setInterval(DHT_INTERVAL_MS,  taskSendToBlynk);       // ส่งค่าไป Blynk ทุก 2 วินาที (เฉพาะเมื่อเชื่อมต่อ)
  timer.setInterval(RECONN_INTERVAL_MS, taskReconnect);       // พยายามเชื่อมต่อ Wi-Fi/Blynk ทุก 5 วินาที
}

/******************** Blynk Connected Callback ********************/
BLYNK_CONNECTED() {
  Serial.println("Blynk connected!");                         // แจ้งสถานะต่อ Blynk สำเร็จ
  Blynk.syncAll();                                            // ซิงค์สถานะ Virtual Pins ทั้งหมด
  digitalWrite(STATUS_LED, HIGH);                             // เปิด GPIO2 เป็นไฟสถานะ
  lcd.setCursor(0, 1);                                        // ไปแถวล่าง
  lcd.print("Blynk: online   ");                              // แสดงสถานะออนไลน์ (เติมช่องว่างลบเศษอักษร)
}

/******************** Relay Controls via Blynk (V10–V13) ********************/
BLYNK_WRITE(V10) {                                            // ควบคุมรีเลย์ 1 ที่ V10
  int st = param.asInt();                                     // รับค่า 0/1
  digitalWrite(RELAY1_PIN, st);                               // เขียนสถานะถึงรีเลย์ 1 (ถ้า Active-LOW ให้ใช้ !st)
  Serial.print("Relay 1 → "); Serial.println(st ? "ON" : "OFF"); // ดีบักสถานะ
}

BLYNK_WRITE(V11) {                                            // ควบคุมรีเลย์ 2 ที่ V11
  int st = param.asInt();                                     // รับค่า 0/1
  digitalWrite(RELAY2_PIN, st);                               // เขียนสถานะถึงรีเลย์ 2
  Serial.print("Relay 2 → "); Serial.println(st ? "ON" : "OFF"); // ดีบักสถานะ
}

BLYNK_WRITE(V12) {                                            // ควบคุมรีเลย์ 3 ที่ V12
  int st = param.asInt();                                     // รับค่า 0/1
  digitalWrite(RELAY3_PIN, st);                               // เขียนสถานะถึงรีเลย์ 3
  Serial.print("Relay 3 → "); Serial.println(st ? "ON" : "OFF"); // ดีบักสถานะ
}

BLYNK_WRITE(V13) {                                            // ควบคุมรีเลย์ 4 ที่ V13
  int st = param.asInt();                                     // รับค่า 0/1
  digitalWrite(RELAY4_PIN, st);                               // เขียนสถานะถึงรีเลย์ 4
  Serial.print("Relay 4 → "); Serial.println(st ? "ON" : "OFF"); // ดีบักสถานะ
}

// ******************** Threshold Controls via Blynk (Slider V30, V31) ********************
BLYNK_WRITE(V30) {                           // คอลแบ็กรับค่าจาก Slider ที่ Virtual Pin V30 (เกณฑ์อุณหภูมิ)
  float v = param.asFloat();                 // แปลงค่าที่ได้จากแอปเป็น float
  if (!isnan(v)) {                           // ป้องกันกรณีอ่านค่าไม่ได้ (NaN)
    tempThreshold = constrain(v, 0.0f, 80.0f); // จำกัดช่วง 0–80°C เพื่อความสมเหตุสมผล
    Serial.print("Set Temp Threshold: ");    // พิมพ์ค่าใหม่ลง Serial เพื่อตรวจสอบ
    Serial.print(tempThreshold);             
    Serial.println(" °C");                   
  }
}

BLYNK_WRITE(V31) {                           // คอลแบ็กรับค่าจาก Slider ที่ Virtual Pin V31 (เกณฑ์ความชื้น)
  float v = param.asFloat();                 // แปลงค่าที่ได้จากแอปเป็น float
  if (!isnan(v)) {                           // ป้องกันกรณีอ่านค่าไม่ได้ (NaN)
    humidityThreshold = constrain(v, 0.0f, 100.0f); // จำกัดช่วง 0–100%RH
    Serial.print("Set Humi Threshold: ");    // พิมพ์ค่าใหม่ลง Serial เพื่อตรวจสอบ
    Serial.print(humidityThreshold);         
    Serial.println(" %");                    
  }
}

// ******************** Threshold Controls via Blynk (Slider V32: Soil %) ********************
BLYNK_WRITE(V32) {                           // คอลแบ็กรับค่าจาก Slider ที่ Virtual Pin V32 (เกณฑ์ความชื้นดิน)
  float v = param.asFloat();                 // แปลงค่าที่ได้จากแอปเป็น float
  if (!isnan(v)) {                           // ป้องกัน NaN (อ่านค่าไม่ได้)
    soilThreshold = constrain(v, 0.0f, 100.0f); // จำกัดช่วง 0–100% ให้สมเหตุสมผล
    Serial.print("Set Soil Threshold: ");    // แสดงค่าใหม่ใน Serial เพื่อตรวจสอบ
    Serial.print(soilThreshold);             
    Serial.println(" %");                    
  }
}

/******************** Periodic Tasks ********************/
void taskReadAndDisplay() {
  float h = dht.readHumidity();                               // อ่านความชื้นอากาศ (%RH)
  float t = dht.readTemperature();                            // อ่านอุณหภูมิ (°C)
  int soilRaw = analogRead(SOIL_PIN);                         // อ่านค่า ADC ของ Soil (0–4095)
  int span = SOIL_DRY_ADC - SOIL_WET_ADC;                     // ช่วงคาลิเบรต (แห้ง-เปียก)
  if (span <= 0) span = 1;                                    // ป้องกันหารด้วยศูนย์
  int soilPct = (int)(((float)(SOIL_DRY_ADC - soilRaw) * 100.0f) / span); // map ดิบ→%
  soilPct = constrain(soilPct, 0, 100);                       // จำกัดช่วง 0–100%

  Serial.print("T: "); Serial.print(t); Serial.print(" C | "); // พิมพ์อุณหภูมิลง Serial
  Serial.print("H: "); Serial.print(h); Serial.print(" % | "); // พิมพ์ความชื้นอากาศลง Serial
  Serial.print("Soil: "); Serial.print(soilPct); Serial.println(" %"); // พิมพ์ Soil% ลง Serial

  lcd.setCursor(0, 0);                                        // ไปต้นบรรทัดแรก
  lcd.print("T:");                                            // ป้ายกำกับอุณหภูมิ
  if (isnan(t)) lcd.print("--.-"); else lcd.print(t, 1);       // ถ้าอ่านไม่ได้แสดง --.- มิฉะนั้นแสดงทศนิยม 1 ตำแหน่ง
  lcd.print((char)223); lcd.print("C ");                      // สัญลักษณ์ °C
  lcd.print("H:");                                            // ป้ายกำกับความชื้น
  if (isnan(h)) lcd.print("--.-"); else lcd.print(h, 0);       // ถ้าอ่านไม่ได้แสดง --.- มิฉะนั้นแสดงจำนวนเต็ม
  lcd.print("%");                                             // หน่วย %

  lcd.setCursor(0, 1);                                        // ไปบรรทัดล่าง
  lcd.print("Soil:");                                         // ป้ายกำกับ Soil
  lcd.print(soilPct);                                         // แสดง Soil%
  lcd.print("%    ");                                         // เติมช่องว่างเพื่อเคลียร์เศษตัวอักษรค้าง

  showNetStatus();                                            // อัปเดตสถานะเครือข่าย (เติมท้ายบรรทัดล่างให้พอดี)

  /***** เพิ่ม: ตรวจเกณฑ์แล้วแจ้งเตือน Telegram (ไม่บล็อก loop) *****/
  if (!isnan(t) && (t > tempThreshold)) {                     // ถ้าอ่านอุณหภูมิได้ และสูงกว่าเกณฑ์ที่ตั้งไว้
    if (millis() - lastTempAlertMs >= ALERT_COOLDOWN_MS) {    // เช็ก cooldown เพื่อกันสแปมแจ้งเตือน
      String msg = "🔥 Temp High: " + String(t, 1) + "°C (>" + String(tempThreshold, 1) + "°C)"; // สร้างข้อความเตือน
      sendTelegramMessage(msg);                               // ส่งข้อความไป Telegram
      lastTempAlertMs = millis();                              // บันทึกเวลาแจ้งเตือนล่าสุด
    }
  }
  if (!isnan(h) && (h < humidityThreshold)) {                 // ถ้าอ่านความชื้นได้ และต่ำกว่าเกณฑ์ที่ตั้งไว้
    if (millis() - lastHumiAlertMs >= ALERT_COOLDOWN_MS) {    // เช็ก cooldown เพื่อกันสแปมแจ้งเตือน
      String msg = "💧 Humidity Low: " + String(h, 0) + "% (<" + String(humidityThreshold, 0) + "%)"; // ข้อความเตือน
      sendTelegramMessage(msg);                               // ส่งข้อความไป Telegram
      lastHumiAlertMs = millis();                              // บันทึกเวลาแจ้งเตือนล่าสุด
    }
  }

  /* ====== เพิ่มบล็อก Soil Alert ตรงนี้ ====== */
  if (soilPct < soilThreshold) {                               // ถ้า % ความชื้นดินต่ำกว่าเกณฑ์ที่ตั้งไว้ (แห้ง)
    if (millis() - lastSoilAlertMs >= ALERT_COOLDOWN_MS) {     // กันสแปม: ส่งซ้ำได้เมื่อพ้นช่วง cooldown ที่กำหนด
      String msg = "🌱 Soil Low: " + String(soilPct) + "% (< " // สร้างข้อความแจ้งเตือนบอกราคา Soil ปัจจุบัน
                   + String(soilThreshold) + "%)";             // ต่อท้ายด้วยเกณฑ์ที่ผู้ใช้ตั้งไว้
      sendTelegramMessage(msg);                                 // เรียกฟังก์ชันส่งข้อความไป Telegram
      lastSoilAlertMs = millis();                               // บันทึกเวลาแจ้งเตือนล่าสุดของ Soil เพื่อคุม cooldown
    }
  }
  /* ====== จบส่วนที่เพิ่ม ====== */
}


void taskSendToBlynk() {
  if (!Blynk.connected()) return;                             // ถ้ายังไม่เชื่อม Blynk ให้ข้าม (ทำงานออฟไลน์ต่อ)
  float h = dht.readHumidity();                               // อ่านความชื้นอากาศ
  float t = dht.readTemperature();                            // อ่านอุณหภูมิ
  int soilRaw = analogRead(SOIL_PIN);                         // อ่าน Soil ADC
  int span = SOIL_DRY_ADC - SOIL_WET_ADC; if (span <= 0) span = 1; // กันหาร 0
  int soilPct = (int)(((float)(SOIL_DRY_ADC - soilRaw) * 100.0f) / span); // map → %
  soilPct = constrain(soilPct, 0, 100);                       // จำกัดช่วง

  if (!isnan(t)) Blynk.virtualWrite(V1, t);                   // ส่งอุณหภูมิไป V1 (ถ้าอ่านได้)
  if (!isnan(h)) Blynk.virtualWrite(V2, h);                   // ส่งความชื้นไป V2 (ถ้าอ่านได้)
  Blynk.virtualWrite(V3, soilPct);                            // ส่ง Soil% ไป V3
}

void taskReconnect() {
  if (WiFi.status() != WL_CONNECTED) {                        // ถ้า Wi-Fi หลุด
    Serial.println("Wi-Fi disconnected → reconnecting...");   // แจ้งเตือน
    WiFi.begin(ssid, pass);                                   // ขอเชื่อมใหม่ (ไม่บล็อกงานอื่น เพราะอยู่ใน timer)
    lcd.setCursor(0, 1); lcd.print("WiFi: reconnect  ");      // แจ้งสถานะบน LCD
    digitalWrite(STATUS_LED, LOW);                            // ดับไฟสถานะ Blynk
  }
  if (WiFi.status() == WL_CONNECTED && !Blynk.connected()) {  // ถ้า Wi-Fi ติด แต่ Blynk ยังไม่ติด
    Serial.println("Blynk reconnecting...");                  // แจ้งเตือน
    Blynk.connect(3000);                                      // พยายามเชื่อม Blynk โดย timeout 3s (ไม่ค้างนาน)
    if (!Blynk.connected()) {                                 // ถ้ายังไม่ติด
      lcd.setCursor(0, 1); lcd.print("Blynk: offline ");      // แสดงออฟไลน์
      digitalWrite(STATUS_LED, LOW);                          // ดับไฟสถานะ
    }
  }
}

/******************** Helper: show Wi-Fi/Blynk status (suffix on line 2) ********************/
void showNetStatus() {
  lcd.setCursor(11, 1);                                       // พิมพ์ส่วนท้ายของบรรทัดล่าง (คอลัมน์ 11–15)
  if (WiFi.status() == WL_CONNECTED) {                        // ถ้า Wi-Fi ติด
    if (Blynk.connected()) lcd.print("Onln");                 // ติดทั้งคู่ → แสดง Onln (Online)
    else                   lcd.print("WiFi");                 // มี Wi-Fi แต่ Blynk หลุด → WiFi
  } else {
    lcd.print("Off ");                                        // ไม่มี Wi-Fi → Off
  }
}

/******************** Main Loop ********************/
void loop() {
  if (Blynk.connected()) {                                    // ถ้าเชื่อม Blynk ได้
    Blynk.run();                                              // ประมวลผลภายใน Blynk (คอลแบ็ก, sync)
  }
  timer.run();                                                // รันตารางงานตามเวลา (อ่าน/ส่งค่า/รีคอนเน็กต์)
}

/******************** Helper: Telegram Sender (เพิ่ม) ********************/
bool sendTelegramMessage(const String& text) {                 // ส่งข้อความไป Telegram; คืน true ถ้าสำเร็จ
  if (WiFi.status() != WL_CONNECTED) {                         // ถ้า Wi-Fi ยังไม่เชื่อม
    Serial.println("Skip Telegram: no WiFi");                 // ข้ามการส่ง (หลีกเลี่ยงค้าง)
    return false;                                             // ส่งไม่สำเร็จ
  }
  HTTPClient http;                                            // สร้างออบเจกต์ HTTP
  String url = String("https://api.telegram.org/bot") + TELEGRAM_BOT_TOKEN + "/sendMessage"; // Endpoint ของ Telegram Bot
  String body = String("{\"chat_id\":\"") + TELEGRAM_CHAT_ID + "\",\"text\":\"" + text + "\"}"; // สร้าง payload แบบ JSON
  http.begin(url);                                            // เริ่มเชื่อมกับปลายทาง
  http.addHeader("Content-Type", "application/json");         // ตั้ง header เป็น JSON
  int code = http.POST(body);                                 // ส่ง POST พร้อม body
  Serial.print("Telegram HTTP code: "); Serial.println(code); // แสดงรหัสตอบกลับ HTTP
  http.end();                                                 // ปิดการเชื่อมต่อ HTTP
  return (code >= 200 && code < 300);                         // สำเร็จเมื่อรหัส 2xx
}

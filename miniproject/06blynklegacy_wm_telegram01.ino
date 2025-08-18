/******************** Includes ********************/
#define BLYNK_PRINT Serial      // ให้ Blynk พิมพ์ข้อความดีบักออก Serial Monitor
#include <Wire.h>               // ไลบรารี I2C (ใช้กับ LCD)
#include <LiquidCrystal_I2C.h>  // ไลบรารีควบคุม LCD1602 ผ่าน I2C
#include <DHT.h>                // ไลบรารีเซ็นเซอร์ DHT (Adafruit)
#include <WiFi.h>               // ไลบรารี Wi-Fi สำหรับ ESP32
#include <WiFiClient.h>         // ไลบรารี TCP client
#include <BlynkSimpleEsp32.h>   // ไลบรารี Blynk 0.6.x สำหรับ ESP32
#include <HTTPClient.h>         // ไลบรารี HTTPClient ใช้สำหรับส่ง HTTP request (GET/POST) ไปยังเซิร์ฟเวอร์
#include <WiFiClientSecure.h>   // ไลบรารี WiFiClientSecure ใช้สำหรับสร้าง connection แบบ HTTPS (SSL/TLS)
#include <WiFiManager.h>        // <<< ใช้จัดการ Wi-Fi แบบไม่ต้องฮาร์ดโค้ด

/******************** LCD ********************/
LiquidCrystal_I2C lcd(0x27, 16, 2);  // LCD I2C address 0x27 ขนาด 16x2 (บางบอร์ดอาจเป็น 0x3F)

/******************** Pins ********************/
#define SOIL_PIN 35
#define RELAY1_PIN 4
#define RELAY2_PIN 5
#define RELAY3_PIN 18
#define RELAY4_PIN 19
#define DHTPIN 16
#define DHTTYPE DHT11
#define STATUS_LED 2

/******************** Sensors ********************/
DHT dht(DHTPIN, DHTTYPE);

/******************** Soil Calibration ********************/
const int SOIL_DRY_ADC = 3000;
const int SOIL_WET_ADC = 1200;

/******************** Blynk Virtual Pins ********************/
#define VPIN_MODE_SWITCH V20
#define VPIN_SOIL_SETPOINT V21

/******** Relay1 state ********/
volatile bool g_relay1State = false;

/******** โปรโตไทป์ของฟังก์ชันควบคุม Relay1 ********/
void setRelay1(bool on, bool reflectToApp = true);

/******************** Telegram ********************/
const char* TG_BOT_TOKEN = "";            // <- ใส่ Telegram Token
const char* TG_CHAT_ID = "";              // <- ใส่ Telegram ChatID

volatile bool g_autoMode = false;
volatile int g_soilSetpoint = 50;

unsigned long lastBelowMsgMs = 0;
bool sentAboveOnce = false;
const unsigned long BELOW_INTERVAL_MS = 60UL * 1000UL;

/******************** Blynk Credentials ********************/
// <<< ลบ ssid/pass แบบฮาร์ดโค้ดออก
const char auth[] = "HYPzVl1742RKViMCxiIhwIrPMWtOb0jy";                 // Blynk Auth Token (จากแอป Blynk)
const char BLYNK_SERVER_IP[] = "iotservices.thddns.net";     // IP ของ Blynk Local Server
const uint16_t BLYNK_SERVER_PORT = 5535;

/******************** Timers ********************/
BlynkTimer timer;
const unsigned long DHT_INTERVAL_MS = 2000;
const unsigned long SOIL_INTERVAL_MS = 1000;
const unsigned long RECONN_INTERVAL_MS = 5000;

/******************** Forward Declarations ********************/
void taskReadAndDisplay();
void taskSendToBlynk();
void taskReconnect();
void showNetStatus();
void autoControlTask();

/******************** WiFiManager (Global) ********************/
WiFiManager wm;

/******************** Setup ********************/
void setup() {
  Serial.begin(9600);
  dht.begin();

  pinMode(SOIL_PIN, INPUT);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);

  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);
  digitalWrite(RELAY3_PIN, LOW);
  digitalWrite(RELAY4_PIN, LOW);
  setRelay1(false, false);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Booting...");
  lcd.setCursor(0, 1);
  lcd.print("WiFi: connecting");

  // ---------- ใช้ WiFiManager แทนการฮาร์ดโค้ด ----------
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // ถ้ายังไม่เคยตั้งค่า Wi-Fi: จะเปิด AP "ESP32-Setup" รหัส "12345678" ให้เชื่อมและกรอก SSID/PASS
  if (!wm.autoConnect("ESP32-Setup", "12345678")) {
    // เข้าสู่โหมด Config Portal (กรณีผู้ใช้ยังไม่กรอกหรือยกเลิก)
    lcd.setCursor(0, 1);
    lcd.print("AP: ESP32-Setup "); // ความยาวพอดี 16 ตัวอักษร
    // หมายเหตุ: โค้ดจะวนอยู่ที่นี่จนเชื่อม Wi-Fi สำเร็จ (autoConnect เป็นบล็อกกิ้ง)
  } else {
    lcd.setCursor(0, 1);
    lcd.print("WiFi: connected ");
  }
  // ---------------------------------------------------------

  // ตั้งค่าปลายทาง Blynk (ยังไม่ connect ทันที)
  Blynk.config(auth, BLYNK_SERVER_IP, BLYNK_SERVER_PORT);

  timer.setInterval(DHT_INTERVAL_MS, taskReadAndDisplay);
  timer.setInterval(DHT_INTERVAL_MS, taskSendToBlynk);
  timer.setInterval(RECONN_INTERVAL_MS, taskReconnect);
  timer.setInterval(2000, autoControlTask);
}

/******************** Blynk Connected Callback ********************/
BLYNK_CONNECTED() {
  Serial.println("Blynk connected!");
  Blynk.syncAll();
  digitalWrite(STATUS_LED, HIGH);
  lcd.setCursor(0, 1);
  lcd.print("Blynk: online   ");
  Blynk.virtualWrite(V10, g_relay1State ? 1 : 0);
}

BLYNK_WRITE(VPIN_MODE_SWITCH) {
  g_autoMode = (param.asInt() == 1);
  sentAboveOnce = false;
  lastBelowMsgMs = 0;

  if (!g_autoMode) {
    setRelay1(false);
    Serial.println("Mode -> MANUAL, Relay1 forced OFF");
  } else {
    Serial.println("Mode -> AUTO");
  }
}

BLYNK_WRITE(VPIN_SOIL_SETPOINT) {
  g_soilSetpoint = constrain(param.asInt(), 0, 100);
  Serial.printf("Soil Setpoint -> %d%%\n", g_soilSetpoint);
}

/******************** Relay Controls via Blynk (V10–V13) ********************/
BLYNK_WRITE(V10) {
  int st = param.asInt();
  if (g_autoMode) {
    Serial.println("Relay 1 command ignored (AUTO mode)");
    Blynk.virtualWrite(V10, g_relay1State ? 1 : 0);
    return;
  }
  setRelay1(st ? true : false);
  Serial.print("Relay 1 -> ");
  Serial.println(st ? "ON" : "OFF");
}

void setRelay1(bool on, bool reflectToApp) {
  digitalWrite(RELAY1_PIN, on ? HIGH : LOW);
  g_relay1State = on;
  if (reflectToApp && Blynk.connected()) {
    Blynk.virtualWrite(V10, on ? 1 : 0);
  }
}

BLYNK_WRITE(V11) {
  int st = param.asInt();
  digitalWrite(RELAY2_PIN, st);
  Serial.print("Relay 2 -> ");
  Serial.println(st ? "ON" : "OFF");
}

BLYNK_WRITE(V12) {
  int st = param.asInt();
  digitalWrite(RELAY3_PIN, st);
  Serial.print("Relay 3 -> ");
  Serial.println(st ? "ON" : "OFF");
}

BLYNK_WRITE(V13) {
  int st = param.asInt();
  digitalWrite(RELAY4_PIN, st);
  Serial.print("Relay 4 -> ");
  Serial.println(st ? "ON" : "OFF");
}

/******************** Periodic Tasks ********************/
void taskReadAndDisplay() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  int soilRaw = analogRead(SOIL_PIN);
  int span = SOIL_DRY_ADC - SOIL_WET_ADC;
  if (span <= 0) span = 1;
  int soilPct = (int)(((float)(SOIL_DRY_ADC - soilRaw) * 100.0f) / span);
  soilPct = constrain(soilPct, 0, 100);

  Serial.print("T: "); Serial.print(t); Serial.print(" C | ");
  Serial.print("H: "); Serial.print(h); Serial.print(" % | ");
  Serial.print("Soil: "); Serial.print(soilPct); Serial.println(" %");

  lcd.setCursor(0, 0);
  lcd.print("T:");
  if (isnan(t)) lcd.print("--.-"); else lcd.print(t, 1);
  lcd.print((char)223); lcd.print("C ");
  lcd.print("H:");
  if (isnan(h)) lcd.print("--.-"); else lcd.print(h, 0);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print("Soil:");
  lcd.print(soilPct);
  lcd.print("%    ");

  showNetStatus();
}

void taskSendToBlynk() {
  if (!Blynk.connected()) return;
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  int soilRaw = analogRead(SOIL_PIN);
  int span = SOIL_DRY_ADC - SOIL_WET_ADC;
  if (span <= 0) span = 1;
  int soilPct = (int)(((float)(SOIL_DRY_ADC - soilRaw) * 100.0f) / span);
  soilPct = constrain(soilPct, 0, 100);

  if (!isnan(t)) Blynk.virtualWrite(V1, t);
  if (!isnan(h)) Blynk.virtualWrite(V2, h);
  Blynk.virtualWrite(V3, soilPct);
}

void taskReconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected -> reconnecting...");
    WiFi.reconnect();                    // <<< ใช้ค่านิ่งที่ WiFiManager บันทึกไว้
    lcd.setCursor(0, 1);
    lcd.print("WiFi: reconnect  ");
    digitalWrite(STATUS_LED, LOW);
  }
  if (WiFi.status() == WL_CONNECTED && !Blynk.connected()) {
    Serial.println("Blynk reconnecting...");
    Blynk.connect(3000);
    if (!Blynk.connected()) {
      lcd.setCursor(0, 1);
      lcd.print("Blynk: offline ");
      digitalWrite(STATUS_LED, LOW);
    }
  }
}

/******************** Helper: show Wi-Fi/Blynk status ********************/
void showNetStatus() {
  lcd.setCursor(11, 1);
  if (WiFi.status() == WL_CONNECTED) {
    if (Blynk.connected()) lcd.print("Onln");
    else lcd.print("WiFi");
  } else {
    lcd.print("Off ");
  }
}

bool telegramSend(const String& text) {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  String url = "https://api.telegram.org/bot" + String(TG_BOT_TOKEN) + "/sendMessage";
  String payload = "chat_id=" + String(TG_CHAT_ID) + "&text=" + urlencode(text);
  https.begin(client, url);
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int code = https.POST(payload);
  bool ok = (code > 0 && code < 400);
  https.end();
  return ok;
}

String urlencode(const String& s) {
  String out; char c; char bufHex[4];
  for (size_t i = 0; i < s.length(); i++) {
    c = s.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else if (c == ' ') {
      out += '+';
    } else {
      snprintf(bufHex, sizeof(bufHex), "%%%02X", (unsigned char)c);
      out += bufHex;
    }
  }
  return out;
}

void autoControlTask() {
  if (!g_autoMode) return;

  int soilRaw = analogRead(SOIL_PIN);
  int span = SOIL_DRY_ADC - SOIL_WET_ADC;
  if (span <= 0) span = 1;
  int soilPct = (int)(((float)(SOIL_DRY_ADC - soilRaw) * 100.0f) / span);
  soilPct = constrain(soilPct, 0, 100);

  unsigned long now = millis();

  if (soilPct < g_soilSetpoint) {
    setRelay1(true);
    sentAboveOnce = false;

    if (now - lastBelowMsgMs >= BELOW_INTERVAL_MS) {
      String msg = "⚠️ ความชื้นดินต่ำกว่าค่าที่ตั้งไว้\n"
                   "ค่าปัจจุบัน: " + String(soilPct) + "%  <  ค่าที่ตั้งไว้: " + String(g_soilSetpoint) + "%\n"
                   "การทำงาน: เปิดวาล์วน้ำ (Relay1)";
      telegramSend(msg);
      lastBelowMsgMs = now;
    }
  } else {
    setRelay1(false);
    if (!sentAboveOnce) {
      String msg = "✅ ความชื้นดินอยู่ในระดับปกติ\n"
                   "ค่าปัจจุบัน: " + String(soilPct) + "%  ≥  ค่าที่ตั้งไว้: " + String(g_soilSetpoint) + "%\n"
                   "การทำงาน: ปิดวาล์วน้ำ (Relay1)";
      telegramSend(msg);
      sentAboveOnce = true;
    }
    lastBelowMsgMs = 0;
  }
}

/******************** Main Loop ********************/
void loop() {
  if (Blynk.connected()) {
    Blynk.run();
  }
  timer.run();
}

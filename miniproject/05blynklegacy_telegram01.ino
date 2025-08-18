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
/******************** LCD ********************/
LiquidCrystal_I2C lcd(0x27, 16, 2);  // LCD I2C address 0x27 ขนาด 16x2 (บางบอร์ดอาจเป็น 0x3F)

/******************** Pins ********************/
#define SOIL_PIN 35    // ADC1 ช่อง GPIO35 (input-only) สำหรับ Soil
#define RELAY1_PIN 4   // รีเลย์ช่อง 1
#define RELAY2_PIN 5   // รีเลย์ช่อง 2
#define RELAY3_PIN 18  // รีเลย์ช่อง 3
#define RELAY4_PIN 19  // รีเลย์ช่อง 4
#define DHTPIN 16      // ขา DATA ของ DHT
#define DHTTYPE DHT11  // ประเภท DHT: DHT11 (เปลี่ยนเป็น DHT22 ได้)
#define STATUS_LED 2   // GPIO2: เปิดเมื่อเชื่อม Blynk สำเร็จ (ไฟสถานะ)
/******************** Sensors ********************/
DHT dht(DHTPIN, DHTTYPE);  // ออบเจกต์ DHT สำหรับอ่านอุณหภูมิ/ความชื้นอากาศ
/******************** Soil Calibration ********************/
const int SOIL_DRY_ADC = 3000;  // ค่า ADC เมื่อดิน “แห้งมาก” (คาลิเบรตจากของจริง)
const int SOIL_WET_ADC = 1200;  // ค่า ADC เมื่อดิน “เปียกมาก” (คาลิเบรตจากของจริง)
/******************** Blynk Virtual Pins ********************/
#define VPIN_MODE_SWITCH V20    // ปุ่มเลือกโหมด Auto/Manual
#define VPIN_SOIL_SETPOINT V21  // Slider ตั้งค่า Soil Setpoint
/******** Relay1 state (ต้องอยู่ก่อน setup และ BLYNK_WRITE) ********/
volatile bool g_relay1State = false;  // false=ปิด, true=เปิด
/******** โปรโตไทป์ของฟังก์ชันควบคุม Relay1 ********/
void setRelay1(bool on, bool reflectToApp = true);
/******************** Telegram ********************/
const char* TG_BOT_TOKEN = "";  // <- ใส่ Telegram Token
const char* TG_CHAT_ID = "";    // <- ใส่ Telegram ChatID

volatile bool g_autoMode = false;  // โหมด Auto/Manual
volatile int g_soilSetpoint = 50;  // ค่าเริ่มต้น Soil Setpoint = 50%

unsigned long lastBelowMsgMs = 0;                       // สำหรับจำเวลาส่งข้อความเมื่อ Soil < Setpoint
bool sentAboveOnce = false;                             // สำหรับจำว่าขึ้นมามากกว่าแล้วส่งไปแล้วหรือยัง
const unsigned long BELOW_INTERVAL_MS = 60UL * 1000UL;  // 1 นาที

/******************** Blynk Credentials ********************/
const char ssid[] = "";                          // ชื่อ Wi-Fi (ใส่ของคุณ)
const char pass[] = "";                         // รหัสผ่าน Wi-Fi (ใส่ของคุณ)
const char auth[] = "";   // Blynk Auth Token (จากแอป Blynk)
const char BLYNK_SERVER_IP[] = "";  // IP ของ Blynk Local Server
const uint16_t BLYNK_SERVER_PORT = 8080;                  // พอร์ตเซิร์ฟเวอร์ (ปกติ 8080)

/******************** Timers ********************/
BlynkTimer timer;                               // ตัวตั้งเวลา non-blocking ของ Blynk
const unsigned long DHT_INTERVAL_MS = 2000;     // รอบเวลาอ่าน DHT (ms) — DHT11 แนะนำ ≥ 2000ms
const unsigned long SOIL_INTERVAL_MS = 1000;    // รอบเวลาอ่าน Soil (ms)
const unsigned long RECONN_INTERVAL_MS = 5000;  // รอบเวลาลองเชื่อมต่อ Wi-Fi/Blynk ใหม่ (ms)

/******************** Forward Declarations ********************/
void taskReadAndDisplay();  // อ่าน DHT/Soil แล้วอัปเดต Serial + LCD (ทำเสมอ)
void taskSendToBlynk();     // ส่งค่าขึ้น Blynk เมื่อเชื่อมต่อแล้ว
void taskReconnect();       // พยายามเชื่อมต่อ Wi-Fi/Blynk เป็นระยะ
void showNetStatus();       // แสดงสถานะเครือข่ายบน LCD แถวล่าง

/******************** Setup ********************/
void setup() {
  Serial.begin(9600);  // เปิด Serial Monitor ที่ 9600 bps
  dht.begin();         // เริ่มเซ็นเซอร์ DHT

  pinMode(SOIL_PIN, INPUT);     // ตั้งขา Soil เป็นอินพุต
  pinMode(RELAY1_PIN, OUTPUT);  // ตั้งรีเลย์ 1 เป็นเอาต์พุต
  pinMode(RELAY2_PIN, OUTPUT);  // ตั้งรีเลย์ 2 เป็นเอาต์พุต
  pinMode(RELAY3_PIN, OUTPUT);  // ตั้งรีเลย์ 3 เป็นเอาต์พุต
  pinMode(RELAY4_PIN, OUTPUT);  // ตั้งรีเลย์ 4 เป็นเอาต์พุต

  digitalWrite(RELAY1_PIN, LOW);  // ปิดรีเลย์ 1 ตอนเริ่ม (ปรับตาม Active-LOW/HIGH ของบอร์ดจริง)
  digitalWrite(RELAY2_PIN, LOW);  // ปิดรีเลย์ 2 ตอนเริ่ม
  digitalWrite(RELAY3_PIN, LOW);  // ปิดรีเลย์ 3 ตอนเริ่ม
  digitalWrite(RELAY4_PIN, LOW);  // ปิดรีเลย์ 4 ตอนเริ่ม
  setRelay1(false, false);        // ปิด Relay1 ตอนเริ่ม (ยังไม่ sync ไปแอป)
  pinMode(STATUS_LED, OUTPUT);    // ตั้ง GPIO2 เป็นเอาต์พุต
  digitalWrite(STATUS_LED, LOW);  // ดับไฟสถานะไว้ก่อน (ยังไม่ออนไลน์ Blynk)

  lcd.begin();                    // เริ่มต้นจอ LCD (อย่าเรียกใน loop)
  lcd.backlight();                // เปิดไฟพื้นหลัง
  lcd.clear();                    // เคลียร์หน้าจอ
  lcd.setCursor(0, 0);            // ไปแถว 1 คอลัมน์ 0
  lcd.print("Booting...");        // ข้อความบูต
  lcd.setCursor(0, 1);            // ไปแถว 2 คอลัมน์ 0
  lcd.print("WiFi: connecting");  // แจ้งกำลังเชื่อม Wi-Fi

  WiFi.begin(ssid, pass);       // เริ่มเชื่อมต่อ Wi-Fi (ไม่บล็อกงานหลัก)
  WiFi.setAutoReconnect(true);  // ให้ต่อ Wi-Fi ใหม่อัตโนมัติเมื่อหลุด
  WiFi.persistent(true);        // บันทึก config เครือข่ายไว้ถาวร

  Blynk.config(auth, BLYNK_SERVER_IP, BLYNK_SERVER_PORT);  // กำหนดปลายทาง Blynk (ยังไม่ connect ทันที)
  // ใช้ Blynk.config + Blynk.connect(timeout) เพื่อไม่ให้โปรแกรมค้างถ้าเน็ตล่ม

  timer.setInterval(DHT_INTERVAL_MS, taskReadAndDisplay);  // อ่าน/แสดงผลเซ็นเซอร์ทุก 2 วินาที (ทำเสมอ)
  timer.setInterval(DHT_INTERVAL_MS, taskSendToBlynk);     // ส่งค่าไป Blynk ทุก 2 วินาที (เฉพาะเมื่อเชื่อมต่อ)
  timer.setInterval(RECONN_INTERVAL_MS, taskReconnect);    // พยายามเชื่อมต่อ Wi-Fi/Blynk ทุก 5 วินาที
  timer.setInterval(2000, autoControlTask);                // ตรวจค่า Soil% ทุก 2 วินาที
}

/******************** Blynk Connected Callback ********************/
BLYNK_CONNECTED() {
  Serial.println("Blynk connected!");  // พิมพ์ข้อความออก Serial Monitor เมื่อเชื่อม Blynk สำเร็จ
  Blynk.syncAll();                     // ดึงค่าล่าสุดจาก Virtual Pin ทุกตัวในแอป Blynk มาให้บอร์ด (sync state)
  digitalWrite(STATUS_LED, HIGH);      // เปิดไฟ LED สถานะ (GPIO2) บอกว่าตอนนี้ออนไลน์แล้ว
  lcd.setCursor(0, 1);                 // ย้าย cursor ไปบรรทัดที่ 2 คอลัมน์ 0 บนจอ LCD
  lcd.print("Blynk: online   ");       // แสดงข้อความว่า Blynk ออนไลน์ (เติมช่องว่างท้ายไว้เคลียร์เศษอักษร)

  // สะท้อนสถานะจริงของ Relay1 ไปที่ปุ่ม V10
  Blynk.virtualWrite(V10, g_relay1State ? 1 : 0);  // ถ้า Relay1 เปิด → ส่งค่า 1 ไปที่ปุ่ม V10, ถ้าปิด → ส่งค่า 0
}

BLYNK_WRITE(VPIN_MODE_SWITCH) {       // ฟังก์ชัน callback เมื่อผู้ใช้กดปุ่ม V20 (Auto/Manual) บนแอป
  g_autoMode = (param.asInt() == 1);  // ถ้าค่าที่ได้ = 1 → เข้าโหมด Auto, ถ้า = 0 → Manual
  sentAboveOnce = false;              // รีเซ็ต flag การส่งข้อความ (ฝั่ง Soil ≥ Setpoint)
  lastBelowMsgMs = 0;                 // รีเซ็ตตัวนับเวลาแจ้งเตือน Soil ต่ำกว่า setpoint

  if (!g_autoMode) {                                      // ถ้าเป็น Manual mode
    setRelay1(false);                                     // ปิด Relay1 ทันที (Active-High → LOW = OFF) + sync ปุ่ม V10
    Serial.println("Mode -> MANUAL, Relay1 forced OFF");  // พิมพ์ log ใน Serial Monitor
  } else {                                                // ถ้าเป็น Auto mode
    Serial.println("Mode -> AUTO");                       // พิมพ์ log บอกว่าเข้าโหมด Auto
  }
}

BLYNK_WRITE(VPIN_SOIL_SETPOINT) {                            // ฟังก์ชัน callback เมื่อ Slider (V21) มีการเปลี่ยนค่า
  g_soilSetpoint = constrain(param.asInt(), 0, 100);         // ดึงค่าจากแอป (int) แล้วบังคับให้อยู่ในช่วง 0–100 (%)
  Serial.printf("Soil Setpoint -> %d%%\n", g_soilSetpoint);  // พิมพ์ค่า setpoint ที่ได้รับออก Serial Monitor
}

/******************** Relay Controls via Blynk (V10–V13) ********************/
BLYNK_WRITE(V10) {         // ฟังก์ชัน callback เรียกเมื่อผู้ใช้กดปุ่ม V10 (Relay1) บนแอป
  int st = param.asInt();  // อ่านค่าจากปุ่ม: 0 = OFF, 1 = ON

  if (g_autoMode) {                                         // ถ้าอยู่ในโหมด Auto
    Serial.println("Relay 1 command ignored (AUTO mode)");  // แจ้งใน Serial ว่าคำสั่งปุ่มถูกเมินเพราะเป็น Auto

    // ส่งสถานะรีเลย์จริงกลับไปที่ปุ่ม V10 บนแอป
    // เพื่อไม่ให้ปุ่มค้างผิดกับสถานะจริง (เช่นกด ON แต่รีเลย์ไม่ทำงาน)
    Blynk.virtualWrite(V10, g_relay1State ? 1 : 0);
    return;  // ออกจากฟังก์ชันทันที ไม่สั่งรีเลย์
  }

  // ถ้าอยู่ในโหมด Manual → ยอมให้กดปุ่มควบคุมรีเลย์ได้
  setRelay1(st ? true : false);  // สั่งเปิดถ้า st=1, ปิดถ้า st=0 (ฟังก์ชันนี้ sync ปุ่มอัตโนมัติด้วย)

  // แสดงสถานะลง Serial Monitor เพื่อ debug
  Serial.print("Relay 1 → ");
  Serial.println(st ? "ON" : "OFF");
}


/******** ฟังก์ชันควบคุม Relay1 + sync ปุ่ม V10 ********/
void setRelay1(bool on, bool reflectToApp) {  // ฟังก์ชันควบคุม Relay1 พร้อมเลือกว่าจะ sync ปุ่มในแอปด้วยหรือไม่
  digitalWrite(RELAY1_PIN, on ? HIGH : LOW);  // ถ้า on=true → HIGH (Active-High = เปิด), ถ้า on=false → LOW (ปิด)
  g_relay1State = on;                         // เก็บสถานะรีเลย์ล่าสุดไว้ในตัวแปร global

  if (reflectToApp && Blynk.connected()) {  // ถ้าสั่งให้สะท้อนสถานะ (reflectToApp=true) และบอร์ดเชื่อมต่อ Blynk อยู่
    Blynk.virtualWrite(V10, on ? 1 : 0);    // ส่งค่า 1/0 ไปยังปุ่ม V10 ในแอป Blynk → ปุ่มแสดงตรงกับรีเลย์จริง
  }
}

BLYNK_WRITE(V11) {               // ควบคุมรีเลย์ 2 ที่ V11
  int st = param.asInt();        // รับค่า 0/1
  digitalWrite(RELAY2_PIN, st);  // เขียนสถานะถึงรีเลย์ 2
  Serial.print("Relay 2 → ");
  Serial.println(st ? "ON" : "OFF");  // ดีบักสถานะ
}

BLYNK_WRITE(V12) {               // ควบคุมรีเลย์ 3 ที่ V12
  int st = param.asInt();        // รับค่า 0/1
  digitalWrite(RELAY3_PIN, st);  // เขียนสถานะถึงรีเลย์ 3
  Serial.print("Relay 3 → ");
  Serial.println(st ? "ON" : "OFF");  // ดีบักสถานะ
}

BLYNK_WRITE(V13) {               // ควบคุมรีเลย์ 4 ที่ V13
  int st = param.asInt();        // รับค่า 0/1
  digitalWrite(RELAY4_PIN, st);  // เขียนสถานะถึงรีเลย์ 4
  Serial.print("Relay 4 → ");
  Serial.println(st ? "ON" : "OFF");  // ดีบักสถานะ
}

/******************** Periodic Tasks ********************/
void taskReadAndDisplay() {
  float h = dht.readHumidity();                                            // อ่านความชื้นอากาศ (%RH)
  float t = dht.readTemperature();                                         // อ่านอุณหภูมิ (°C)
  int soilRaw = analogRead(SOIL_PIN);                                      // อ่านค่า ADC ของ Soil (0–4095)
  int span = SOIL_DRY_ADC - SOIL_WET_ADC;                                  // ช่วงคาลิเบรต (แห้ง-เปียก)
  if (span <= 0) span = 1;                                                 // ป้องกันหารด้วยศูนย์
  int soilPct = (int)(((float)(SOIL_DRY_ADC - soilRaw) * 100.0f) / span);  // map ดิบ→%
  soilPct = constrain(soilPct, 0, 100);                                    // จำกัดช่วง 0–100%

  Serial.print("T: ");
  Serial.print(t);
  Serial.print(" C | ");  // พิมพ์อุณหภูมิลง Serial
  Serial.print("H: ");
  Serial.print(h);
  Serial.print(" % | ");  // พิมพ์ความชื้นอากาศลง Serial
  Serial.print("Soil: ");
  Serial.print(soilPct);
  Serial.println(" %");  // พิมพ์ Soil% ลง Serial

  lcd.setCursor(0, 0);  // ไปต้นบรรทัดแรก
  lcd.print("T:");      // ป้ายกำกับอุณหภูมิ
  if (isnan(t)) lcd.print("--.-");
  else lcd.print(t, 1);  // ถ้าอ่านไม่ได้แสดง --.- มิฉะนั้นแสดงทศนิยม 1 ตำแหน่ง
  lcd.print((char)223);
  lcd.print("C ");  // สัญลักษณ์ °C
  lcd.print("H:");  // ป้ายกำกับความชื้น
  if (isnan(h)) lcd.print("--.-");
  else lcd.print(h, 0);  // ถ้าอ่านไม่ได้แสดง --.- มิฉะนั้นแสดงจำนวนเต็ม
  lcd.print("%");        // หน่วย %

  lcd.setCursor(0, 1);  // ไปบรรทัดล่าง
  lcd.print("Soil:");   // ป้ายกำกับ Soil
  lcd.print(soilPct);   // แสดง Soil%
  lcd.print("%    ");   // เติมช่องว่างเพื่อเคลียร์เศษตัวอักษรค้าง

  showNetStatus();  // อัปเดตสถานะเครือข่าย (เติมท้ายบรรทัดล่างให้พอดี)
}

void taskSendToBlynk() {
  if (!Blynk.connected()) return;      // ถ้ายังไม่เชื่อม Blynk ให้ข้าม (ทำงานออฟไลน์ต่อ)
  float h = dht.readHumidity();        // อ่านความชื้นอากาศ
  float t = dht.readTemperature();     // อ่านอุณหภูมิ
  int soilRaw = analogRead(SOIL_PIN);  // อ่าน Soil ADC
  int span = SOIL_DRY_ADC - SOIL_WET_ADC;
  if (span <= 0) span = 1;                                                 // กันหาร 0
  int soilPct = (int)(((float)(SOIL_DRY_ADC - soilRaw) * 100.0f) / span);  // map → %
  soilPct = constrain(soilPct, 0, 100);                                    // จำกัดช่วง

  if (!isnan(t)) Blynk.virtualWrite(V1, t);  // ส่งอุณหภูมิไป V1 (ถ้าอ่านได้)
  if (!isnan(h)) Blynk.virtualWrite(V2, h);  // ส่งความชื้นไป V2 (ถ้าอ่านได้)
  Blynk.virtualWrite(V3, soilPct);           // ส่ง Soil% ไป V3
}

void taskReconnect() {
  if (WiFi.status() != WL_CONNECTED) {                       // ถ้า Wi-Fi หลุด
    Serial.println("Wi-Fi disconnected → reconnecting...");  // แจ้งเตือน
    WiFi.begin(ssid, pass);                                  // ขอเชื่อมใหม่ (ไม่บล็อกงานอื่น เพราะอยู่ใน timer)
    lcd.setCursor(0, 1);
    lcd.print("WiFi: reconnect  ");  // แจ้งสถานะบน LCD
    digitalWrite(STATUS_LED, LOW);   // ดับไฟสถานะ Blynk
  }
  if (WiFi.status() == WL_CONNECTED && !Blynk.connected()) {  // ถ้า Wi-Fi ติด แต่ Blynk ยังไม่ติด
    Serial.println("Blynk reconnecting...");                  // แจ้งเตือน
    Blynk.connect(3000);                                      // พยายามเชื่อม Blynk โดย timeout 3s (ไม่ค้างนาน)
    if (!Blynk.connected()) {                                 // ถ้ายังไม่ติด
      lcd.setCursor(0, 1);
      lcd.print("Blynk: offline ");   // แสดงออฟไลน์
      digitalWrite(STATUS_LED, LOW);  // ดับไฟสถานะ
    }
  }
}

/******************** Helper: show Wi-Fi/Blynk status (suffix on line 2) ********************/
void showNetStatus() {
  lcd.setCursor(11, 1);                        // พิมพ์ส่วนท้ายของบรรทัดล่าง (คอลัมน์ 11–15)
  if (WiFi.status() == WL_CONNECTED) {         // ถ้า Wi-Fi ติด
    if (Blynk.connected()) lcd.print("Onln");  // ติดทั้งคู่ → แสดง Onln (Online)
    else lcd.print("WiFi");                    // มี Wi-Fi แต่ Blynk หลุด → WiFi
  } else {
    lcd.print("Off ");  // ไม่มี Wi-Fi → Off
  }
}

bool telegramSend(const String& text) {             // ฟังก์ชันส่งข้อความ text ไปยัง Telegram
  if (WiFi.status() != WL_CONNECTED) return false;  // ถ้า WiFi ยังไม่เชื่อมต่อ → ยกเลิกและ return false

  WiFiClientSecure client;  // สร้าง client แบบ secure (HTTPS)
  client.setInsecure();     // ปิดการตรวจสอบใบรับรอง SSL (ง่ายสำหรับ IoT)

  HTTPClient https;  // ออบเจกต์สำหรับจัดการ HTTP/HTTPS
  String url = "https://api.telegram.org/bot"
               + String(TG_BOT_TOKEN)
               + "/sendMessage";  // URL ของ Telegram Bot API

  String payload = "chat_id=" + String(TG_CHAT_ID)
                   + "&text=" + urlencode(text);  // ข้อความที่จะส่ง (chat_id + ข้อความ)

  https.begin(client, url);                                              // เริ่มการเชื่อมต่อ HTTPS ไปยัง API ของ Telegram
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");  // กำหนด header ของ request
  int code = https.POST(payload);                                        // ส่งข้อมูลแบบ POST ไปยัง Telegram API
  bool ok = (code > 0 && code < 400);                                    // ถ้าส่งสำเร็จ (HTTP code 200–399) ให้ ok=true
  https.end();                                                           // ปิดการเชื่อมต่อ
  return ok;                                                             // คืนค่า true/false บอกว่าส่งสำเร็จหรือไม่
}

String urlencode(const String& s) {  // ฟังก์ชันรับ String s (ข้อความดิบ) แล้วคืนค่าเป็น URL encoded
  String out;                        // ตัวแปร String สำหรับเก็บข้อความที่ encode แล้ว
  char c;                            // ตัวแปรเก็บอักขระปัจจุบัน
  char bufHex[4];                    // buffer สำหรับเก็บตัวอักษรแบบ %XX (เช่น %20)

  for (size_t i = 0; i < s.length(); i++) {  // วนลูปตรวจทีละตัวอักษรในข้อความ
    c = s.charAt(i);                         // ดึงอักขระตำแหน่ง i

    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {  // ถ้าเป็น a-z, A-Z, 0-9 หรืออักขระที่ URL อนุญาต
      out += c;                                                        // ใช้ตัวอักษรนั้นตรง ๆ
    } else if (c == ' ') {                                             // ถ้าเป็นช่องว่าง (space)
      out += '+';                                                      // แทนด้วยเครื่องหมาย + (มาตรฐานของ x-www-form-urlencoded)
    } else {                                                           // กรณีอื่น ๆ เช่น !, @, #, อักษรไทย, เครื่องหมายพิเศษ
      snprintf(bufHex, sizeof(bufHex), "%%%02X", (unsigned char)c);    // แปลงเป็นรหัส hex เช่น %20, %E0%B8
      out += bufHex;                                                   // ต่อเข้ากับข้อความผลลัพธ์
    }
  }
  return out;  // คืนค่าข้อความที่ encode แล้ว
}


void autoControlTask() {
  if (!g_autoMode) return;  // ถ้าไม่ใช่โหมด Auto → ออกจากฟังก์ชันทันที

  int soilRaw = analogRead(SOIL_PIN);                                      // อ่านค่า ADC จาก Soil sensor
  int span = SOIL_DRY_ADC - SOIL_WET_ADC;                                  // หาช่วงคาลิเบรต (ค่าดินแห้ง - ค่าดินเปียก)
  if (span <= 0) span = 1;                                                 // ป้องกันหารด้วยศูนย์
  int soilPct = (int)(((float)(SOIL_DRY_ADC - soilRaw) * 100.0f) / span);  // map ค่าดิบเป็น %
  soilPct = constrain(soilPct, 0, 100);                                    // บังคับค่าให้อยู่ระหว่าง 0–100%

  unsigned long now = millis();  // เวลา ณ ปัจจุบัน (ms)

  if (soilPct < g_soilSetpoint) {  // ถ้าความชื้นดิน < ค่าที่ตั้งไว้
    setRelay1(true);               // เปิด Relay1 (Active-High → HIGH=ON) + sync ปุ่ม V10
    sentAboveOnce = false;         // รีเซ็ต flag การส่งข้อความฝั่งสูงกว่า

    if (now - lastBelowMsgMs >= BELOW_INTERVAL_MS) {                                                                         // ถ้าห่างจากครั้งก่อน ≥ 1 นาที
      String msg = "⚠️ ความชื้นดินต่ำกว่าค่าที่ตั้งไว้\n"  // สร้างข้อความแจ้งเตือน
                   "ค่าปัจจุบัน: "
                   + String(soilPct) + "%  <  ค่าที่ตั้งไว้: " + String(g_soilSetpoint) + "%\n"
                                                                                    "การทำงาน: เปิดวาล์วน้ำ (Relay1)";
      telegramSend(msg);     // ส่งข้อความไป Telegram
      lastBelowMsgMs = now;  // อัปเดตเวลาส่งครั้งล่าสุด
    }
  } else {             // ถ้าความชื้นดิน ≥ ค่าที่ตั้งไว้
    setRelay1(false);  // ปิด Relay1 (Active-High → LOW=OFF) + sync ปุ่ม V10

    if (!sentAboveOnce) {                     // ถ้ายังไม่ได้ส่งแจ้งเตือนฝั่งสูง
      String msg = "✅ ความชื้นดินอยู่ในระดับปกติ\n"  // สร้างข้อความแจ้งเตือน
                   "ค่าปัจจุบัน: "
                   + String(soilPct) + "%  ≥  ค่าที่ตั้งไว้: " + String(g_soilSetpoint) + "%\n"
                                                                                    "การทำงาน: ปิดวาล์วน้ำ (Relay1)";
      telegramSend(msg);     // ส่งข้อความไป Telegram
      sentAboveOnce = true;  // ตั้ง flag ว่าแจ้งไปแล้ว
    }
    lastBelowMsgMs = 0;  // รีเซ็ตเวลาแจ้งเตือนฝั่งต่ำ (เพื่อพร้อมส่งใหม่)
  }
}

/******************** Main Loop ********************/
void loop() {
  if (Blynk.connected()) {  // ถ้าเชื่อม Blynk ได้
    Blynk.run();            // ประมวลผลภายใน Blynk (คอลแบ็ก, sync)
  }
  timer.run();  // รันตารางงานตามเวลา (อ่าน/ส่งค่า/รีคอนเน็กต์)
}

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
#define SOIL_PIN 35    // ขาอินพุตสำหรับเซ็นเซอร์ความชื้นดิน (ADC1 GPIO35, เป็น input-only)
#define RELAY1_PIN 4   // ขาควบคุมรีเลย์ช่อง 1 (ขึ้นกับบอร์ดรีเลย์; โค้ดนี้ใช้ HIGH=ON, LOW=OFF)
#define RELAY2_PIN 5   // ขาควบคุมรีเลย์ช่อง 2
#define RELAY3_PIN 18  // ขาควบคุมรีเลย์ช่อง 3
#define RELAY4_PIN 19  // ขาควบคุมรีเลย์ช่อง 4
#define DHTPIN 16      // ขา DATA ของเซ็นเซอร์ DHT
#define DHTTYPE DHT11  // ประเภทของเซ็นเซอร์ DHT ที่ใช้งาน (DHT11)
#define STATUS_LED 2   // ขาไฟสถานะบนบอร์ด (เช่น GPIO2 ของ ESP32)

/******************** Sensors ********************/
DHT dht(DHTPIN, DHTTYPE);  // สร้างออบเจกต์ DHT สำหรับอ่านค่าอุณหภูมิ (°C) และความชื้นอากาศ (%RH)

/******************** Soil Calibration ********************/
const int SOIL_DRY_ADC = 3000;  // ค่าดิบ ADC เมื่อดิน “แห้งมาก” (ต้องคาลิเบรตจากหน้างานจริง)
const int SOIL_WET_ADC = 1200;  // ค่าดิบ ADC เมื่อดิน “เปียกมาก” (ต้องคาลิเบรตจากหน้างานจริง)

/******************** Blynk Virtual Pins ********************/
#define VPIN_MODE_SWITCH V20    // ปุ่มเลือกโหมด Auto/Manual บนแอป Blynk
#define VPIN_SOIL_SETPOINT V21  // สไลเดอร์ตั้งค่าเซ็ตพอยต์ความชื้นดินเป็นเปอร์เซ็นต์ (%)

/******** Relay1 state ********/
volatile bool g_relay1State = false;  // เก็บสถานะรีเลย์ 1 (true=ON/false=OFF); ใช้ volatile เพราะถูกเข้าถึงจากหลายคอลแบ็ก

/******** โปรโตไทป์ของฟังก์ชันควบคุม Relay1 ********/
void setRelay1(bool on, bool reflectToApp = true);  // ฟังก์ชันสั่งเปิด/ปิดรีเลย์ 1; reflectToApp=true จะ sync สถานะกลับไปที่แอป (V10)

/******************** Telegram ********************/
const char* TG_BOT_TOKEN = "";  // <- ใส่ Telegram Token
const char* TG_CHAT_ID = "";                                        // <- ใส่ Telegram ChatID

volatile bool g_autoMode = false;  // โหมดการทำงาน: true=Auto, false=Manual (volatile เพราะอาจถูกเปลี่ยนในคอลแบ็ก/ทาสก์)
volatile int g_soilSetpoint = 50;  // ค่าเซ็ตพอยต์ความชื้นดินเริ่มต้นเป็นเปอร์เซ็นต์ (0–100%)

unsigned long lastBelowMsgMs = 0;                       // เวลา (millis) ที่ส่งแจ้งเตือนครั้งล่าสุดเมื่อค่า Soil% ต่ำกว่าเซ็ตพอยต์
bool sentAboveOnce = false;                             // ธงว่าได้ส่งแจ้งเตือน “ค่าปกติ/มากกว่าเซ็ตพอยต์” ไปแล้ว 1 ครั้งหรือยัง (กันสแปม)
const unsigned long BELOW_INTERVAL_MS = 60UL * 1000UL;  // ช่วงห่างการส่งซ้ำเมื่อค่าต่ำกว่าเซ็ตพอยต์: 60 วินาที (หน่วยมิลลิวินาที)

/******************** Blynk Credentials ********************/
// <<< ลบ ssid/pass แบบฮาร์ดโค้ดออก
const char auth[] = "";   // Blynk Auth Token (จากแอป Blynk)
const char BLYNK_SERVER_IP[] = "";  // IP ของ Blynk Local Server
const uint16_t BLYNK_SERVER_PORT = 8080;                  // พอร์ตเชื่อมต่อ Blynk Server (ต้องตรงกับการตั้งค่าที่เซิร์ฟเวอร์)

/******************** Timers ********************/
BlynkTimer timer;                               // ตัวจัดตารางงานแบบไม่บล็อก (non-blocking) ของ Blynk
const unsigned long DHT_INTERVAL_MS = 2000;     // รอบเวลาอ่าน/แสดงผล DHT ทุก 2 วินาที (มิลลิวินาที)
const unsigned long SOIL_INTERVAL_MS = 1000;    // รอบเวลาอ่านเซ็นเซอร์ความชื้นดินทุก 1 วินาที
const unsigned long RECONN_INTERVAL_MS = 5000;  // รอบเวลาเช็ก/พยายามเชื่อมต่อ Wi-Fi/Blynk ใหม่ทุก 5 วินาที

/******************** Forward Declarations ********************/
void taskReadAndDisplay();  // งานอ่าน DHT/Soil แล้วอัปเดต Serial + LCD ทุกคาบ
void taskSendToBlynk();     // งานส่งค่าขึ้น Blynk (ทำเมื่อเชื่อมต่อ Blynk แล้วเท่านั้น)
void taskReconnect();       // งานตรวจสถานะและพยายาม reconnect Wi-Fi/Blynk ตาม RECONN_INTERVAL_MS
void showNetStatus();       // แสดงสถานะเครือข่าย (Onln/WiFi/Off) ต่อท้ายบรรทัดล่างของ LCD
void autoControlTask();     // ลอจิกโหมด Auto: เปรียบเทียบ Soil% กับ setpoint แล้วสั่งเปิด/ปิด Relay1

/******************** WiFiManager (Global) ********************/
WiFiManager wm;  // ออบเจกต์ WiFiManager ระดับโกลบอล (ดูแล Config Portal และบันทึก SSID/PASS ลง NVS)

/******************** Setup ********************/
void setup() {         // ฟังก์ชันเริ่มต้น รันครั้งเดียวเมื่อบอร์ดบูต
  Serial.begin(9600);  // เปิด Serial Monitor ที่ 9600 bps สำหรับดีบัก
  dht.begin();         // เริ่มต้นเซ็นเซอร์ DHT (เตรียมอ่านอุณหภูมิ/ความชื้น)

  pinMode(SOIL_PIN, INPUT);     // กำหนดขา Soil sensor เป็นอินพุต (อ่านค่า ADC)
  pinMode(RELAY1_PIN, OUTPUT);  // ตั้งขารีเลย์ 1 เป็นเอาต์พุต
  pinMode(RELAY2_PIN, OUTPUT);  // ตั้งขารีเลย์ 2 เป็นเอาต์พุต
  pinMode(RELAY3_PIN, OUTPUT);  // ตั้งขารีเลย์ 3 เป็นเอาต์พุต
  pinMode(RELAY4_PIN, OUTPUT);  // ตั้งขารีเลย์ 4 เป็นเอาต์พุต

  digitalWrite(RELAY1_PIN, LOW);  // ปิดรีเลย์ 1 ตอนเริ่ม (ป้องกันสั่งทำงานโดยไม่ตั้งใจ)
  digitalWrite(RELAY2_PIN, LOW);  // ปิดรีเลย์ 2 ตอนเริ่ม
  digitalWrite(RELAY3_PIN, LOW);  // ปิดรีเลย์ 3 ตอนเริ่ม
  digitalWrite(RELAY4_PIN, LOW);  // ปิดรีเลย์ 4 ตอนเริ่ม
  setRelay1(false, false);        // ยืนยันสถานะรีเลย์ 1 = ปิด และไม่ซิงก์กลับแอป (reflectToApp=false)
  pinMode(STATUS_LED, OUTPUT);    // ตั้งขาไฟสถานะ (เช่น GPIO2) เป็นเอาต์พุต
  digitalWrite(STATUS_LED, LOW);  // ดับไฟสถานะไว้ก่อน (ยังไม่ออนไลน์ Blynk)

  lcd.begin();                    // เริ่มต้นการทำงานของจอ LCD 16x2
  lcd.backlight();                // เปิดไฟพื้นหลังจอ LCD
  lcd.clear();                    // เคลียร์หน้าจอ LCD
  lcd.setCursor(0, 0);            // ย้ายเคอร์เซอร์ไปบรรทัดที่ 1 คอลัมน์ 0
  lcd.print("Booting...");        // แสดงข้อความขณะเริ่มระบบ
  lcd.setCursor(0, 1);            // ย้ายเคอร์เซอร์ไปบรรทัดที่ 2 คอลัมน์ 0
  lcd.print("WiFi: connecting");  // แจ้งสถานะกำลังเชื่อมต่อ Wi-Fi


  // ---------- ใช้ WiFiManager แทนการฮาร์ดโค้ด ----------
  WiFi.setAutoReconnect(true);  // ให้ ESP32 เชื่อม Wi-Fi ใหม่อัตโนมัติเมื่อหลุด
  WiFi.persistent(true);        // บันทึกคอนฟิกเครือข่าย (SSID/PASS) ลง NVS ให้คงอยู่หลังรีบูต

  // ถ้ายังไม่เคยตั้งค่า Wi-Fi: จะเปิด AP "ESP32-Setup" รหัส "12345678" ให้เชื่อมและกรอก SSID/PASS
  if (!wm.autoConnect("ESP32-Setup", "12345678")) {  // ถ้าเชื่อม Wi-Fi ไม่สำเร็จ → เข้าสู่โหมด Config Portal (AP ชื่อ ESP32-Setup)
    // เข้าสู่โหมด Config Portal (กรณีผู้ใช้ยังไม่กรอกหรือยกเลิก)
    lcd.setCursor(0, 1);            // ย้ายเคอร์เซอร์ไปบรรทัดล่างของจอ LCD
    lcd.print("AP: ESP32-Setup ");  // แสดงข้อความบอกสถานะว่าอยู่ในโหมด AP/ตั้งค่า (ยาวพอดี 16 ตัวอักษร)
    // หมายเหตุ: โค้ดจะวนอยู่ที่นี่จนเชื่อม Wi-Fi สำเร็จ (autoConnect เป็นบล็อกกิ้ง)
  } else {
    lcd.setCursor(0, 1);            // ย้ายเคอร์เซอร์ไปบรรทัดล่างของจอ LCD
    lcd.print("WiFi: connected ");  // แสดงว่าต่อ Wi-Fi สำเร็จแล้ว
  }
  // ---------------------------------------------------------

  // ตั้งค่าปลายทาง Blynk (ยังไม่ connect ทันที)
  Blynk.config(auth, BLYNK_SERVER_IP, BLYNK_SERVER_PORT);  // กำหนดโทเค็น/ไอพี/พอร์ตของ Blynk Server เพื่อใช้เชื่อมต่อภายหลัง

  timer.setInterval(DHT_INTERVAL_MS, taskReadAndDisplay);  // เรียกงานอ่าน/แสดงผล DHT+Soil เป็นระยะ ๆ (ทุก DHT_INTERVAL_MS)
  timer.setInterval(DHT_INTERVAL_MS, taskSendToBlynk);     // ส่งค่าขึ้น Blynk เป็นระยะ ๆ (เมื่อเชื่อมต่อแล้ว)
  timer.setInterval(RECONN_INTERVAL_MS, taskReconnect);    // ตรวจและพยายามเชื่อม Wi-Fi/Blynk ใหม่ทุก RECONN_INTERVAL_MS
  timer.setInterval(2000, autoControlTask);                // ลอจิกโหมด Auto: ตรวจ Soil% เทียบ setpoint ทุก 2000 ms
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

BLYNK_WRITE(VPIN_SOIL_SETPOINT) {                            // คอลแบ็กเมื่อสไลเดอร์ตั้งค่าเซ็ตพอยต์ความชื้นดิน (V21) เปลี่ยนค่า
  g_soilSetpoint = constrain(param.asInt(), 0, 100);         // อ่านค่าเป็น int แล้วบีบให้อยู่ช่วง 0–100 (%)
  Serial.printf("Soil Setpoint -> %d%%\n", g_soilSetpoint);  // พิมพ์ค่า setpoint ที่ตั้งใหม่ออก Serial เพื่อดีบัก
}

/******************** Relay Controls via Blynk (V10–V13) ********************/
BLYNK_WRITE(V10) {                                          // คอลแบ็กเมื่อผู้ใช้กดปุ่มควบคุมรีเลย์ 1 (V10) บนแอป
  int st = param.asInt();                                   // อ่านสถานะปุ่ม: 0=OFF, 1=ON
  if (g_autoMode) {                                         // ถ้าอยู่ในโหมด Auto
    Serial.println("Relay 1 command ignored (AUTO mode)");  // แจ้งว่าไม่รับคำสั่งกดปุ่ม (ให้ลอจิกอัตโนมัติควบคุม)
    Blynk.virtualWrite(V10, g_relay1State ? 1 : 0);         // ซิงก์สถานะปุ่มกลับให้ตรงกับรีเลย์จริง
    return;                                                 // ออกจากฟังก์ชัน ไม่สั่งรีเลย์
  }
  setRelay1(st ? true : false);       // โหมด Manual: สั่งเปิดถ้า st=1, ปิดถ้า st=0
  Serial.print("Relay 1 -> ");        // พิมพ์หัวข้อความเปลี่ยนแปลงสถานะรีเลย์ 1
  Serial.println(st ? "ON" : "OFF");  // ต่อท้ายด้วยข้อความ ON/OFF
}

void setRelay1(bool on, bool reflectToApp) {  // ฟังก์ชันควบคุมรีเลย์ 1; reflectToApp=true จะส่งสถานะกลับไปที่แอป
  digitalWrite(RELAY1_PIN, on ? HIGH : LOW);  // เขียนสัญญาณไปยังขารีเลย์ 1 (HIGH=เปิด, LOW=ปิด ตามบอร์ดที่ใช้)
  g_relay1State = on;                         // บันทึกสถานะล่าสุดของรีเลย์ 1 ไว้ในตัวแปรโกลบอล
  if (reflectToApp && Blynk.connected()) {    // หากต้องสะท้อนสถานะ และขณะนั้นเชื่อม Blynk อยู่
    Blynk.virtualWrite(V10, on ? 1 : 0);      // อัปเดตปุ่ม V10 บนแอปให้ตรงกับสถานะจริงของรีเลย์ 1
  }
}

BLYNK_WRITE(V11) {                    // คอลแบ็กเมื่อผู้ใช้กดปุ่มควบคุมรีเลย์ 2 (V11) บนแอป
  int st = param.asInt();             // อ่านค่าจากปุ่ม: 0 = OFF, 1 = ON
  digitalWrite(RELAY2_PIN, st);       // สั่งรีเลย์ 2 ตามค่า st (HIGH=ON, LOW=OFF)
  Serial.print("Relay 2 -> ");        // พิมพ์ข้อความนำหน้าใน Serial เพื่อดีบัก
  Serial.println(st ? "ON" : "OFF");  // แสดงผลสถานะ ON/OFF ของรีเลย์ 2
}

BLYNK_WRITE(V12) {                    // คอลแบ็กเมื่อผู้ใช้กดปุ่มควบคุมรีเลย์ 3 (V12) บนแอป
  int st = param.asInt();             // อ่านค่าจากปุ่ม: 0 = OFF, 1 = ON
  digitalWrite(RELAY3_PIN, st);       // สั่งรีเลย์ 3 ตามค่า st (HIGH=ON, LOW=OFF)
  Serial.print("Relay 3 -> ");        // พิมพ์ข้อความนำหน้าใน Serial เพื่อดีบัก
  Serial.println(st ? "ON" : "OFF");  // แสดงผลสถานะ ON/OFF ของรีเลย์ 3
}

BLYNK_WRITE(V13) {                    // คอลแบ็กเมื่อผู้ใช้กดปุ่มควบคุมรีเลย์ 4 (V13) บนแอป
  int st = param.asInt();             // อ่านค่าจากปุ่ม: 0 = OFF, 1 = ON
  digitalWrite(RELAY4_PIN, st);       // สั่งรีเลย์ 4 ตามค่า st (HIGH=ON, LOW=OFF)
  Serial.print("Relay 4 -> ");        // พิมพ์ข้อความนำหน้าใน Serial เพื่อดีบัก
  Serial.println(st ? "ON" : "OFF");  // แสดงผลสถานะ ON/OFF ของรีเลย์ 4
}

/******************** Periodic Tasks ********************/
void taskReadAndDisplay() {                                                // งานอ่านค่าเซ็นเซอร์แล้วแสดงผลบน Serial + LCD
  float h = dht.readHumidity();                                            // อ่านความชื้นอากาศจาก DHT (%RH)
  float t = dht.readTemperature();                                         // อ่านอุณหภูมิอากาศจาก DHT (°C)
  int soilRaw = analogRead(SOIL_PIN);                                      // อ่านค่าดิบ ADC ของเซ็นเซอร์ความชื้นดิน (0–4095)
  int span = SOIL_DRY_ADC - SOIL_WET_ADC;                                  // ช่วงคาลิเบรต: ค่าดินแห้ง - ค่าดินเปียก
  if (span <= 0) span = 1;                                                 // ป้องกันการหารด้วยศูนย์ (กรณีคาลิเบรตผิด)
  int soilPct = (int)(((float)(SOIL_DRY_ADC - soilRaw) * 100.0f) / span);  // แปลงค่าดิบเป็นเปอร์เซ็นต์ความชื้นดิน
  soilPct = constrain(soilPct, 0, 100);                                    // บีบค่าให้อยู่ในช่วง 0–100%

  Serial.print("T: ");
  Serial.print(t);
  Serial.print(" C | ");  // พิมพ์อุณหภูมิลง Serial
  Serial.print("H: ");
  Serial.print(h);
  Serial.print(" % | ");  // พิมพ์ความชื้นอากาศลง Serial
  Serial.print("Soil: ");
  Serial.print(soilPct);
  Serial.println(" %");  // พิมพ์เปอร์เซ็นต์ความชื้นดินลง Serial

  lcd.setCursor(0, 0);  // ย้ายเคอร์เซอร์ไปบรรทัดบนซ้ายสุด
  lcd.print("T:");      // แสดงป้ายอุณหภูมิ
  if (isnan(t)) lcd.print("--.-");
  else lcd.print(t, 1);  // ถ้าอ่านไม่ได้แสดง --.-; ถ้าได้ แสดงทศนิยม 1 ตำแหน่ง
  lcd.print((char)223);
  lcd.print("C ");  // แสดงสัญลักษณ์องศา (°) และตัว C พร้อมช่องว่าง
  lcd.print("H:");  // แสดงป้ายความชื้นอากาศ
  if (isnan(h)) lcd.print("--.-");
  else lcd.print(h, 0);  // ถ้าอ่านไม่ได้แสดง --.-; ถ้าได้ แสดงจำนวนเต็ม
  lcd.print("%");        // หน่วยเปอร์เซ็นต์

  lcd.setCursor(0, 1);  // ย้ายเคอร์เซอร์ไปบรรทัดล่างซ้ายสุด
  lcd.print("Soil:");   // แสดงป้ายความชื้นดิน
  lcd.print(soilPct);   // แสดงค่าเปอร์เซ็นต์ความชื้นดิน
  lcd.print("%    ");   // แสดงหน่วยและเติมช่องว่างเพื่อลบเศษตัวอักษรค้างบนจอ

  showNetStatus();  // แสดงสถานะเครือข่ายต่อท้ายบรรทัดล่าง (Onln/WiFi/Off)
}

void taskSendToBlynk() {                                                   // งานส่งข้อมูลขึ้น Blynk เป็นระยะ
  if (!Blynk.connected()) return;                                          // ถ้ายังไม่เชื่อมต่อ Blynk ให้ข้ามทันที
  float h = dht.readHumidity();                                            // อ่านความชื้นอากาศจาก DHT (%RH)
  float t = dht.readTemperature();                                         // อ่านอุณหภูมิอากาศจาก DHT (°C)
  int soilRaw = analogRead(SOIL_PIN);                                      // อ่านค่าดิบ ADC ของเซ็นเซอร์ความชื้นดิน
  int span = SOIL_DRY_ADC - SOIL_WET_ADC;                                  // ช่วงคาลิเบรต (ค่าดินแห้ง - ค่าดินเปียก)
  if (span <= 0) span = 1;                                                 // ป้องกันหารด้วยศูนย์กรณีคาลิเบรตผิด
  int soilPct = (int)(((float)(SOIL_DRY_ADC - soilRaw) * 100.0f) / span);  // แปลงค่าดิบเป็นเปอร์เซ็นต์ความชื้นดิน
  soilPct = constrain(soilPct, 0, 100);                                    // บีบค่าให้อยู่ในช่วง 0–100%

  if (!isnan(t)) Blynk.virtualWrite(V1, t);  // ส่งอุณหภูมิไป V1 หากอ่านได้
  if (!isnan(h)) Blynk.virtualWrite(V2, h);  // ส่งความชื้นอากาศไป V2 หากอ่านได้
  Blynk.virtualWrite(V3, soilPct);           // ส่งเปอร์เซ็นต์ความชื้นดินไป V3
}

void taskReconnect() {                                        // งานตรวจสถานะและพยายามเชื่อมต่อใหม่
  if (WiFi.status() != WL_CONNECTED) {                        // ถ้า Wi-Fi หลุด
    Serial.println("Wi-Fi disconnected -> reconnecting...");  // แจ้งเตือนทาง Serial
    WiFi.reconnect();                                         // <<< ใช้ค่านิ่งที่ WiFiManager บันทึกไว้ // ขอเชื่อม Wi-Fi ใหม่ด้วยคอนฟิกเดิม
    lcd.setCursor(0, 1);                                      // ย้ายเคอร์เซอร์ไปบรรทัดที่ 2
    lcd.print("WiFi: reconnect  ");                           // แสดงสถานะกำลังเชื่อม Wi-Fi ใหม่
    digitalWrite(STATUS_LED, LOW);                            // ดับไฟสถานะ (ยังไม่ออนไลน์ Blynk)
  }
  if (WiFi.status() == WL_CONNECTED && !Blynk.connected()) {  // ถ้า Wi-Fi ติดแล้วแต่ Blynk ยังไม่เชื่อม
    Serial.println("Blynk reconnecting...");                  // แจ้งกำลังเชื่อม Blynk ใหม่
    Blynk.connect(3000);                                      // พยายามเชื่อม Blynk โดยตั้ง timeout 3 วินาที
    if (!Blynk.connected()) {                                 // หากยังเชื่อมไม่สำเร็จ
      lcd.setCursor(0, 1);                                    // ย้ายเคอร์เซอร์ไปบรรทัดล่าง
      lcd.print("Blynk: offline ");                           // แสดงสถานะ Blynk ออฟไลน์
      digitalWrite(STATUS_LED, LOW);                          // ดับไฟสถานะ
    }
  }
}

/******************** Helper: show Wi-Fi/Blynk status ********************/
void showNetStatus() {                         // ฟังก์ชันแสดงสถานะเครือข่ายสั้น ๆ บนตำแหน่งท้ายบรรทัดล่างของ LCD
  lcd.setCursor(11, 1);                        // วางเคอร์เซอร์คอลัมน์ที่ 11 บรรทัดที่ 2 (พื้นที่ 4 ตัวอักษรสุดท้าย)
  if (WiFi.status() == WL_CONNECTED) {         // ถ้า Wi-Fi เชื่อมต่อแล้ว
    if (Blynk.connected()) lcd.print("Onln");  // ทั้ง Wi-Fi และ Blynk ติด → แสดง "Onln" (Online)
    else lcd.print("WiFi");                    // มี Wi-Fi แต่ Blynk หลุด → แสดง "WiFi"
  } else {
    lcd.print("Off ");  // ไม่มี Wi-Fi → แสดง "Off " (เติมช่องว่างเคลียร์ตัวเก่า)
  }
}

bool telegramSend(const String& text) {             // ฟังก์ชันส่งข้อความ text ไปยัง Telegram Bot API
  if (WiFi.status() != WL_CONNECTED) return false;  // หากยังไม่มี Wi-Fi ให้ยกเลิกและคืนค่า false

  WiFiClientSecure client;  // ใช้คลาส client แบบ HTTPS (SSL/TLS)
  client.setInsecure();     // ข้ามการตรวจสอบใบรับรอง (เหมาะกับอุปกรณ์ IoT / ลดความยุ่งยาก)
  HTTPClient https;         // ออบเจกต์ช่วยจัดการคำสั่ง HTTP/HTTPS

  String url = "https://api.telegram.org/bot"            // สร้าง URL ปลายทางของ Telegram Bot API
               + String(TG_BOT_TOKEN) + "/sendMessage";  // ต่อด้วยโทเค็นบอทและเมธอดส่งข้อความ

  String payload = "chat_id=" + String(TG_CHAT_ID)  // สร้างข้อมูลที่จะ POST: chat_id ของห้อง/ผู้รับ
                   + "&text=" + urlencode(text);    // ตามด้วยข้อความที่เข้ารหัสแบบ x-www-form-urlencoded

  https.begin(client, url);                                              // เริ่มเชื่อมต่อ HTTPS ไปยัง URL ที่กำหนด
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");  // ตั้งค่า Content-Type ให้สอดคล้องกับ payload
  int code = https.POST(payload);                                        // ส่งคำขอแบบ POST พร้อม payload
  bool ok = (code > 0 && code < 400);                                    // สำเร็จเมื่อได้รหัสสถานะ 2xx–3xx (หรือ >0 และ <400)
  https.end();                                                           // ปิด/คืนทรัพยากรการเชื่อมต่อ
  return ok;                                                             // คืนค่า true=ส่งสำเร็จ, false=ล้มเหลว
}

String urlencode(const String& s) {  // ฟังก์ชันเข้ารหัสสตริงเป็นรูปแบบ URL-encoded
  String out;
  char c;
  char bufHex[4];                            // out=ผลลัพธ์, c=ตัวอักษรที่กำลังประมวลผล, bufHex=บัฟเฟอร์ %XX
  for (size_t i = 0; i < s.length(); i++) {  // วนทุกตัวอักษรในสตริงต้นฉบับ
    c = s.charAt(i);                         // อ่านตัวอักษรตำแหน่ง i
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;  // อักขระที่ URL อนุญาต → ใส่ตรง ๆ
    } else if (c == ' ') {
      out += '+';  // ช่องว่าง → แทนด้วยเครื่องหมาย '+' (ตาม x-www-form-urlencoded)
    } else {
      snprintf(bufHex, sizeof(bufHex), "%%%02X", (unsigned char)c);  // อื่น ๆ → เข้ารหัสเป็น %XX (เลขฐาน 16 สองหลัก)
      out += bufHex;                                                 // ต่อผลลัพธ์ที่เข้ารหัสเข้ากับ out
    }
  }
  return out;  // คืนสตริงที่ถูกเข้ารหัสเรียบร้อย
}

void autoControlTask() {    // งานควบคุมอัตโนมัติ: เปรียบเทียบ Soil% กับเซ็ตพอยต์แล้วสั่งรีเลย์/แจ้งเตือน
  if (!g_autoMode) return;  // ถ้าไม่ได้อยู่ในโหมด Auto ให้จบฟังก์ชันทันที

  int soilRaw = analogRead(SOIL_PIN);                                      // อ่านค่าดิบจากเซ็นเซอร์ความชื้นดิน (0–4095)
  int span = SOIL_DRY_ADC - SOIL_WET_ADC;                                  // คำนวณช่วงคาลิเบรต: ดินแห้ง - ดินเปียก
  if (span <= 0) span = 1;                                                 // กันหารด้วยศูนย์กรณีคาลิเบรตผิด
  int soilPct = (int)(((float)(SOIL_DRY_ADC - soilRaw) * 100.0f) / span);  // แปลงค่าดิบเป็นเปอร์เซ็นต์ความชื้นดิน
  soilPct = constrain(soilPct, 0, 100);                                    // บีบค่าให้อยู่ในช่วง 0–100%

  unsigned long now = millis();  // เวลา ณ ปัจจุบัน (มิลลิวินาที นับจากบอร์ดเริ่มทำงาน)

  if (soilPct < g_soilSetpoint) {  // ถ้าความชื้นดินต่ำกว่าเซ็ตพอยต์
    setRelay1(true);               // สั่งเปิดวาล์วน้ำ (Relay1)
    sentAboveOnce = false;         // รีเซ็ตธงฝั่ง “มากกว่า/ปกติ” เพื่อพร้อมส่งใหม่เมื่อกลับขึ้นไป

    if (now - lastBelowMsgMs >= BELOW_INTERVAL_MS) {                                                                 // ถ้าห่างจากการแจ้งเตือนครั้งก่อนครบช่วงที่กำหนด
      String msg = "⚠️ ความชื้นดินต่ำกว่าค่าที่ตั้งไว้\n"  // สร้างข้อความแจ้งเตือน (กรณีต่ำกว่า)
                   "ค่าปัจจุบัน: "
                   + String(soilPct) + "%  <  ค่าที่ตั้งไว้: " + String(g_soilSetpoint) + "%\n"                            // แสดงค่าเปรียบเทียบ
                                                                                    "การทำงาน: เปิดวาล์วน้ำ (Relay1)";  // ระบุการกระทำที่ระบบทำ
      telegramSend(msg);                                                                                             // ส่งข้อความไป Telegram
      lastBelowMsgMs = now;                                                                                          // อัปเดตเวลาแจ้งเตือนครั้งล่าสุด
    }
  } else {                                    // กรณีความชื้นดิน ≥ เซ็ตพอยต์
    setRelay1(false);                         // ปิดวาล์วน้ำ (Relay1)
    if (!sentAboveOnce) {                     // ส่งแจ้งเตือนฝั่ง “ปกติ/มากกว่า” เพียงครั้งเดียวต่อช่วง
      String msg = "✅ ความชื้นดินอยู่ในระดับปกติ\n"  // สร้างข้อความแจ้งเตือน (กรณีปกติ/มากกว่า)
                   "ค่าปัจจุบัน: "
                   + String(soilPct) + "%  ≥  ค่าที่ตั้งไว้: " + String(g_soilSetpoint) + "%\n"                           // แสดงค่าเปรียบเทียบ
                                                                                    "การทำงาน: ปิดวาล์วน้ำ (Relay1)";  // ระบุการกระทำที่ระบบทำ
      telegramSend(msg);                                                                                            // ส่งข้อความไป Telegram
      sentAboveOnce = true;                                                                                         // ตั้งธงว่าแจ้งเตือนไปแล้ว
    }
    lastBelowMsgMs = 0;  // รีเซ็ตตัวนับเวลาแจ้งเตือนกรณีต่ำ เพื่อเริ่มนับใหม่เมื่อค่าตกลงต่ำ
  }
}

/******************** Main Loop ********************/
void loop() {
  if (Blynk.connected()) {  // ถ้าเชื่อมต่อกับ Blynk อยู่
    Blynk.run();            // ประมวลผลงานภายในของ Blynk (คอลแบ็ก/ซิงก์/ไทม์เมอร์)
  }
  timer.run();  // รันตารางงานตามเวลาที่ตั้งไว้ทั้งหมด (non-blocking)
}

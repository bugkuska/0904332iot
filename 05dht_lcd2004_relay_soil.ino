#include <Wire.h>
#include <LiquidCrystal_I2C.h>  // https://github.com/bugkuska/esp32/raw/main/basic/lcd/LiquidCrystal_i2c.zip
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ---------- Pins ----------
#define SOIL_PIN 35     // ขาอนาล็อกของ ESP32
#define relay1 4
#define relay2 5
#define relay3 18
#define relay4 19

// ---------- DHT ----------
#include <DHT.h>        // https://github.com/adafruit/DHT-sensor-library
#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ---------- Soil calibration (ปรับจริงภายหลัง) ----------
const int SOIL_DRY_ADC = 3000;   // ค่าที่อ่านได้ตอนดินแห้ง
const int SOIL_WET_ADC = 1200;   // ค่าที่อ่านได้ตอนดินเปียก

void setup() {
  Serial.begin(9600);
  dht.begin();

  pinMode(SOIL_PIN, INPUT);

  pinMode(relay1, OUTPUT);
  pinMode(relay2, OUTPUT);
  pinMode(relay3, OUTPUT);
  pinMode(relay4, OUTPUT);

  digitalWrite(relay1, LOW);
  digitalWrite(relay2, LOW);
  digitalWrite(relay3, LOW);
  digitalWrite(relay4, LOW);

  // ---------- LCD: init ครั้งเดียว ----------
  lcd.begin();        // ไลบรารีของคุณใช้ begin() ไม่รับพารามิเตอร์
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Booting...");
  delay(500);
}

void loop() {
  // ----- อ่าน DHT -----
  float h = dht.readHumidity();
  float t = dht.readTemperature();   // C

  // ----- อ่าน Soil Moisture (ADC) -----
  int soilRaw = analogRead(SOIL_PIN);

  // แปลงเป็น % ความชื้นดิน (คาลิเบรตได้แม่นกว่า)
  int span = SOIL_DRY_ADC - SOIL_WET_ADC;
  if (span <= 0) span = 1;
  int soilPct = (int)(((float)(SOIL_DRY_ADC - soilRaw) * 100.0f) / (float)span);
  soilPct = constrain(soilPct, 0, 100);

  // ----- Serial Monitor -----
  Serial.print("Temperature: ");
  Serial.print(t); Serial.print(" C | Humidity: ");
  Serial.print(h); Serial.print(" % | Soil RAW: ");
  Serial.print(soilRaw); Serial.print(" | Soil: ");
  Serial.print(soilPct); Serial.println(" %");
  delay(200);

  // ----- LCD (อย่าเรียก lcd.begin ใน loop) -----
  lcd.setCursor(0, 0);
  lcd.print("TEMP = ");
  if (isnan(t)) lcd.print("--.-");
  else          lcd.print(t, 1);
  lcd.print((char)223); lcd.print("C   ");  // เติมช่องว่างลบเลขค้าง

  lcd.setCursor(0, 1);
  lcd.print("HUMI = ");
  if (isnan(h)) lcd.print("--.-");
  else          lcd.print(h, 1);
  lcd.print("%    ");

  lcd.setCursor(0, 2);
  lcd.print("SOIL = ");
  lcd.print(soilPct);
  lcd.print("%    ");

  // ----- ตัวอย่างทดสอบรีเลย์แบบเดิม -----
  digitalWrite(relay1, LOW);  delay(500);
  digitalWrite(relay1, HIGH); delay(500);

  digitalWrite(relay2, LOW);  delay(500);
  digitalWrite(relay2, HIGH); delay(500);

  digitalWrite(relay3, LOW);  delay(500);
  digitalWrite(relay3, HIGH); delay(500);

  digitalWrite(relay4, LOW);  delay(500);
  digitalWrite(relay4, HIGH); delay(500);

  digitalWrite(relay1, LOW);
  digitalWrite(relay2, LOW);
  digitalWrite(relay3, LOW);
  digitalWrite(relay4, LOW);
  delay(500);

  digitalWrite(relay1, HIGH);
  digitalWrite(relay2, HIGH);
  digitalWrite(relay3, HIGH);
  digitalWrite(relay4, HIGH);
  delay(500);
}

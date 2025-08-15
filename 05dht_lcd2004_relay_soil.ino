#include <Wire.h>                     // นำเข้าไลบรารี Wire สำหรับสื่อสารแบบ I2C (ESP32 มีฮาร์ดแวร์ I2C ในตัว)
#include <LiquidCrystal_I2C.h>        // นำเข้าไลบรารีควบคุมจอ LCD ผ่าน I2C เพื่อลดจำนวนสายสัญญาณเหลือ SDA/SCL
//Download ได้จาก https://drive.google.com/file/d/1hkmODw9q8CdxQx99Bp1Eu2foACQ-C4Sx/view?usp=sharing
LiquidCrystal_I2C lcd(0x27, 20, 4);   // สร้างออบเจกต์ LCD ที่แอดเดรส I2C = 0x27 ขนาด 20 คอลัมน์ 4 แถว
                                      // หมายเหตุ: แอดเดรสอาจต่างกัน (0x3F/0x27) แล้วแต่โมดูล I2C backpack; ถ้าไม่ขึ้น ลองสแกน I2C ก่อน
// ---------- Pins ----------
#define SOIL_PIN 35     // เลือก GPIO35 เป็นขาอ่านอนาล็อกสำหรับเซ็นเซอร์ความชื้นดิน (ADC1 channel; GPIO34–39 เป็น input-only)
#define relay1 4        // กำหนดชื่อสัญลักษณ์ให้ GPIO4 เป็นรีเลย์ 1 (อ่านง่ายขึ้นและแก้ไขสะดวกภายหลัง)
#define relay2 5        // กำหนด GPIO5 เป็นรีเลย์ 2
#define relay3 18       // กำหนด GPIO18 เป็นรีเลย์ 3
#define relay4 19       // กำหนด GPIO19 เป็นรีเลย์ 4

// ---------- DHT ----------
#include <DHT.h>        // ไลบรารีเซ็นเซอร์ DHT ของ Adafruit ใช้สำหรับอ่านอุณหภูมิ/ความชื้นอากาศ
#define DHTPIN 15       // ต่อขา DATA ของ DHT11 เข้ากับ GPIO15 (ควรมีตัวต้านทาน pull-up 10k ไปที่ VCC ตามคู่มือ)
#define DHTTYPE DHT11   // ระบุชนิดเซ็นเซอร์เป็น DHT11 (ถ้าเปลี่ยนเป็น DHT22 ให้แก้ตรงนี้)
DHT dht(DHTPIN, DHTTYPE); // สร้างออบเจกต์ dht ผูกกับพินและชนิด เพื่อเรียกใช้ .readTemperature() / .readHumidity()

// ---------- Soil calibration (ปรับจริงภายหลัง) ----------
const int SOIL_DRY_ADC = 3000;   // ค่าดิบ ADC ประมาณเมื่อ "ดินแห้งมาก" (ต้องคาลิเบรตจากหน้างานจริงเพื่อความแม่นยำ)
const int SOIL_WET_ADC = 1200;   // ค่าดิบ ADC ประมาณเมื่อ "ดินเปียกมาก" (จุ่มน้ำ/ดินแฉะ) — ยิ่งเปียกยิ่งอ่านได้น้อยในหลายโมดูล

void setup() {
  Serial.begin(9600);            // เปิด Serial ที่ 9600bps เพื่อดีบัก/ดูค่าเซ็นเซอร์ใน Serial Monitor
  dht.begin();                   // เริ่มต้นเซ็นเซอร์ DHT (ต้องทำใน setup เสมอเพื่อเตรียมการอ่าน)

  pinMode(SOIL_PIN, INPUT);      // ตั้งขาอ่านความชื้นดินเป็นโหมด INPUT (ESP32 ADC อ่านค่า 0–4095 เป็น 12-bit โดยดีฟอลต์)

  pinMode(relay1, OUTPUT);       // ตั้งขารีเลย์ทั้งหมดเป็น OUTPUT เพื่อให้สั่ง HIGH/LOW ได้
  pinMode(relay2, OUTPUT);
  pinMode(relay3, OUTPUT);
  pinMode(relay4, OUTPUT);

  digitalWrite(relay1, LOW);     // กำหนดสถานะเริ่มต้นของรีเลย์เป็น LOW (ขึ้นอยู่กับโมดูลรีเลย์ว่า Active-LOW หรือ Active-HIGH)
  digitalWrite(relay2, LOW);     // ถ้าโมดูลคุณเป็น Active-LOW: LOW = ON, HIGH = OFF — ให้เปลี่ยนตามความจริงของบอร์ด
  digitalWrite(relay3, LOW);     // แนะนำทดสอบทีละช่องเพื่อยืนยันพฤติกรรมก่อนใช้งานจริงกับโหลด
  digitalWrite(relay4, LOW);

  // ---------- LCD: init ครั้งเดียว ----------
  lcd.begin();                   // สำหรับไลบรารีเวอร์ชันนี้: เริ่มต้นจอด้วย begin() ที่ "ไม่รับพารามิเตอร์"
                                 // เวอร์ชันอื่นอาจใช้ lcd.init() แทน; ห้ามเรียก begin()/init ใน loop เพราะจะทำให้จอกะพริบ/ช้า
  lcd.backlight();               // เปิดไฟพื้นหลังจอ LCD (บางโมดูลต้องสั่ง open backlight แยกจาก begin())
  lcd.clear();                   // ล้างหน้าจอเคลียร์ทุกแถว
  lcd.setCursor(0, 0);           // วางเคอร์เซอร์ที่คอลัมน์ 0 แถว 0 (แถวแรก)
  lcd.print("System Booting...");// พิมพ์ข้อความบูตเพื่อระบุว่าเครื่องเริ่มทำงานแล้ว
  delay(500);                    // หน่วงสั้น ๆ เพื่อให้ผู้ใช้เห็นข้อความเริ่มต้น (หลีกเลี่ยงการหน่วงยาวเกินจำเป็น)
}

void loop() {
  // ----- อ่าน DHT -----
  float h = dht.readHumidity();        // อ่านความชื้นสัมพัทธ์ของอากาศ (%RH); ควรอ่านไม่ถี่มาก (DHT11 ~1–2s/ครั้ง)
  float t = dht.readTemperature();     // อ่านอุณหภูมิหน่วย °C (ถ้าอยากได้ °F ใช้ readTemperature(true))

  // ----- อ่าน Soil Moisture (ADC) -----
  int soilRaw = analogRead(SOIL_PIN);  // อ่านค่า ADC 12-bit ช่วง 0–4095; ยิ่งแรงดันเข้า ADC มากค่ายิ่งสูง
                                       // หมายเหตุ: แรงดันอินพุต ADC ESP32 สูงสุด ~3.3V (ห้ามเกิน); บางโมดูลดินให้สัญญาณกลับด้าน

  // แปลงเป็น % ความชื้นดิน (คำนวณแบบเส้นตรงจากค่าคาลิเบรต)
  int span = SOIL_DRY_ADC - SOIL_WET_ADC;    // ช่วงระหว่างค่าดินแห้งกับดินเปียก (ค่าบน-ค่าล่าง)
  if (span <= 0) span = 1;                   // ป้องกันการหารด้วยศูนย์/ค่าผิดพลาดเมื่อคาลิเบรตสลับกัน
  int soilPct = (int)(((float)(SOIL_DRY_ADC - soilRaw) * 100.0f) / (float)span);
                                             // สูตร: แปลงค่า soilRaw ให้เป็นสัดส่วน 0–100%
                                             // - เมื่อ soilRaw ใกล้ SOIL_DRY_ADC => ค่า % ใกล้ 0 (แห้ง)
                                             // - เมื่อ soilRaw ใกล้ SOIL_WET_ADC => ค่า % ใกล้ 100 (เปียก)
  soilPct = constrain(soilPct, 0, 100);      // บังคับค่าอยู่ในช่วง 0–100% ป้องกันการล้นจากสัญญาณรบกวน

  // ----- Serial Monitor (เพื่อดีบัก/ตรวจสอบ) -----
  Serial.print("Temperature: ");       // พิมพ์หัวข้ออุณหภูมิ
  Serial.print(t); Serial.print(" C | Humidity: ");  // แสดงอุณหภูมิพร้อมหน่วย °C ต่อด้วยข้อความคั่น
  Serial.print(h); Serial.print(" % | Soil RAW: ");  // แสดงความชื้นอากาศต่อด้วยค่า ADC ดิน (ดิบ)
  Serial.print(soilRaw); Serial.print(" | Soil: ");  // แสดงค่า ADC แล้วคั่นด้วยข้อความ "Soil:"
  Serial.print(soilPct); Serial.println(" %");       // แสดง % ความชื้นดิน แล้วขึ้นบรรทัดใหม่
  delay(200);                             // พักสั้น ๆ เพื่อลดภาระซีเรียล/การกวาดจอ (ยังถือว่าอ่าน DHT ค่อนข้างถี่อยู่)

  // ----- LCD (อย่าเรียก lcd.begin ใน loop) -----
  lcd.setCursor(0, 0);                   // วางเคอร์เซอร์บรรทัดที่ 1
  lcd.print("TEMP = ");                  // พิมพ์ป้ายกำกับอุณหภูมิ
  if (isnan(t)) lcd.print("--.-");       // ถ้าอ่านอุณหภูมิไม่สำเร็จ (NaN) ให้ขึ้นเครื่องหมายบอกข้อผิดพลาด
  else          lcd.print(t, 1);         // ถ้าอ่านสำเร็จ แสดงทศนิยม 1 ตำแหน่ง (อ่านง่าย ไม่แกว่งมาก)
  lcd.print((char)223); lcd.print("C   "); // พิมพ์สัญลักษณ์องศา (ASCII 223 ในจอ HD44780) ต่อด้วยตัว C และช่องว่างลบเศษตัวอักษรเดิม

  lcd.setCursor(0, 1);                   // วางเคอร์เซอร์บรรทัดที่ 2
  lcd.print("HUMI = ");                  // ป้ายกำกับความชื้นอากาศ
  if (isnan(h)) lcd.print("--.-");       // ถ้าอ่านความชื้นไม่สำเร็จ แสดง --.-
  else          lcd.print(h, 1);         // ถ้าอ่านได้ แสดงทศนิยม 1 ตำแหน่ง
  lcd.print("%    ");                    // แสดงสัญลักษณ์ % และช่องว่างเพื่อเคลียร์เศษตัวอักษรค้าง

  lcd.setCursor(0, 2);                   // วางเคอร์เซอร์บรรทัดที่ 3
  lcd.print("SOIL = ");                  // ป้ายกำกับความชื้นดิน
  lcd.print(soilPct);                    // แสดงค่า % ความชื้นดิน (จำนวนเต็ม)
  lcd.print("%    ");                    // เติมช่องว่างท้ายบรรทัดเพื่อเคลียร์เศษตัวอักษรจากรอบก่อน

  // ----- ตัวอย่างทดสอบรีเลย์แบบเดิม (Demonstration only) -----
  digitalWrite(relay1, LOW);  delay(500); // สั่งรีเลย์ 1 เป็น LOW แล้วหน่วง 0.5s (ถ้า Active-LOW จะเท่ากับ "เปิด")
  digitalWrite(relay1, HIGH); delay(500); // สั่งรีเลย์ 1 เป็น HIGH แล้วหน่วง 0.5s (ถ้า Active-LOW จะเท่ากับ "ปิด")

  digitalWrite(relay2, LOW);  delay(500); // ทำเช่นเดียวกันกับรีเลย์ 2
  digitalWrite(relay2, HIGH); delay(500);

  digitalWrite(relay3, LOW);  delay(500); // ทำเช่นเดียวกันกับรีเลย์ 3
  digitalWrite(relay3, HIGH); delay(500);

  digitalWrite(relay4, LOW);  delay(500); // ทำเช่นเดียวกันกับรีเลย์ 4
  digitalWrite(relay4, HIGH); delay(500);

  digitalWrite(relay1, LOW);              // ปิดทั้งหมด (หรือเปิด ถ้าโมดูลเป็น Active-LOW)
  digitalWrite(relay2, LOW);
  digitalWrite(relay3, LOW);
  digitalWrite(relay4, LOW);
  delay(500);                             // เว้นระยะ 0.5s เพื่อให้มองเห็นการเปลี่ยนสถานะ

  digitalWrite(relay1, HIGH);             // เปิดทั้งหมด (หรือปิด ถ้า Active-LOW) เพื่อทดสอบพร้อมกัน
  digitalWrite(relay2, HIGH);
  digitalWrite(relay3, HIGH);
  digitalWrite(relay4, HIGH);
  delay(500);                             // เว้นระยะ 0.5s ปิดท้ายลูป
}

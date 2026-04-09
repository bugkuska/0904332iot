/************************************************************
 * ESP32 + Modbus Relay 12CH + Blynk Legacy
 * TTL to RS485 (no DE/RE)
 * Read-back sync using actual relay states from board
 *
 * Read DO status  : Function 01
 * Write single DO : Function 05
 * Write all 12 DO : Function 15 (0x0F)
 ************************************************************/

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <ModbusMaster.h>

// =========================
// USER CONFIG
// =========================
char auth[] = ""; //แก้ Token
char ssid[] = "Com@Science";
char pass[] = "ComS2568";

// ถ้าใช้ Blynk Legacy Local Server ให้แก้เป็น IP/Domain ของคุณ
char blynkServer[] = "192.168.0.253";
uint16_t blynkPort = 8080;

// =========================
// RS485 UART
// =========================
#define TXD 17
#define RXD 18

// =========================
// Modbus settings
// =========================
#define MODBUS_BAUDRATE 9600
#define MODBUS_SLAVE_ID 1

ModbusMaster node1;
BlynkTimer timer;

// =========================
// Virtual pins
// =========================
#define VPIN_SW1   V1
#define VPIN_SW2   V2
#define VPIN_SW3   V3
#define VPIN_SW4   V4
#define VPIN_SW5   V5
#define VPIN_SW6   V6
#define VPIN_SW7   V7
#define VPIN_SW8   V8
#define VPIN_SW9   V9
#define VPIN_SW10  V10
#define VPIN_SW11  V11
#define VPIN_SW12  V12
#define VPIN_ALL   V13

// =========================
// Relay addresses
// 12 channels: 0x0000 - 0x000B
// =========================
const uint16_t relayCoils[12] = {
  0x0000, 0x0001, 0x0002, 0x0003,
  0x0004, 0x0005, 0x0006, 0x0007,
  0x0008, 0x0009, 0x000A, 0x000B
};

// actual state cache from device
bool relayState[12] = {0};

// ป้องกัน loop ตอน virtualWrite กลับเข้า BLYNK_WRITE
bool isSyncingUI = false;

// =========================
// Forward declarations
// =========================
bool writeSingleRelay(uint8_t relayIndex, bool state);
bool writeAllRelays(bool state);
bool readAllRelayStatesFromBoard();
void syncAllToBlynk();
void checkConnections();
void pollRelayStates();
void printRelayStates();

// =========================
// Sync current cache to Blynk
// =========================
void syncAllToBlynk() {
  if (!Blynk.connected()) return;

  isSyncingUI = true;

  Blynk.virtualWrite(VPIN_SW1,  relayState[0]);
  Blynk.virtualWrite(VPIN_SW2,  relayState[1]);
  Blynk.virtualWrite(VPIN_SW3,  relayState[2]);
  Blynk.virtualWrite(VPIN_SW4,  relayState[3]);
  Blynk.virtualWrite(VPIN_SW5,  relayState[4]);
  Blynk.virtualWrite(VPIN_SW6,  relayState[5]);
  Blynk.virtualWrite(VPIN_SW7,  relayState[6]);
  Blynk.virtualWrite(VPIN_SW8,  relayState[7]);
  Blynk.virtualWrite(VPIN_SW9,  relayState[8]);
  Blynk.virtualWrite(VPIN_SW10, relayState[9]);
  Blynk.virtualWrite(VPIN_SW11, relayState[10]);
  Blynk.virtualWrite(VPIN_SW12, relayState[11]);

  bool allOn = true;
  for (int i = 0; i < 12; i++) {
    if (!relayState[i]) {
      allOn = false;
      break;
    }
  }
  Blynk.virtualWrite(VPIN_ALL, allOn ? 1 : 0);

  isSyncingUI = false;
}

// =========================
// Write single relay using FC05
// =========================
bool writeSingleRelay(uint8_t relayIndex, bool state) {
  if (relayIndex >= 12) return false;

  uint8_t result;
  uint16_t coilAddr = relayCoils[relayIndex];

  if (state) {
    result = node1.writeSingleCoil(coilAddr, 0x00FF); // ON
  } else {
    result = node1.writeSingleCoil(coilAddr, 0x0000); // OFF
  }

  if (result == node1.ku8MBSuccess) {
    Serial.print("Write Relay ");
    Serial.print(relayIndex + 1);
    Serial.print(" -> ");
    Serial.println(state ? "ON" : "OFF");

    // อ่านกลับทันทีหลังเขียน
    readAllRelayStatesFromBoard();
    syncAllToBlynk();
    return true;
  } else {
    Serial.print("Write single failed, relay ");
    Serial.print(relayIndex + 1);
    Serial.print(", err=");
    Serial.println(result);
    return false;
  }
}

// =========================
// Write all relays using FC15 (0x0F)
// เวอร์ชันนี้แก้ให้เข้ากับ ModbusMaster ของคุณ
// =========================
bool writeAllRelays(bool state) {
  uint8_t result;

  // ล้าง buffer ก่อน
  node1.clearTransmitBuffer();

  if (state) {
    // 12 ช่อง ON
    // low word = 0x00FF (ch0-ch7)
    // next word = 0x000F (ch8-ch11)
    node1.setTransmitBuffer(0, 0x00FF);
    node1.setTransmitBuffer(1, 0x000F);
  } else {
    // 12 ช่อง OFF
    node1.setTransmitBuffer(0, 0x0000);
    node1.setTransmitBuffer(1, 0x0000);
  }

  result = node1.writeMultipleCoils(0x0000, 12);

  if (result == node1.ku8MBSuccess) {
    Serial.println(state ? "ALL RELAYS -> ON" : "ALL RELAYS -> OFF");

    // อ่านกลับทันทีหลังเขียน
    readAllRelayStatesFromBoard();
    syncAllToBlynk();
    return true;
  } else {
    Serial.print("Write all failed, err=");
    Serial.println(result);
    return false;
  }
}

// =========================
// Read all 12 relay states from board using FC01
// =========================
bool readAllRelayStatesFromBoard() {
  uint8_t result = node1.readCoils(0x0000, 12);

  if (result != node1.ku8MBSuccess) {
    Serial.print("Read coils failed, err=");
    Serial.println(result);
    return false;
  }

  uint16_t packed = node1.getResponseBuffer(0);

  for (int i = 0; i < 12; i++) {
    relayState[i] = (packed >> i) & 0x01;
  }

  return true;
}

// =========================
// Poll actual board states
// =========================
void pollRelayStates() {
  static bool firstRead = true;
  bool oldState[12];

  for (int i = 0; i < 12; i++) {
    oldState[i] = relayState[i];
  }

  if (!readAllRelayStatesFromBoard()) {
    return;
  }

  bool changed = firstRead;
  for (int i = 0; i < 12; i++) {
    if (relayState[i] != oldState[i]) {
      changed = true;
      Serial.print("Detected change: Relay ");
      Serial.print(i + 1);
      Serial.print(" = ");
      Serial.println(relayState[i] ? "ON" : "OFF");
    }
  }

  if (changed) {
    printRelayStates();
    syncAllToBlynk();
    firstRead = false;
  }
}

// =========================
// Serial print status
// =========================
void printRelayStates() {
  Serial.print("Relay states: ");
  for (int i = 0; i < 12; i++) {
    Serial.print(relayState[i] ? "1" : "0");
    if (i < 11) Serial.print(" ");
  }
  Serial.println();
}

// =========================
// Connectivity check
// =========================
void checkConnections() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    WiFi.begin(ssid, pass);

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
      delay(300);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi reconnected");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
    }
  }

  if (WiFi.status() == WL_CONNECTED && !Blynk.connected()) {
    Serial.println("Blynk disconnected, reconnecting...");
    Blynk.connect(5000);
  }
}

// =========================
// Blynk connected
// =========================
BLYNK_CONNECTED() {
  Serial.println("Blynk connected");

  if (readAllRelayStatesFromBoard()) {
    printRelayStates();
    syncAllToBlynk();
  }
}

// =========================
// Blynk handlers
// =========================
BLYNK_WRITE(VPIN_SW1)  { if (!isSyncingUI) writeSingleRelay(0,  param.asInt()); }
BLYNK_WRITE(VPIN_SW2)  { if (!isSyncingUI) writeSingleRelay(1,  param.asInt()); }
BLYNK_WRITE(VPIN_SW3)  { if (!isSyncingUI) writeSingleRelay(2,  param.asInt()); }
BLYNK_WRITE(VPIN_SW4)  { if (!isSyncingUI) writeSingleRelay(3,  param.asInt()); }
BLYNK_WRITE(VPIN_SW5)  { if (!isSyncingUI) writeSingleRelay(4,  param.asInt()); }
BLYNK_WRITE(VPIN_SW6)  { if (!isSyncingUI) writeSingleRelay(5,  param.asInt()); }
BLYNK_WRITE(VPIN_SW7)  { if (!isSyncingUI) writeSingleRelay(6,  param.asInt()); }
BLYNK_WRITE(VPIN_SW8)  { if (!isSyncingUI) writeSingleRelay(7,  param.asInt()); }
BLYNK_WRITE(VPIN_SW9)  { if (!isSyncingUI) writeSingleRelay(8,  param.asInt()); }
BLYNK_WRITE(VPIN_SW10) { if (!isSyncingUI) writeSingleRelay(9,  param.asInt()); }
BLYNK_WRITE(VPIN_SW11) { if (!isSyncingUI) writeSingleRelay(10, param.asInt()); }
BLYNK_WRITE(VPIN_SW12) { if (!isSyncingUI) writeSingleRelay(11, param.asInt()); }

BLYNK_WRITE(VPIN_ALL) {
  if (isSyncingUI) return;
  writeAllRelays(param.asInt() == 1);
}

// =========================
// Setup
// =========================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Starting ESP32 Modbus Relay 12CH REAL SYNC (No WiFiManager)");

  Serial2.begin(MODBUS_BAUDRATE, SERIAL_8N1, RXD, TXD);
  Serial2.setTimeout(200);
  node1.begin(MODBUS_SLAVE_ID, Serial2);

  Serial.println("Connecting WiFi...");
  WiFi.begin(ssid, pass);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connect failed");
  }

  Blynk.config(auth, blynkServer, blynkPort);
  if (Blynk.connect(10000)) {
    Serial.println("Blynk connected successfully");
  } else {
    Serial.println("Initial Blynk connect failed");
  }

  timer.setInterval(300L, pollRelayStates);
  timer.setInterval(10000L, checkConnections);

  if (readAllRelayStatesFromBoard()) {
    printRelayStates();
  }
}

// =========================
// Loop
// =========================
void loop() {
  Blynk.run();
  timer.run();
}

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <DHT.h>

// Wi-Fi and Blynk credentials
const char ssid[] = "smf001";
const char pass[] = "0814111142";
const char auth[] = "OUVoNemiCMONaENUF4Ze-oF55sBH2YHy";

// InfluxDB Configuration
#define INFLUXDB_URL "http://192.168.1.168:8086"
#define INFLUXDB_TOKEN "B51vRj_PQ5fYFcRL7NhTXzPySM5vQW82cLerov1i8aZN9XkZqpeRSDwsTNna6teHlBvr4mLpNX0NKqYa_iGUMw=="
#define INFLUXDB_ORG "bb1cc76268fd69ea"
#define INFLUXDB_BUCKET "smart01"
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point sensor("environment");

// GPIO Configuration
#define LED_PIN 2
#define DHTPIN 15
#define DHTTYPE DHT11
#define RELAY1_PIN 26
#define RELAY2_PIN 25
#define RELAY3_PIN 33
#define RELAY4_PIN 32

// DHT and LCD Initialization
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Blynk Timer
BlynkTimer timer;

// Relay states
int relay1State = LOW;
int relay2State = LOW;
int relay3State = LOW;
int relay4State = LOW;

// Last reconnect attempt time
unsigned long lastReconnectAttempt = 0;

// Function Prototypes
void readDHTSensor();
void reconnectWiFi();
void reconnectBlynk();

void setup() {
  Serial.begin(9600);
  dht.begin();
  lcd.begin();
  lcd.backlight();
  lcd.clear();

  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);

  // Initial Wi-Fi connection
  reconnectWiFi();

  // Configure Blynk (non-blocking)
  Blynk.config(auth, "192.168.1.168", 8080);

  // Check InfluxDB connection
  if (!client.validateConnection()) {
    Serial.println("InfluxDB connection failed!");
  }

  timer.setInterval(10000L, readDHTSensor);  // Read DHT every 10 seconds
  timer.setInterval(5000L, reconnectWiFi);   // Check Wi-Fi every 5 seconds
  timer.setInterval(10000L, reconnectBlynk); // Attempt to reconnect Blynk every 10 seconds
}

// Sync all buttons when Blynk is connected
BLYNK_CONNECTED() {
  Serial.println("Blynk connected!");
  Blynk.syncAll(); // Sync all Virtual Pins

  // Sync relay states (optional for better clarity)
  Blynk.syncVirtual(V10);
  Blynk.syncVirtual(V11);
  Blynk.syncVirtual(V12);
  Blynk.syncVirtual(V13);
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

void readDHTSensor() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (!isnan(temperature) && !isnan(humidity)) {
    Serial.print("Temp: ");
    Serial.print(temperature);
    Serial.print("C, Humidity: ");
    Serial.println(humidity);

    // Send to Blynk if connected
    if (Blynk.connected()) {
      Blynk.virtualWrite(V1, temperature);
      Blynk.virtualWrite(V2, humidity);
    }

    // Display on LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temp: " + String(temperature) + "C");
    lcd.setCursor(0, 1);
    lcd.print("Humidity: " + String(humidity) + "%");

    // Write to InfluxDB
    sensor.clearFields();
    sensor.addField("temperature", temperature);
    sensor.addField("humidity", humidity);
    if (!client.writePoint(sensor)) {
      Serial.println("Failed to write to InfluxDB");
    }
  }
}

void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected! Reconnecting...");
    WiFi.begin(ssid, pass);
  }
}

void reconnectBlynk() {
  unsigned long now = millis();
  if (!Blynk.connected() && (now - lastReconnectAttempt > 10000)) { // Attempt reconnect every 10 seconds
    Serial.println("Blynk disconnected! Attempting to reconnect...");
    if (Blynk.connect()) {
      Serial.println("Blynk reconnected!");
    } else {
      Serial.println("Blynk reconnect failed.");
    }
    lastReconnectAttempt = now;
  }
}

void loop() {
  if (Blynk.connected()) {
    Blynk.run();
  }
  timer.run();
}

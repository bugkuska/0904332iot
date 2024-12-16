/*
Libraries
1. Blynk : https://downloads.arduino.cc/libraries/github.com/blynkkk/Blynk-1.0.1.zip
2. DHT : Install from Arduino Library Manager
*/

#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>

// Wi-Fi and Blynk credentials
const char ssid[] = "Your_SSID";        // Your Wi-Fi SSID
const char pass[] = "Your_PASSWORD";   // Your Wi-Fi password
const char auth[] = "Your_AUTH_TOKEN"; // Auth token from Blynk app

// GPIO configuration
#define LED_PIN 2
#define DHTPIN 15          // GPIO15 connected to DHT sensor
#define DHTTYPE DHT22      // DHT type: DHT11 or DHT22

// Create DHT instance
DHT dht(DHTPIN, DHTTYPE);

// Timer for periodic tasks
BlynkTimer timer;

// Function prototypes
void checkConnections();
void readDHTSensor();

void setup() {
  // Initialize serial communication
  Serial.begin(9600);

  // Configure LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Initialize DHT sensor
  dht.begin();

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  // Wait for Wi-Fi connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Enable auto-reconnect for Wi-Fi
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // Connect to Blynk server
  Serial.println("Connecting to Blynk server...");
  Blynk.begin(auth, ssid, pass, "ip-address-blynk-localserver", 8080);

  // Set up periodic tasks
  timer.setInterval(2000L, readDHTSensor);   // Read DHT every 2 seconds
  timer.setInterval(5000L, checkConnections); // Check connections every 5 seconds
}

// Callback when Blynk is connected
BLYNK_CONNECTED() {
  Serial.println("Blynk connected!");
  Blynk.syncAll(); // Synchronize all Blynk states
}

// Callback to control LED from Blynk app
BLYNK_WRITE(V0) {
  int ledState = param.asInt(); // Get value from Blynk app (0 or 1)
  digitalWrite(LED_PIN, ledState); // Set LED state
  Serial.print("LED state set to: ");
  Serial.println(ledState ? "ON" : "OFF");
}

// Function to read DHT sensor
void readDHTSensor() {
  float temperature = dht.readTemperature(); // Read temperature in Celsius
  float humidity = dht.readHumidity();       // Read humidity

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Print to Serial Monitor
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print("°C, Humidity: ");
  Serial.print(humidity);
  Serial.println("%");

  // Send to Blynk app (Virtual Pins V1 and V2)
  Blynk.virtualWrite(V1, temperature);
  Blynk.virtualWrite(V2, humidity);
}

// Function to check Wi-Fi and Blynk connection
void checkConnections() {
  // Check Wi-Fi status
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected! Reconnecting...");
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\nWi-Fi reconnected");
  }

  // Check Blynk connection
  if (!Blynk.connected()) {
    Serial.println("Blynk disconnected! Reconnecting...");
    Blynk.connect();
  }
}

void loop() {
  // Run Blynk processes
  if (Blynk.connected()) {
    Blynk.run();
  }
  
  // Run timer tasks
  timer.run();
}

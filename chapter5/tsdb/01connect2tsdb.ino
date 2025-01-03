#if defined(ESP32)
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
  #define DEVICE "ESP32"
#elif defined(ESP8266)
  #include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
  #define DEVICE "ESP8266"
#endif
//version 3.13.2
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <DHT.h>

// DHT11 Configuration
#define DHTPIN 15     // Pin where the DHT11 is connected
#define DHTTYPE DHT11 // DHT11 type sensor
DHT dht(DHTPIN, DHTTYPE);

// WiFi AP SSID
#define WIFI_SSID "YOUR_WIFI_SSID" //แก้ชื่อ Wi-Fi
// WiFi password
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD" //แก้รหัสการเชื่อมต่อ Wi-Fi

#define INFLUXDB_URL "http://ip-address-influxdb:8086"
#define INFLUXDB_TOKEN "API Token"
#define INFLUXDB_ORG "Organization id"
#define INFLUXDB_BUCKET "ชื่อbucket"

// Time zone info
#define TZ_INFO "UTC7"

// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Declare Data point
Point sensor("environment");

void setup() {
  Serial.begin(115200);

  // Initialize DHT sensor
  dht.begin();

  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // Accurate time is necessary for certificate validation and writing in batches
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void loop() {
  // Read data from DHT11
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Check if readings are valid
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Print readings to Serial
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print("°C, Humidity: ");
  Serial.print(humidity);
  Serial.println("%");

  // Prepare data point
  sensor.clearFields();
  sensor.addField("temperature", temperature);
  sensor.addField("humidity", humidity);

  // Write data to InfluxDB
  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  // Wait before next reading
  delay(2000);
}

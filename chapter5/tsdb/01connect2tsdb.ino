#include <WiFi.h>
#include <InfluxDbClient.h>
#include <DHT.h>

// กำหนดพินเซ็นเซอร์และประเภท
#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ข้อมูล WiFi
const char* ssid = "Your_SSID"; //แก้ชื่อ Wi-Fi
const char* password = "Your_PASSWORD"; //ใส่รหัสการเชื่อมต่อ Wi-Fi (ถ้ามี)

// ข้อมูล InfluxDB
#define INFLUXDB_URL "http://<your_influxdb_server>:8086"
#define INFLUXDB_TOKEN "your_api_token"
#define INFLUXDB_ORG "your_organization"
#define INFLUXDB_BUCKET "iot_data"  //ชื่อฐานข้อมูล

// สร้าง InfluxDB Client
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
Point sensor("environment"); // Measurement ชื่อ environment

void setup() {
  Serial.begin(9600);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  dht.begin();

  if (client.validateConnection()) {
    Serial.println("Connected to InfluxDB!");
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void loop() {
  // อ่านค่าเซ็นเซอร์
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    delay(10000);
    return;
  }

  // เพิ่มข้อมูล Tags และ Fields
  sensor.addTag("device", "esp32");
  sensor.addTag("location", "living_room");
  sensor.addField("temperature", temperature);
  sensor.addField("humidity", humidity);

  // เขียนข้อมูลไปยัง InfluxDB
  if (client.writePoint(sensor)) {
    Serial.println("Data written successfully!");
  } else {
    Serial.print("Failed to write data: ");
    Serial.println(client.getLastErrorMessage());
  }

  delay(10000); // ส่งข้อมูลทุก 10 วินาที
}

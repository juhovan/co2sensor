#include "config.h"

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme; // I2C

#ifdef HTTPUpdateServer
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>

ESP8266WebServer httpUpdateServer(80);
ESP8266HTTPUpdateServer httpUpdater;
#endif

SoftwareSerial readerSerial(RX_PIN, TX_PIN); // RX, TX
WiFiClient espClient;
PubSubClient client(espClient);

bool sensorReady = false;

void checkConnection()
{
  if (client.connected())
  {
    return;
  }
  digitalWrite(LED_BUILTIN, LOW);
  int retries = 0;
  while (!client.connected())
  {
    if (retries < 150)
    {
      Serial.print("Attempting MQTT connection...");
      if (client.connect(MQTT_CLIENT_NAME, MQTT_USER, MQTT_PASS, MQTT_CLIENT_NAME "/availability", 0, true, "offline"))
      {
        Serial.println("connected");
        client.publish(MQTT_CLIENT_NAME "/availability", "online", true);
      }
      else
      {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        retries++;
        delay(5000);
      }
    }
    else
    {
      ESP.restart();
    }
  }
  digitalWrite(LED_BUILTIN, HIGH);
}

char buffer[16];
int previousCo2 = 0;

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;

  Serial.println("\nStarting");
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  readerSerial.begin(9600);

  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

#ifdef HTTPUpdateServer
  MDNS.begin(MQTT_CLIENT_NAME);

  httpUpdater.setup(&httpUpdateServer, USER_HTTP_USERNAME, USER_HTTP_PASSWORD);
  httpUpdateServer.begin();

  MDNS.addService("http", "tcp", 80);
#endif

  client.setServer(MQTT_SERVER, MQTT_PORT);

  checkConnection();

  unsigned status;

  // default settings
  status = bme.begin(0x76);
  // You can also pass in a Wire library object like &Wire2
  // status = bme.begin(0x76, &Wire2)
  if (!status)
  {
    Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
    Serial.print("SensorID was: 0x");
    Serial.println(bme.sensorID(), 16);
    Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
    Serial.print("        ID of 0x60 represents a BME 280.\n");
    Serial.print("        ID of 0x61 represents a BME 680.\n");
    while (1)
      delay(10);
  }
}

void loop()
{
  checkConnection();
  client.loop();
  client.loop();

  if (readerSerial.available() >= 16)
  {
    for (int i = 0; i < 16; i++)
    {
      buffer[i] = readerSerial.read();
      Serial.print(String(buffer[i], HEX) + " ");
      if (i > 1 && buffer[i] == 0x4d && buffer[i - 1] == 0x42)
      { // We are out of sync, start again
        i = 1;
        buffer[0] = 0x42;
        buffer[1] = 0x4d;
        Serial.println(" - Out of sync");
        Serial.print("42 4d ");
      }
    }

    Serial.println();

    if (buffer[0] == 0x42 && buffer[1] == 0x4d)
    {

      const int value = (buffer[6] << 8) | buffer[7];

      if (sensorReady == true || value != 511)
      {
        if (previousCo2 == 0 || abs(value - previousCo2) < 100)
        {
          sensorReady = true;
          previousCo2 = value;
          Serial.println("Received valid value: " + value);

          char buf[256];
          snprintf(buf, 256,
                   "{\"mcu_name\":\"" MQTT_CLIENT_NAME "\","
                   "\"co2\":%d,"
                   "\"temperature\":%f,"
                   "\"humidity\":%f,"
                   "\"pressure\":%f}",
                   value, bme.readTemperature(), bme.readHumidity(), bme.readPressure());
          Serial.print("Publish to MQTT: ");
          Serial.println(buf);
          client.publish(MQTT_CLIENT_NAME "/state", buf);

          client.loop();
          client.loop();

          Serial.println("Entering light sleep");
          wifi_set_sleep_type(LIGHT_SLEEP_T);
          delay(10000); // 10s
          client.loop();
        }
        else
        {
          Serial.println("Received invalid value: " + value);
        }
      }
      else
      {
        Serial.println("CO2 sensor not ready...");
        for (int i = 0; i < UPDATE_INTERVAL_MS; i += 10)
        {
          digitalWrite(LED_BUILTIN, LOW);
          delay(1);
          digitalWrite(LED_BUILTIN, HIGH);
          delay(9);
        }
        return;
      }
    }
    else
    {
      Serial.println("Invalid header received!");
    }
  }

#ifdef HTTPUpdateServer
  for (int i = 0; i < UPDATE_INTERVAL_MS; i += 10)
  {
    httpUpdateServer.handleClient();
    delay(10);
  }
#else
  delay(UPDATE_INTERVAL_MS);
#endif
}

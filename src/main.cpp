#define SRC_ESP_VER 1.3

#define TRIG_PIN 5
#define ECHO_PIN 18

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <string>

// WiFi credentials
const char *ssid = "StayTab";
const char *password = "        ";

// MQTT settings
const char *mqtt_server = "143.198.195.172";
const char *mqtt_token = "iw53jnne5x9zxffc9t3b";

// Value
static const int firmware_chunk_size = 220;
static float fw_version_new = 0;
const char *fw_title_new = nullptr;
static size_t totalLength = 0;
static size_t currentLength = 0;
int requestId = 0;
int chunkIndex = 0;
static bool updating = false;
long distance;

// temp string for mqtt
char topic[64] = "";
char payload[100];
String distanceStatus;

enum TopicType
{
  ATTRIBUTES_RESPONSE,
  ATTRIBUTES,
  FW_RESPONSE,
  NOT_THING
};

String topicStr;
TopicType topicType;

WiFiClient espClient;
PubSubClient client(espClient);

void reconnect() // reconnect to mqtt
{
  while (!client.connected())
  {
    Serial.println("Attempting MQTT connection...");
    if (client.connect("ESP32Client", mqtt_token, NULL))
    {
      Serial.println("MQTT connected");
      Serial.println("Source code version : " + String(SRC_ESP_VER));

      client.subscribe("v1/devices/me/attributes");
      client.subscribe("v2/fw/response/+/chunk/+");
      client.publish("v1/devices/me/attributes/request/1", "{\"sharedKeys\":\"fw_title,fw_version,fw_size\"}", 2);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void connection() // reconnect when lost connection
{
  if (!client.connected())
  {
    reconnect();
  }
  else
  {
    client.loop();
  }

  ArduinoOTA.handle();
}

void connectionTask(void *pvParameters) // RTOS connection task
{
  for (;;)
  {
    connection();
    vTaskDelay(pdMS_TO_TICKS(100)); // Adjust delay as needed
  }
}

void publish_distance() // publish distance to mqtt
{
  if (distance < 30)
    distanceStatus = "too close";
  else if (distance < 100)
    distanceStatus = "good distance";
  else
    distanceStatus = "too far";

  snprintf(payload, sizeof(payload), "{\"distance\": %ld, \"status\": \"%s\"}", distance, distanceStatus.c_str());
  client.publish("v1/devices/me/telemetry", payload, 2);
}

void distance_measurement() // detect distance by sensor
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  distance = pulseIn(ECHO_PIN, HIGH) * 0.034 / 2;

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  publish_distance(); // send distance to mqtt

  delay(2000);
}

void distanceTask(void *pvParameters) // RTOS distance task
{
  for (;;)
  {
    distance_measurement();
    vTaskDelay(pdMS_TO_TICKS(1000)); // Adjust delay as needed
  }
}

void connect_wifi() // connect wifi
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\n");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_ota() // OTA setup
{
  ArduinoOTA.onStart([]()
                     {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {
      type = "filesystem";
    }
    Serial.println("Start updating " + type); });

  ArduinoOTA.onEnd([]()
                   { Serial.println("\nEnd"); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });

  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    } });

  ArduinoOTA.begin();
}

TopicType getTopicType(const String &topic) // get topic of mqtt input
{
  if (topic.startsWith("v1/devices/me/attributes/response"))
    return ATTRIBUTES_RESPONSE;
  else if (topic.startsWith("v1/devices/me/attributes"))
    return ATTRIBUTES;

  if (topic.startsWith("v2/fw/response"))
    return FW_RESPONSE;

  return NOT_THING;
}

void requestFirmwareChunk() // send request firmware data
{
  snprintf(topic, sizeof(topic), "v2/fw/request/%d/chunk/%d", requestId, chunkIndex);
  client.publish(topic, String(firmware_chunk_size).c_str(), 2);
}

void mqtt_callback(char *topic, byte *payload, unsigned int length) // callback mqtt
{
  // Serial.print("Message arrived [");
  // Serial.print(topic);
  // Serial.print("] Payload length: ");
  // Serial.println(length);
  // Serial.println(String((char *)payload).substring(0, length));
  // Serial.println("-----------------------");

  topicStr = String(topic);
  topicType = getTopicType(topicStr);

  switch (topicType)
  {
  case ATTRIBUTES_RESPONSE: // check vision on thingboard when start ESP32
  {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error)
    {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }
    fw_title_new = strdup(doc["shared"]["fw_title"].as<const char *>());
    fw_version_new = atof(doc["shared"]["fw_version"]);
    totalLength = doc["shared"]["fw_size"];

    if (String(SRC_ESP_VER) != String(fw_version_new))
    {
      Serial.println("Total fw Length : " + String(totalLength));
      Serial.println("Update begin");
      chunkIndex = 0;
      currentLength = 0;
      updating = true;
      if (!Update.begin(totalLength))
      {
        Serial.println("Update begin failed");
        return;
      }
      requestFirmwareChunk();
    }
    else
    {
      Serial.println("Not need update");
    }

    break;
  }
  case ATTRIBUTES: // recive new assign vision
  {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error)
    {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }
    if (doc.containsKey("deleted"))
    {
      return;
    }

    fw_title_new = strdup(doc["fw_title"].as<const char *>());
    fw_version_new = atof(doc["fw_version"]);

    Serial.println("Device : " + String(requestId));
    Serial.println("Version now : " + String(SRC_ESP_VER));
    Serial.println("Find version : " + String(fw_version_new));

    totalLength = doc["fw_size"];

    if (String(SRC_ESP_VER) != String(fw_version_new))
    {
      Serial.println("Total fw Length : " + String(totalLength));
      Serial.println("Update begin");
      chunkIndex = 0;
      currentLength = 0;
      updating = true;
      if (!Update.begin(totalLength))
      {
        Serial.println("Update begin failed");
        return;
      }
      requestFirmwareChunk();
    }
    else
    {
      Serial.println("Not need update");
    }

    break;
  }
  case FW_RESPONSE: // rescive firmware data and update
  {
    size_t written = Update.write(payload, length);
    currentLength += written;
    if (written != length)
    {
      Serial.println("Failed to write chunk");
      Update.abort();
      updating = false;
    }
    else
    {
      Serial.printf("Written chunk %d, total %d/%d\n", chunkIndex, currentLength, totalLength);
      chunkIndex++;

      if (currentLength == totalLength)
      {
        Serial.println("Update complete");
        if (Update.end(true))
        {
          Serial.println("OTA Update successful, restarting...");
          ESP.restart();
        }
        else
        {
          Serial.println("Update failed");
          Update.abort();
          updating = false;
        }
      }
      else
      {
        requestFirmwareChunk();
      }
    }
    break;
  }
  }
}

void setup()
{
  Serial.begin(115200);
  connect_wifi();
  setup_ota();

  client.setServer(mqtt_server, 1883);
  client.setCallback(mqtt_callback);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  xTaskCreatePinnedToCore(connectionTask, "ConnectionTask", 10000, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(distanceTask, "DistanceTask", 10000, NULL, 1, NULL, 1);
}

void loop()
{
}
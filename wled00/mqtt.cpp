#include "wled.h"

/*
 * MQTT communication protocol for home automation
 */

#ifdef WLED_ENABLE_MQTT
#define MQTT_KEEP_ALIVE_TIME 60    // contact the MQTT broker every 60 seconds

void parseMQTTBriPayload(char* payload)
{
  if      (strstr(payload, "ON") || strstr(payload, "on") || strstr(payload, "true")) {bri = briLast; colorUpdated(1);}
  else if (strstr(payload, "T" ) || strstr(payload, "t" )) {toggleOnOff(); colorUpdated(1);}
  else {
    uint8_t in = strtoul(payload, NULL, 10);
    if (in == 0 && bri > 0) briLast = bri;
    bri = in;
    colorUpdated(NOTIFIER_CALL_MODE_DIRECT_CHANGE);
  }
}


void onMqttConnect(bool sessionPresent)
{
  //(re)subscribe to required topics
  char subuf[38];

  if (mqttDeviceTopic[0] != 0)
  {
    strcpy(subuf, mqttDeviceTopic);
    mqtt->subscribe(subuf, 0);
    strcat(subuf, "/state");
  }

  if (mqttGroupTopic[0] != 0)
  {
    strcpy(subuf, mqttGroupTopic);
    mqtt->subscribe(subuf, 0);
    strcat(subuf, "/col");
    mqtt->subscribe(subuf, 0);
    strcpy(subuf, mqttGroupTopic);
    strcat(subuf, "/api");
    mqtt->subscribe(subuf, 0);
  }

  doPublishMqtt = true;
  DEBUG_PRINTLN(F("MQTT ready"));
}


void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {

  DEBUG_PRINT(F("MQTT msg: "));
  DEBUG_PRINTLN(topic);

  // paranoia check to avoid npe if no payload
  if (payload==nullptr) {
    DEBUG_PRINTLN(F("no payload -> leave"));
    return;
  }
  DEBUG_PRINTLN(payload);
  size_t topicPrefixLen = strlen(mqttDeviceTopic);
  if (strncmp(topic, mqttDeviceTopic, topicPrefixLen) == 0) {
    DEBUG_PRINTLN("Bien hecho");
    StaticJsonDocument<200> jsonBuffer;
    DeserializationError error = deserializeJson(jsonBuffer, payload);
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return;
    }
    JsonObject toor = jsonBuffer.as<JsonObject>();
    deserializeHA(toor);
    topic += topicPrefixLen;
  }

  else if (strstr(topic, "/api"))
  {
    if (payload[0] == '{') { //JSON API
      DynamicJsonDocument doc(JSON_BUFFER_SIZE);
      deserializeJson(doc, payload);
      deserializeState(doc.as<JsonObject>());
    } else { //HTTP API
      String apireq = "win&";
      apireq += (char*)payload;
      handleSet(nullptr, apireq);
    }
  } else if (strcmp(topic, "") == 0)
  {
    parseMQTTBriPayload(payload);
  }
  if (strncmp(topic, mqttDeviceTopic, topicPrefixLen) == 0) {
      topic += topicPrefixLen;
  } else {
      size_t topic_prefix_len = strlen(mqttGroupTopic);
      if (strncmp(topic, mqttGroupTopic, topicPrefixLen) == 0) {
          topic += topicPrefixLen;
      } else {
          // Topic not used here. Probably a usermod subscribed to this topic.
          return;
      }
  }
  DEBUG_PRINT(topic);
  DEBUG_PRINT("=");
  DEBUG_PRINTLN(mqttDeviceTopic);
  //Prefix is stripped from the topic at this point
}


void publishMqtt()
{
  doPublishMqtt = false;
  if (!WLED_MQTT_CONNECTED) return;
  DEBUG_PRINTLN(F("Publish MQTT"));

  char s[10];
  char subuf[38];
  String obj;

  // sprintf(s, "%ld", bri);
  // strcpy(subuf, mqttDeviceTopic);
  // strcat(subuf, "/g");
  // mqtt->publish(subuf, 0, true, s);

  // sprintf(s, "#%06X", (col[3] << 24) | (col[0] << 16) | (col[1] << 8) | (col[2]));
  // strcpy(subuf, mqttDeviceTopic);
  // strcat(subuf, "/c");
  // mqtt->publish(subuf, 0, true, s);

  StaticJsonDocument<200> doc1;
  JsonObject state1 = doc1.to<JsonObject>();
  serializeHA(state1);
  serializeJsonPretty(doc1, obj);
  strcpy(subuf, mqttDeviceTopic);
  strcat(subuf, "/state");
  mqtt->publish(subuf, 0, true, obj.c_str());

  // char apires[1024];
  // XML_response(nullptr, apires);
  // strcpy(subuf, mqttDeviceTopic);
  // strcat(subuf, "/v");
  // mqtt->publish(subuf, 0, true, apires);
}


//HA autodiscovery was removed in favor of the native integration in HA v0.102.0

bool initMqtt()
{
  lastMqttReconnectAttempt = millis();
  if (!mqttEnabled || mqttServer[0] == 0 || !WLED_CONNECTED) return false;

  if (mqtt == nullptr) {
    mqtt = new AsyncMqttClient();
    mqtt->onMessage(onMqttMessage);
    mqtt->onConnect(onMqttConnect);
  }
  if (mqtt->connected()) return true;

  DEBUG_PRINTLN(F("Reconnecting MQTT"));
  IPAddress mqttIP;
  if (mqttIP.fromString(mqttServer)) //see if server is IP or domain
  {
    mqtt->setServer(mqttIP, mqttPort);
  } else {
    mqtt->setServer(mqttServer, mqttPort);
  }
  mqtt->setClientId(mqttClientID);
  if (mqttUser[0] && mqttPass[0]) mqtt->setCredentials(mqttUser, mqttPass);

  strcpy(mqttStatusTopic, mqttDeviceTopic);
  strcat(mqttStatusTopic, "/status");
  mqtt->setWill(mqttStatusTopic, 0, true, "offline");
  mqtt->setKeepAlive(MQTT_KEEP_ALIVE_TIME);
  mqtt->connect();
  return true;
}

#else
bool initMqtt(){return false;}
void publishMqtt(){}
#endif

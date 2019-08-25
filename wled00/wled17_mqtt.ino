/*
 * MQTT communication protocol for home automation
 */

#define WLED_MQTT_PORT 1883

void parseMQTTBriPayload(char* payload)
{
  if      (strstr(payload, "ON") || strstr(payload, "on") || strstr(payload, "true")) {bri = briLast; colorUpdated(1);}
  else if (strstr(payload, "T" ) || strstr(payload, "t" )) {toggleOnOff(); colorUpdated(1);}
  else {
    uint8_t in = strtoul(payload, NULL, 10);
    if (in == 0 && bri > 0) briLast = bri;
    bri = in;
    colorUpdated(1);
  }
}


void onMqttConnect(bool sessionPresent)
{
  //(re)subscribe to required topics
  char subuf[38];
  strcpy(subuf, mqttDeviceTopic);
  
  if (mqttDeviceTopic[0] != 0)
  {
    strcpy(subuf, mqttDeviceTopic);
    mqtt->subscribe(subuf, 0);
    strcat(subuf, "/col");
    mqtt->subscribe(subuf, 0);
    strcpy(subuf, mqttDeviceTopic);
    strcat(subuf, "/api");
    mqtt->subscribe(subuf, 0);

    //Me suscribo al Speed, como Pocholo
    strcpy(subuf, mqttDeviceTopic);
    strcat(subuf, "/speed");
    mqtt->subscribe(subuf, 0);

    //Me suscribo al Intentsity
    strcpy(subuf, mqttDeviceTopic);
    strcat(subuf, "/intent");
    mqtt->subscribe(subuf, 0);

    //Me suscribo al color_t
    strcpy(subuf, mqttDeviceTopic);
    strcat(subuf, "/ct");
    mqtt->subscribe(subuf, 0);
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

  //sendHADiscoveryMQTT();
  publishMqtt();
  DEBUG_PRINTLN("MQTT ready");
}


void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {

  DEBUG_PRINT("MQTT callb rec: ");
  DEBUG_PRINTLN(topic);
  DEBUG_PRINTLN(payload);

  if (!strcmp(topic, "/habitacion/escritorio"))
  {
  DEBUG_PRINTLN("Bien hecho");
  StaticJsonDocument<500> doc;
  DeserializationError error = deserializeJson(doc, payload);
   if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }
  JsonObject toor = doc.as<JsonObject>();
  deserializeHA(toor);}
}


void publishMqtt()
{
  if (mqtt == NULL) return;
  if (!mqtt->connected()) return;
  char subuf[35];
  String object;
  DEBUG_PRINTLN("Publish MQTT");
  const size_t bufferSize = JSON_ARRAY_SIZE(3) + JSON_OBJECT_SIZE(8) + 500;
  DynamicJsonDocument jsonBuffer(bufferSize);
  // create an object
  JsonObject toor = jsonBuffer.to<JsonObject>();
  serializeHA(toor);
  serializeJson(toor, object);
  strcpy(subuf, mqttDeviceTopic);
  strcat(subuf, "/state");
  mqtt->publish(subuf, 0, true, object.c_str());

}

bool initMqtt()
{
  if (mqttServer[0] == 0) return false;
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!mqtt) mqtt = new AsyncMqttClient();
  if (mqtt->connected()) return true;
  
  IPAddress mqttIP;
  if (mqttIP.fromString(mqttServer)) //see if server is IP or domain
  {
    mqtt->setServer(mqttIP, WLED_MQTT_PORT);
  } else {
    mqtt->setServer(mqttServer, WLED_MQTT_PORT);
  }
  mqtt->setClientId(clientID);
  //When Credentials is activated send it to server
  if(mqttcredential){mqtt->setCredentials(MQTTuser, MQTTpass);}
  mqtt->onMessage(onMqttMessage);
  mqtt->onConnect(onMqttConnect);
  mqtt->connect();
  return true;
}

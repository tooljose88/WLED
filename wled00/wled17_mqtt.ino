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
  char s[10];
  if (bri > 0){toor["state"]="ON";} else{toor["state"]="OFF";}
  toor["brightness"] = bri;
  toor["transition"] = effectSpeed;

  JsonObject nl = toor.createNestedObject("color");
  nl["r"] = col[0];
  nl["g"] = col[1];
  nl["b"] = col[2]; 

  toor["color_temp"] = tempcol;
  toor["intensity"] = effectIntensity;
  toor["transition"] = effectSpeed;
  sprintf (s, "[FX=%02d] %s", strip.getMode(), efectos[strip.getMode()]);
  toor["effect"] = s;
  //serializeHA(doc);
  size_t jlen = measureJson(toor);
  //serializeJson(toor, object, jlen);
  serializeJson(toor, object);
  strcpy(subuf, mqttDeviceTopic);
  strcat(subuf, "/state");
  mqtt->publish(subuf, 0, true, object.c_str());


  // char s[10];
  // char subuf[38];
  
  // sprintf(s, "%ld", bri);
  // strcpy(subuf, mqttDeviceTopic);
  // strcat(subuf, "/g");
  // mqtt->publish(subuf, 0, true, s);

  // // sprintf(s, "#%06X", col[3]*16777216 + col[0]*65536 + col[1]*256 + col[2]);
  // // strcpy(subuf, mqttDeviceTopic);
  // // strcat(subuf, "/c");
  // // mqtt->publish(subuf, 0, true, s);
  // sprintf(s, "%u,%u,%u", col[0], col[1], col[2]);
  // strcpy(subuf, mqttDeviceTopic);
  // strcat(subuf, "/c");
  // mqtt->publish(subuf, 0, true, s);

  // //Envio del estado encendido/apagado
  // strcpy(subuf, mqttDeviceTopic);
  // strcat(subuf, "/state");
  // if (bri>0){strcpy(s, "ON");}else{strcpy(s, "OFF");}
  // mqtt->publish(subuf, 0, true, s);
  // DEBUG_PRINTLN(strip.getSpeed());

  // //Envio del Efecto seleccionado
  // strcpy(subuf, mqttDeviceTopic);
  // strcat(subuf, "/api/state");
  // sprintf (s, "[FX=%02d] %s", strip.getMode(), efectos[strip.getMode()]);
  // mqtt->publish(subuf, 0, true, s);

  // //Envio la velocidad
  // strcpy(subuf, mqttDeviceTopic);
  // strcat(subuf, "/speed/state");
  // sprintf (s, "%d", strip.getSpeed());
  // mqtt->publish(subuf, 0, true, s);

  // //Envio la intensidad
  // strcpy(subuf, mqttDeviceTopic);
  // strcat(subuf, "/intent/state");
  // sprintf (s, "%d", effectIntensity);
  // mqtt->publish(subuf, 0, true, s);

  // //Envío de Temp. Color
  // strcpy(subuf, mqttDeviceTopic);
  // strcat(subuf, "/ct/state");
  // sprintf (s, "%d", tempcol);
  // mqtt->publish(subuf, 0, true, s);

  // char apires[1024];
  // XML_response(nullptr, false, apires);
  // strcpy(subuf, mqttDeviceTopic);
  // strcat(subuf, "/v");
  // mqtt->publish(subuf, 0, true, apires);
}

//const char HA_static_JSON[] PROGMEM = R"=====(,"bri_val_tpl":"{{value}}","rgb_cmd_tpl":"{{'#%02x%02x%02x' | format(red, green, blue)}}","rgb_val_tpl":"{{value[1:3]|int(base=16)}},{{value[3:5]|int(base=16)}},{{value[5:7]|int(base=16)}}","qos":0,"opt":true,"pl_on":"ON","pl_off":"OFF","fx_val_tpl":"{{value}}","fx_list":[)=====";

// void sendHADiscoveryMQTT()
// {
  
// #if ARDUINO_ARCH_ESP32 || LEDPIN != 3
// /*

// YYYY is discovery tipic
// XXXX is device name

// Send out HA MQTT Discovery message on MQTT connect (~2.4kB):
// {
// "name": "XXXX",
// "stat_t":"YYYY/c",
// "cmd_t":"YYYY",
// "rgb_stat_t":"YYYY/c",
// "rgb_cmd_t":"YYYY/col",
// "bri_cmd_t":"YYYY",
// "bri_stat_t":"YYYY/g",
// "bri_val_tpl":"{{value}}",
// "rgb_cmd_tpl":"{{'#%02x%02x%02x' | format(red, green, blue)}}",
// "rgb_val_tpl":"{{value[1:3]|int(base=16)}},{{value[3:5]|int(base=16)}},{{value[5:7]|int(base=16)}}",
// "qos": 0,
// "opt":true,
// "pl_on": "ON",
// "pl_off": "OFF",
// "fx_cmd_t":"YYYY/api",
// "fx_stat_t":"YYYY/api",
// "fx_val_tpl":"{{value}}",
// "fx_list":[
// "[FX=00] Solid",
// "[FX=01] Blink", 
// "[FX=02] ...",
// "[FX=79] Ripple"
// ]
// }

//   */
//   char bufc[36], bufcc[4], bufcol[38], bufg[36], bufapi[38], buffer[2500];

//   strcpy(bufc, mqttDeviceTopic);
//   strcpy(bufcol, mqttDeviceTopic);
//   strcpy(bufg, mqttDeviceTopic);
//   strcpy(bufapi, mqttDeviceTopic);
//   if (bri=0){strcpy(bufcc, "OFF");}else{(bufcc, "ON");}
//   strcat(bufc, "/c");
//   strcat(bufcol, "/col");
//   strcat(bufg, "/g");
//   strcat(bufapi, "/api");

//   StaticJsonDocument<JSON_OBJECT_SIZE(9) +512> toor;
//   toor["name"] = serverDescription;
//   toor["stat_t"] = bufc;
//   toor["cmd_t"] = mqttDeviceTopic;
//   toor["rgb_stat_t"] = bufc;
//   toor["rgb_cmd_t"] = bufcol;
//   toor["bri_cmd_t"] = mqttDeviceTopic;
//   toor["bri_stat_t"] = bufg;
//   toor["fx_cmd_t"] = bufapi;
//   toor["fx_stat_t"] = bufapi;

//   size_t jlen = measureJson(toor);
//   DEBUG_PRINTLN(jlen);
//   serializeJson(toor, buffer, jlen);

//   //add values which don't change
//   strcpy_P(buffer + jlen -1, HA_static_JSON);

//   olen = 0;
//   obuf = buffer + jlen -1 + strlen_P(HA_static_JSON);

//   //add fx_list
//   uint16_t jmnlen = strlen_P(JSON_mode_names);
//   uint16_t nameStart = 0, nameEnd = 0;
//   int i = 0;
//   bool isNameStart = true;

//   for (uint16_t j = 0; j < jmnlen; j++)
//   {
//     if (pgm_read_byte(JSON_mode_names + j) == '\"' || j == jmnlen -1)
//     {
//       if (isNameStart) 
//       {
//         nameStart = j +1;
//       }
//       else 
//       {
//         nameEnd = j;
//         char mdnfx[64], mdn[56];
//         uint16_t namelen = nameEnd - nameStart;
//         strncpy_P(mdn, JSON_mode_names + nameStart, namelen);
//         mdn[namelen] = 0;
//         snprintf(mdnfx, 64, "\"[FX=%02d] %s\",", i, mdn);
//         oappend(mdnfx);
//         DEBUG_PRINTLN(mdnfx);
//         i++;
//       }
//       isNameStart = !isNameStart;
//     }   
//   }
//   olen--;
//   oappend("]}");

//   DEBUG_PRINT("HA Discovery Sending >>");
//   DEBUG_PRINTLN(buffer);

//   char pubt[25 + 12 + 8];
//   strcpy(pubt, "homeassistant/light/WLED_");
//   strcat(pubt, escapedMac.c_str());
//   strcat(pubt, "/config");
//   mqtt->publish(pubt, 0, true, buffer);
// #endif
// }

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

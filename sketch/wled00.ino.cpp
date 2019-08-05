#line 1 "/home/topota/Arduino/WLED/wled00/wled00.ino"
#line 1 "/home/topota/Arduino/WLED/wled00/wled00.ino"
/*
 * Main sketch, global variable declarations
 */
/*
 * @title WLED project sketch
 * @version 0.8.4
 * @author Christian Schwinne
 */


//ESP8266-01 (blue) got too little storage space to work with all features of WLED. To use it, you must use ESP8266 Arduino Core v2.4.2 and the setting 512K(No SPIFFS).

//ESP8266-01 (black) has 1MB flash and can thus fit the whole program. Use 1M(64K SPIFFS).
//Uncomment some of the following lines to disable features to compile for ESP8266-01 (max flash size 434kB):

//You are required to disable over-the-air updates:
//#define WLED_DISABLE_OTA

//You need to choose 1-2 of these features to disable:
//#define WLED_DISABLE_ALEXA
//#define WLED_DISABLE_BLYNK
//#define WLED_DISABLE_CRONIXIE
//#define WLED_DISABLE_HUESYNC
//#define WLED_DISABLE_INFRARED    //there is no pin left for this on ESP8266-01
//#define WLED_DISABLE_MOBILE_UI

#define LED_BUILTIN 0
#define WLED_DISABLE_FILESYSTEM   
 //SPIFFS is not used by any WLED feature yet
//#define WLED_ENABLE_FS_SERVING   //Enable sending html file from SPIFFS before serving progmem version
//#define WLED_ENABLE_FS_EDITOR    //enable /edit page for editing SPIFFS content. Will also be disabled with OTA lock
//to toggle usb serial debug (un)comment the following line
#define WLED_DEBUG


//library inclusions
#include <Arduino.h>
#ifdef ARDUINO_ARCH_ESP32
 #include <WiFi.h>
 #include <ESPmDNS.h>
 #include <AsyncTCP.h>
 #include "SPIFFS.h"
#else
 #include <ESP8266WiFi.h>
 #include <ESP8266mDNS.h>
 #include <ESPAsyncTCP.h>
#endif

#include <ESPAsyncWebServer.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <DNSServer.h>
#ifndef WLED_DISABLE_OTA
 #include <ArduinoOTA.h>
#endif
#include <SPIFFSEditor.h>
#include "src/dependencies/time/Time.h"
#include "src/dependencies/time/TimeLib.h"
#include "src/dependencies/timezone/Timezone.h"
#ifndef WLED_DISABLE_ALEXA
 #define ESPALEXA_ASYNC
 #define ESPALEXA_NO_SUBPAGE
 #define ESPALEXA_MAXDEVICES 1
 #include "src/dependencies/espalexa/Espalexa.h"
#endif
#ifndef WLED_DISABLE_BLYNK
 #include "src/dependencies/blynk/BlynkSimpleEsp.h"
#endif
#include "src/dependencies/e131/E131.h"
#include "src/dependencies/async-mqtt-client/AsyncMqttClient.h"
#include "src/dependencies/json/AsyncJson-v6.h"
#include "src/dependencies/json/ArduinoJson-v6.h"
#include "html_classic.h"
#include "html_mobile.h"
#include "html_settings.h"
#include "html_other.h"
#include "WS2812FX.h"
#include "ir_codes.h"
#include <Ticker.h>


#if IR_PIN < 0
 #ifndef WLED_DISABLE_INFRARED
  #define WLED_DISABLE_INFRARED
 #endif
#endif

#ifdef ARDUINO_ARCH_ESP32
 /*#ifndef WLED_DISABLE_INFRARED
  #include <IRremote.h>
 #endif*/ //there are issues with ESP32 infrared, so it is disabled for now
#else
 #ifndef WLED_DISABLE_INFRARED
  #include <IRremoteESP8266.h>
  #include <IRrecv.h>
  #include <IRutils.h>
 #endif
#endif


//version code in format yymmddb (b = daily build)
#define VERSION 1906201
char versionString[] = "0.8.5-dev";


//AP and OTA default passwords (for maximum change them!)
char apPass[65] = "wled1234";
char otaPass[33] = "wledota";


//Hardware CONFIG (only changeble HERE, not at runtime)
//LED strip pin, button pin and IR pin changeable in NpbWrapper.h!

byte auxDefaultState   = 0;                   //0: input 1: high 2: low
byte auxTriggeredState = 0;                   //0: input 1: high 2: low
char ntpServerName[] = "0.wled.pool.ntp.org"; //NTP server to use


//WiFi CONFIG (all these can be changed via web UI, no need to set them here)
char clientSSID[33] = "Your_Network";
char clientPass[65] = "";
char cmDNS[33] = "x";                         //mDNS address (placeholder, will be replaced by wledXXXXXXXXXXXX.local)
char apSSID[33] = "";                         //AP off by default (unless setup)
byte apChannel = 1;                           //2.4GHz WiFi AP channel (1-13)
byte apHide = 0;                              //hidden AP SSID
byte apWaitTimeSecs = 32;                     //time to wait for connection before opening AP
bool recoveryAPDisabled = false;              //never open AP (not recommended)
IPAddress staticIP(0, 0, 0, 0);               //static IP of ESP
IPAddress staticGateway(0, 0, 0, 0);          //gateway (router) IP
IPAddress staticSubnet(255, 255, 255, 0);     //most common subnet in home networks

//MQTT CONFIG (Prepare for web UI)
char clientID[15] = "WLED";
char MQTTuser[15] = "";
char MQTTpass[25] = "";
bool mqttcredential = false;


//LED CONFIG
uint16_t ledCount = 30;                       //overcurrent prevented by ABL             
bool useRGBW = false;                         //SK6812 strips can contain an extra White channel
bool autoRGBtoRGBW = false;                   //if RGBW enabled, calculate White channel from RGB
#define ABL_MILLIAMPS_DEFAULT 850;            //auto lower brightness to stay close to milliampere limit 
bool turnOnAtBoot  = true;                    //turn on LEDs at power-up
byte bootPreset = 0;                          //save preset to load after power-up

byte colS[]{255, 159, 0, 0};                  //default RGB(W) color
byte colSecS[]{0, 0, 0, 0};                   //default RGB(W) secondary color
byte briS = 127;                              //default brightness
byte effectDefault = 0;                   
byte effectSpeedDefault = 75;
byte effectIntensityDefault = 128;            //intensity is supported on some effects as an additional parameter (e.g. for blink you can change the duty cycle)
byte effectPaletteDefault = 0;                //palette is supported on the FastLED effects, otherwise it has no effect

//bool strip.gammaCorrectBri = false;         //gamma correct brightness (not recommended) --> edit in WS2812FX.h
//bool strip.gammaCorrectCol = true;          //gamma correct colors (strongly recommended)

byte nightlightTargetBri = 0;                 //brightness after nightlight is over
byte nightlightDelayMins = 60;
bool nightlightFade = true;                   //if enabled, light will gradually dim towards the target bri. Otherwise, it will instantly set after delay over
bool fadeTransition = true;                   //enable crossfading color transition
bool enableSecTransition = true;              //also enable transition for secondary color
uint16_t transitionDelay = 750;               //default crossfade duration in ms

//bool strip.reverseMode  = false;            //flip entire LED strip (reverses all effect directions) --> edit in WS2812FX.h
bool skipFirstLed = false;                    //ignore first LED in strip (useful if you need the LED as signal repeater)
byte briMultiplier =  100;                    //% of brightness to set (to limit power, if you set it to 50 and set bri to 255, actual brightness will be 127)


//User Interface CONFIG
char serverDescription[33] = "WLED_Light";    //Name of module
byte currentTheme = 7;                        //UI theme index for settings and classic UI
byte uiConfiguration = 0;                     //0: automatic (depends on user-agent) 1: classic UI 2: mobile UI
bool useHSB = true;                           //classic UI: use HSB sliders instead of RGB by default
char cssFont[33] = "Verdana";                 //font to use in classic UI

bool useHSBDefault = useHSB;


//Sync CONFIG
bool buttonEnabled =  true;
bool irEnabled     = false;                   //Infrared receiver

uint16_t udpPort    = 21324;                  //WLED notifier default port
uint16_t udpRgbPort = 19446;                  //Hyperion port

bool receiveNotificationBrightness = true;    //apply brightness from incoming notifications
bool receiveNotificationColor      = true;    //apply color
bool receiveNotificationEffects    = true;    //apply effects setup
bool notifyDirect = false;                    //send notification if change via UI or HTTP API
bool notifyButton = false;                    //send if updated by button or infrared remote
bool notifyAlexa  = false;                    //send notification if updated via Alexa
bool notifyMacro  = false;                    //send notification for macro
bool notifyHue    =  true;                    //send notification if Hue light changes
bool notifyTwice  = false;                    //notifications use UDP: enable if devices don't sync reliably

bool alexaEnabled = true;                     //enable device discovery by Amazon Echo
char alexaInvocationName[33] = "Light";       //speech control name of device. Choose something voice-to-text can understand

char blynkApiKey[36] = "";                    //Auth token for Blynk server. If empty, no connection will be made

uint16_t realtimeTimeoutMs = 2500;            //ms timeout of realtime mode before returning to normal mode
int  arlsOffset = 0;                          //realtime LED offset
bool receiveDirect    =  true;                //receive UDP realtime
bool arlsDisableGammaCorrection = true;       //activate if gamma correction is handled by the source
bool arlsForceMaxBri = false;                 //enable to force max brightness if source has very dark colors that would be black

bool e131Enabled = true;                      //settings for E1.31 (sACN) protocol
uint16_t e131Universe = 1;
bool e131Multicast = false;

char mqttDeviceTopic[33] = "/habitacion/escritorio";                //main MQTT topic (individual per device, default is wled/mac)
char mqttGroupTopic[33] = "wled/all";         //second MQTT topic (for example to group devices)
char mqttServer[33] = "";                     //both domains and IPs should work (no SSL)

bool huePollingEnabled = false;               //poll hue bridge for light state
uint16_t huePollIntervalMs = 2500;            //low values (< 1sec) may cause lag but offer quicker response
char hueApiKey[47] = "api";                   //key token will be obtained from bridge
byte huePollLightId = 1;                      //ID of hue lamp to sync to. Find the ID in the hue app ("about" section)
IPAddress hueIP = (0,0,0,0);                  //IP address of the bridge
bool hueApplyOnOff = true;
bool hueApplyBri   = true;
bool hueApplyColor = true;


//Time CONFIG
bool ntpEnabled = false;                      //get internet time. Only required if you use clock overlays or time-activated macros
bool useAMPM = false;                         //12h/24h clock format
byte currentTimezone = 0;                     //Timezone ID. Refer to timezones array in wled10_ntp.ino
int  utcOffsetSecs   = 0;                     //Seconds to offset from UTC before timzone calculation

byte overlayDefault = 0;                      //0: no overlay 1: analog clock 2: single-digit clocl 3: cronixie
byte overlayMin = 0, overlayMax = ledCount-1; //boundaries of overlay mode

byte analogClock12pixel = 0;                  //The pixel in your strip where "midnight" would be
bool analogClockSecondsTrail = false;         //Display seconds as trail of LEDs instead of a single pixel
bool analogClock5MinuteMarks = false;         //Light pixels at every 5-minute position

char cronixieDisplay[7] = "HHMMSS";           //Cronixie Display mask. See wled13_cronixie.ino
bool cronixieBacklight = true;                //Allow digits to be back-illuminated

bool countdownMode = false;                   //Clock will count down towards date
byte countdownYear = 19, countdownMonth = 1;  //Countdown target date, year is last two digits
byte countdownDay  =  1, countdownHour  = 0;
byte countdownMin  =  0, countdownSec   = 0;


byte macroBoot = 0;                           //macro loaded after startup
byte macroNl = 0;                             //after nightlight delay over
byte macroCountdown = 0;                      
byte macroAlexaOn = 0, macroAlexaOff = 0;
byte macroButton = 0, macroLongPress = 0, macroDoublePress = 0;


//Security CONFIG
bool otaLock = false;                         //prevents OTA firmware updates without password. ALWAYS enable if system exposed to any public networks
bool wifiLock = false;                        //prevents access to WiFi settings when OTA lock is enabled
bool aOtaEnabled = true;                      //ArduinoOTA allows easy updates directly from the IDE. Careful, it does not auto-disable when OTA lock is on


uint16_t userVar0 = 0, userVar1 = 0;


//internal global variable declarations
//color
byte col[]{255, 159, 0, 0};                   //target RGB(W) color
byte colOld[]{0, 0, 0, 0};                    //color before transition
byte colT[]{0, 0, 0, 0};                      //current color
byte colIT[]{0, 0, 0, 0};                     //color that was last sent to LEDs
byte colSec[]{0, 0, 0, 0};
byte colSecT[]{0, 0, 0, 0};
byte colSecOld[]{0, 0, 0, 0};
byte colSecIT[]{0, 0, 0, 0};

byte lastRandomIndex = 0;                     //used to save last random color so the new one is not the same

//transitions
bool transitionActive = false;
uint16_t transitionDelayDefault = transitionDelay;
uint16_t transitionDelayTemp = transitionDelay;
unsigned long transitionStartTime;
float tperLast = 0;                           //crossfade transition progress, 0.0f - 1.0f

//nightlight
bool nightlightActive = false;
bool nightlightActiveOld = false;
uint32_t nightlightDelayMs = 10;
uint8_t nightlightDelayMinsDefault = nightlightDelayMins;
unsigned long nightlightStartTime;
byte briNlT = 0;                              //current nightlight brightness
//effects
const char* efectos[] = {
"Solid","Blink","Breathe","Wipe","Wipe Random","Random Colors","Sweep","Dynamic","Colorloop","Rainbow",
"Scan","Dual Scan","Fade","Chase","Chase Rainbow","Running","Saw","Twinkle","Dissolve","Dissolve Rnd",
"Sparkle","Dark Sparkle","Sparkle+","Strobe","Strobe Rainbow","Mega Strobe","Blink Rainbow","Android","Chase","Chase Random",
"Chase Rainbow","Chase Flash","Chase Flash Rnd","Rainbow Runner","Colorful","Traffic Light","Sweep Random","Running 2","Red & Blue","Stream",
"Scanner","Lighthouse","Fireworks","Rain","Merry Christmas","Fire Flicker","Gradient","Loading","In Out","In In",
"Out Out","Out In","Circus","Halloween","Tri Chase","Tri Wipe","Tri Fade","Lightning","ICU","Multi Comet",
"Dual Scanner","Stream 2","Oscillate","Pride 2015","Juggle","Palette","Fire 2012","Colorwaves","BPM","Fill Noise","Noise 1",
"Noise 2","Noise 3","Noise 4","Colortwinkle","Lake","Meteor","Smooth Meteor","Railway","Ripple"};

//brightness
unsigned long lastOnTime = 0;
bool offMode = !turnOnAtBoot;
byte bri = briS;
byte briOld = 0;
byte briT = 0;
byte briIT = 0;
byte briLast = 127;                           //brightness before turned off. Used for toggle function

//button
bool buttonPressedBefore = false;
unsigned long buttonPressedTime = 0;
unsigned long buttonWaitTime = 0;

//notifications
bool notifyDirectDefault = notifyDirect;
bool receiveNotifications = true;
unsigned long notificationSentTime = 0;
byte notificationSentCallMode = 0;
bool notificationTwoRequired = false;

//effects
byte effectCurrent = effectDefault;
byte effectSpeed = effectSpeedDefault;
byte effectIntensity = effectIntensityDefault;
byte effectPalette = effectPaletteDefault;

//network
bool onlyAP = false;                          //only Access Point active, no connection to home network
bool udpConnected = false, udpRgbConnected = false;

//ui style
char cssCol[6][9]={"","","","","",""};
bool showWelcomePage = false;

//hue
char hueError[25] = "Inactive";
//uint16_t hueFailCount = 0;
float hueXLast=0, hueYLast=0;
uint16_t hueHueLast=0, hueCtLast=0;
byte hueSatLast=0, hueBriLast=0;
unsigned long hueLastRequestSent = 0;
bool hueAuthRequired = false;
bool hueReceived = false;
bool hueStoreAllowed = false, hueNewKey = false;
//unsigned long huePollIntervalMsTemp = huePollIntervalMs;
//bool hueAttempt = false;

//overlays
byte overlayCurrent = overlayDefault;
byte overlaySpeed = 200;
unsigned long overlayRefreshMs = 200;
unsigned long overlayRefreshedTime;
int overlayArr[6];
uint16_t overlayDur[6];
uint16_t overlayPauseDur[6];
int nixieClockI = -1;
bool nixiePause = false;

//cronixie
byte dP[]{0,0,0,0,0,0};
bool cronixieInit = false;

//countdown
unsigned long countdownTime = 1514764800L;
bool countdownOverTriggered = true;

//timer
byte lastTimerMinute = 0;
byte timerHours[]   = {0,0,0,0,0,0,0,0};
byte timerMinutes[] = {0,0,0,0,0,0,0,0};
byte timerMacro[]   = {0,0,0,0,0,0,0,0};
byte timerWeekday[] = {255,255,255,255,255,255,255,255}; //weekdays to activate on
//bit pattern of arr elem: 0b11111111: sun,sat,fri,thu,wed,tue,mon,validity
Ticker ticker;

//blynk
bool blynkEnabled = false;

//preset cycling
bool presetCyclingEnabled = false;
byte presetCycleMin = 1, presetCycleMax = 5;
uint16_t presetCycleTime = 1250;
unsigned long presetCycledTime = 0; byte presetCycCurr = presetCycleMin;
bool presetApplyBri = false, presetApplyCol = true, presetApplyFx = true;
bool saveCurrPresetCycConf = false;

//realtime
bool realtimeActive = false;
IPAddress realtimeIP = (0,0,0,0);
unsigned long realtimeTimeout = 0;

//mqtt
long nextMQTTReconnectAttempt = 0;
long lastInterfaceUpdate = 0;
byte interfaceUpdateCallMode = 0;

#if AUXPIN >= 0
//auxiliary debug pin
byte auxTime = 0;
unsigned long auxStartTime = 0;
bool auxActive = false, auxActiveBefore = false;
#endif

//alexa udp
String escapedMac;
#ifndef WLED_DISABLE_ALEXA
Espalexa espalexa;
EspalexaDevice* espalexaDevice;
#endif

//dns server
DNSServer dnsServer;
bool dnsActive = false;

//network time
bool ntpConnected = false;
time_t local = 0;
unsigned long ntpLastSyncTime = 999000000L;
unsigned long ntpPacketSentTime = 999000000L;
IPAddress ntpServerIP;
unsigned int ntpLocalPort = 2390;
#define NTP_PACKET_SIZE 48

//string temp buffer (now stored in stack locally)
#define OMAX 2048
char* obuf;
uint16_t olen = 0;

String messageHead, messageSub;
byte optionType;

bool doReboot = false; //flag to initiate reboot from async handlers

//server library objects
AsyncWebServer server(80);
AsyncClient* hueClient = NULL;
AsyncMqttClient* mqtt = NULL;

//udp interface objects
WiFiUDP notifierUdp, rgbUdp;
WiFiUDP ntpUdp;
E131* e131;

//led fx library object
WS2812FX strip = WS2812FX();

//debug macros
#ifdef WLED_DEBUG
 #define DEBUG_PRINT(x)  Serial.print (x)
 #define DEBUG_PRINTLN(x) Serial.println (x)
 #define DEBUG_PRINTF(x) Serial.printf (x)
 unsigned long debugTime = 0;
 int lastWifiState = 3;
 unsigned long wifiStateChangedTime = 0;
#else
 #define DEBUG_PRINT(x)
 #define DEBUG_PRINTLN(x)
 #define DEBUG_PRINTF(x)
#endif

//filesystem
#ifndef WLED_DISABLE_FILESYSTEM
 #include <FS.h>
 #ifdef ARDUINO_ARCH_ESP32
  #include "SPIFFS.h"
 #endif
 #include "SPIFFSEditor.h"
#endif

//function prototypes
void serveMessage(AsyncWebServerRequest*,uint16_t,String,String,byte);


//turns all LEDs off and restarts ESP
void reset()
{
  briT = 0;
  long dly = millis();
  while(millis() - dly < 250)
  {
    yield(); //enough time to send response to client
  }
  setAllLeds();
  DEBUG_PRINTLN("MODULE RESET");
  ESP.restart();
}


//append new c string to temp buffer efficiently
bool oappend(char* txt)
{
  uint16_t len = strlen(txt);
  if (olen + len >= OMAX) return false; //buffer full
  strcpy(obuf + olen, txt);
  olen += len;
  return true;
}


//append new number to temp buffer efficiently
bool oappendi(int i)
{
  char s[11]; 
  sprintf(s,"%ld", i);
  return oappend(s);
}


//boot starts here
void setup() {
 // pinMode(4, OUTPUT); digitalWrite(4, LOW);
 // pinMode(16, OUTPUT); digitalWrite(16, LOW);
  wledInit();
}


//main program loop
void loop() {
  handleSerial();
  handleNotifications();
  handleTransitions();
  userLoop();
  
  yield();
  handleIO();
  handleIR();
  handleNetworkTime();
  if (!onlyAP) handleAlexa();
  
  handleOverlays();

  yield();
  if (doReboot) reset();
  
  if (!realtimeActive) //block stuff if WARLS/Adalight is enabled
  {
    if (dnsActive) dnsServer.processNextRequest();
    #ifndef WLED_DISABLE_OTA
     if (aOtaEnabled) ArduinoOTA.handle();
    #endif
    handleNightlight();
    yield();
    if (!onlyAP) {
      handleHue();
      handleBlynk();
      yield();
      if (millis() > nextMQTTReconnectAttempt)
      {
        yield();
        initMqtt();
        nextMQTTReconnectAttempt = millis() + 30000;
      }
    }
    yield();
    if (!offMode) strip.service();
  }
  
  //DEBUG serial logging
  #ifdef WLED_DEBUG
   if (millis() - debugTime > 9999)
   {
     DEBUG_PRINTLN("---DEBUG INFO---");
     DEBUG_PRINT("Runtime: "); DEBUG_PRINTLN(millis());
     DEBUG_PRINT("Unix time: "); DEBUG_PRINTLN(now());
     DEBUG_PRINT("Free heap: "); DEBUG_PRINTLN(ESP.getFreeHeap());
     DEBUG_PRINT("Wifi state: "); DEBUG_PRINTLN(WiFi.status());
     if (WiFi.status() != lastWifiState)
     {
       wifiStateChangedTime = millis();
     }
     lastWifiState = WiFi.status();
     DEBUG_PRINT("State time: "); DEBUG_PRINTLN(wifiStateChangedTime);
     DEBUG_PRINT("NTP last sync: "); DEBUG_PRINTLN(ntpLastSyncTime);
     DEBUG_PRINT("Client IP: "); DEBUG_PRINTLN(WiFi.localIP());
     debugTime = millis(); 
   }
  #endif
}
void Restore_Solid(){
  String apireq = "win&[FX=00] Solid";
  handleSet(nullptr, apireq);
}
#line 1 "/home/topota/Arduino/WLED/wled00/wled01_eeprom.ino"
/*
 * Methods to handle saving and loading to non-volatile memory
 * EEPROM Map: https://github.com/Aircoookie/WLED/wiki/EEPROM-Map
 */

#define EEPSIZE 2560

//eeprom Version code, enables default settings instead of 0 init on update
#define EEPVER 10
//0 -> old version, default
//1 -> 0.4p 1711272 and up
//2 -> 0.4p 1711302 and up
//3 -> 0.4  1712121 and up
//4 -> 0.5.0 and up
//5 -> 0.5.1 and up
//6 -> 0.6.0 and up
//7 -> 0.7.1 and up
//8 -> 0.8.0-a and up
//9 -> 0.8.0
//10-> 0.8.2


/*
 * Erase all configuration data
 */
void clearEEPROM()
{
  for (int i = 0; i < EEPSIZE; i++)
  {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}


void writeStringToEEPROM(uint16_t pos, char* str, uint16_t len)
{
  for (int i = 0; i < len; ++i)
  {
    EEPROM.write(pos + i, str[i]);
    if (str[i] == 0) return;
  }
}


void readStringFromEEPROM(uint16_t pos, char* str, uint16_t len)
{
  for (int i = 0; i < len; ++i)
  {
    str[i] = EEPROM.read(pos + i);
    if (str[i] == 0) return;
  }
  str[len] = 0; //make sure every string is properly terminated. str must be at least len +1 big.
}


/*
 * Write configuration to flash
 */
void saveSettingsToEEPROM()
{
  if (EEPROM.read(233) != 233) //set no first boot flag
  {
    clearEEPROM();
    EEPROM.write(233, 233);
  }
  
  writeStringToEEPROM(  0, clientSSID, 32);
  writeStringToEEPROM( 32, clientPass, 64);
  writeStringToEEPROM( 96,      cmDNS, 32);
  writeStringToEEPROM(128,     apSSID, 32);
  writeStringToEEPROM(160,     apPass, 64);

  EEPROM.write(224, nightlightDelayMinsDefault);
  EEPROM.write(225, nightlightFade);
  EEPROM.write(226, notifyDirectDefault);
  EEPROM.write(227, apChannel);
  EEPROM.write(228, apHide);
  EEPROM.write(229, ledCount & 0xFF);
  EEPROM.write(230, notifyButton);
  EEPROM.write(231, notifyTwice);
  EEPROM.write(232, buttonEnabled);
  //233 reserved for first boot flag
  
  for (int i = 0; i<4; i++) //ip addresses
  {
    EEPROM.write(234+i, staticIP[i]);
    EEPROM.write(238+i, staticGateway[i]);
    EEPROM.write(242+i, staticSubnet[i]);
  }
  
  EEPROM.write(246, colS[0]);
  EEPROM.write(247, colS[1]);
  EEPROM.write(248, colS[2]);
  EEPROM.write(249, briS);
  
  EEPROM.write(250, receiveNotificationBrightness);
  EEPROM.write(251, fadeTransition);
  EEPROM.write(252, strip.reverseMode);
  EEPROM.write(253, transitionDelayDefault & 0xFF);
  EEPROM.write(254, (transitionDelayDefault >> 8) & 0xFF);
  EEPROM.write(255, briMultiplier);
  
  //255,250,231,230,226 notifier bytes
  writeStringToEEPROM(256, otaPass, 32);
  
  EEPROM.write(288, nightlightTargetBri);
  EEPROM.write(289, otaLock);
  EEPROM.write(290, udpPort & 0xFF);
  EEPROM.write(291, (udpPort >> 8) & 0xFF);
  writeStringToEEPROM(292, serverDescription, 32);
  
  EEPROM.write(324, effectDefault);
  EEPROM.write(325, effectSpeedDefault);
  EEPROM.write(326, effectIntensityDefault);
  
  EEPROM.write(327, ntpEnabled);
  EEPROM.write(328, currentTimezone);
  EEPROM.write(329, useAMPM);
  EEPROM.write(330, strip.gammaCorrectBri);
  EEPROM.write(331, strip.gammaCorrectCol);
  EEPROM.write(332, overlayDefault);
  
  EEPROM.write(333, alexaEnabled);
  writeStringToEEPROM(334, alexaInvocationName, 32);
  EEPROM.write(366, notifyAlexa);
  
  EEPROM.write(367, (arlsOffset>=0));
  EEPROM.write(368, abs(arlsOffset));
  EEPROM.write(369, turnOnAtBoot);
  EEPROM.write(370, useHSBDefault);
  EEPROM.write(371, colS[3]); //white default
  EEPROM.write(372, useRGBW);
  EEPROM.write(373, effectPaletteDefault);
  EEPROM.write(374, strip.paletteFade);
  EEPROM.write(375, apWaitTimeSecs);
  EEPROM.write(376, recoveryAPDisabled);
  
  EEPROM.write(377, EEPVER); //eeprom was updated to latest
  
  EEPROM.write(378, colSecS[0]);
  EEPROM.write(379, colSecS[1]);
  EEPROM.write(380, colSecS[2]);
  EEPROM.write(381, colSecS[3]);
  EEPROM.write(382, strip.paletteBlend);
  EEPROM.write(383, strip.colorOrder);

  EEPROM.write(385, irEnabled);

  EEPROM.write(387, strip.ablMilliampsMax & 0xFF);
  EEPROM.write(388, (strip.ablMilliampsMax >> 8) & 0xFF);
  EEPROM.write(389, bootPreset);
  EEPROM.write(390, aOtaEnabled);
  EEPROM.write(391, receiveNotificationColor);
  EEPROM.write(392, receiveNotificationEffects);
  EEPROM.write(393, wifiLock);
  
  EEPROM.write(394, abs(utcOffsetSecs) & 0xFF);
  EEPROM.write(395, (abs(utcOffsetSecs) >> 8) & 0xFF);
  EEPROM.write(396, (utcOffsetSecs<0)); //is negative
  //397 was initLedsLast
  EEPROM.write(398, (ledCount >> 8) & 0xFF);
  EEPROM.write(399, !enableSecTransition);

  //favorite setting (preset) memory (25 slots/ each 20byte)
  //400 - 899 reserved

  for (int k=0;k<6;k++){
    int in = 900+k*8;
    writeStringToEEPROM(in, cssCol[k], 8);
  }

  EEPROM.write(948,currentTheme);
  writeStringToEEPROM(950, cssFont, 32);
  writeStringToEEPROM(982, MQTTuser, 14);
  writeStringToEEPROM(996, MQTTpass, 24);
  EEPROM.write(1020, mqttcredential);

  EEPROM.write(2048, huePollingEnabled);
  //EEPROM.write(2049, hueUpdatingEnabled);
  for (int i = 2050; i < 2054; ++i)
  {
    EEPROM.write(i, hueIP[i-2050]);
  }
  writeStringToEEPROM(2054, hueApiKey, 46);
  EEPROM.write(2100, huePollIntervalMs & 0xFF);
  EEPROM.write(2101, (huePollIntervalMs >> 8) & 0xFF);
  EEPROM.write(2102, notifyHue);
  EEPROM.write(2103, hueApplyOnOff);
  EEPROM.write(2104, hueApplyBri);
  EEPROM.write(2105, hueApplyColor);
  EEPROM.write(2106, huePollLightId);

  EEPROM.write(2150, overlayMin);
  EEPROM.write(2151, overlayMax);
  EEPROM.write(2152, analogClock12pixel);
  EEPROM.write(2153, analogClock5MinuteMarks);
  EEPROM.write(2154, analogClockSecondsTrail);
  
  EEPROM.write(2155, countdownMode);
  EEPROM.write(2156, countdownYear);
  EEPROM.write(2157, countdownMonth);
  EEPROM.write(2158, countdownDay);
  EEPROM.write(2159, countdownHour);
  EEPROM.write(2160, countdownMin);
  EEPROM.write(2161, countdownSec);
  setCountdown();

  writeStringToEEPROM(2165, cronixieDisplay, 6);
  EEPROM.write(2171, cronixieBacklight);
  setCronixie();
  
  EEPROM.write(2175, macroBoot);
  EEPROM.write(2176, macroAlexaOn);
  EEPROM.write(2177, macroAlexaOff);
  EEPROM.write(2178, macroButton);
  EEPROM.write(2179, macroLongPress);
  EEPROM.write(2180, macroCountdown);
  EEPROM.write(2181, macroNl);
  EEPROM.write(2182, macroDoublePress);

  EEPROM.write(2190, e131Universe & 0xFF);
  EEPROM.write(2191, (e131Universe >> 8) & 0xFF);
  EEPROM.write(2192, e131Multicast);
  EEPROM.write(2193, realtimeTimeoutMs & 0xFF);
  EEPROM.write(2194, (realtimeTimeoutMs >> 8) & 0xFF);
  EEPROM.write(2195, arlsForceMaxBri);
  EEPROM.write(2196, arlsDisableGammaCorrection);

  EEPROM.write(2200, !receiveDirect);
  EEPROM.write(2201, notifyMacro); //was enableRealtime
  EEPROM.write(2202, uiConfiguration);
  EEPROM.write(2203, autoRGBtoRGBW);
  EEPROM.write(2204, skipFirstLed);

  if (saveCurrPresetCycConf)
  {
    EEPROM.write(2205, presetCyclingEnabled);
    EEPROM.write(2206, presetCycleTime & 0xFF);
    EEPROM.write(2207, (presetCycleTime >> 8) & 0xFF);
    EEPROM.write(2208, presetCycleMin);
    EEPROM.write(2209, presetCycleMax);
    EEPROM.write(2210, presetApplyBri);
    EEPROM.write(2211, presetApplyCol);
    EEPROM.write(2212, presetApplyFx);
    saveCurrPresetCycConf = false;
  }

  writeStringToEEPROM(2220, blynkApiKey, 35);

  for (int i = 0; i < 8; ++i)
  {
    EEPROM.write(2260 + i, timerHours[i]  );
    EEPROM.write(2270 + i, timerMinutes[i]);
    EEPROM.write(2280 + i, timerWeekday[i]);
    EEPROM.write(2290 + i, timerMacro[i]  );
  }

  writeStringToEEPROM(2300,      mqttServer, 32);
  writeStringToEEPROM(2333, mqttDeviceTopic, 32);
  writeStringToEEPROM(2366,  mqttGroupTopic, 32);
  
  EEPROM.commit();
}


/*
 * Read all configuration from flash
 */
void loadSettingsFromEEPROM(bool first)
{
  if (EEPROM.read(233) != 233) //first boot/reset to default
  {
    DEBUG_PRINT("Settings invalid, restoring defaults...");
    saveSettingsToEEPROM();
    DEBUG_PRINTLN("done");
    return;
  }
  int lastEEPROMversion = EEPROM.read(377); //last EEPROM version before update
  

  readStringFromEEPROM(  0, clientSSID, 32);
  readStringFromEEPROM( 32, clientPass, 64);
  readStringFromEEPROM( 96,      cmDNS, 32);
  readStringFromEEPROM(128,     apSSID, 32);
  readStringFromEEPROM(160,     apPass, 64);

  //MQTT ADDED
  readStringFromEEPROM(982, MQTTuser, 14);
  readStringFromEEPROM(996, MQTTpass, 24);
  mqttcredential = EEPROM.read(1020);

  nightlightDelayMinsDefault = EEPROM.read(224);
  nightlightDelayMins = nightlightDelayMinsDefault;
  nightlightFade = EEPROM.read(225);
  notifyDirectDefault = EEPROM.read(226);
  notifyDirect = notifyDirectDefault;
  
  apChannel = EEPROM.read(227);
  if (apChannel > 13 || apChannel < 1) apChannel = 1;
  apHide = EEPROM.read(228);
  if (apHide > 1) apHide = 1;
  ledCount = EEPROM.read(229) + ((EEPROM.read(398) << 8) & 0xFF00); if (ledCount > 1200 || ledCount == 0) ledCount = 30;
  
  notifyButton = EEPROM.read(230);
  notifyTwice = EEPROM.read(231);
  buttonEnabled = EEPROM.read(232);
  
  staticIP[0] = EEPROM.read(234);
  staticIP[1] = EEPROM.read(235);
  staticIP[2] = EEPROM.read(236);
  staticIP[3] = EEPROM.read(237);
  staticGateway[0] = EEPROM.read(238);
  staticGateway[1] = EEPROM.read(239);
  staticGateway[2] = EEPROM.read(240);
  staticGateway[3] = EEPROM.read(241);
  staticSubnet[0] = EEPROM.read(242);
  staticSubnet[1] = EEPROM.read(243);
  staticSubnet[2] = EEPROM.read(244);
  staticSubnet[3] = EEPROM.read(245);
  
  colS[0] = EEPROM.read(246); col[0] = colS[0];
  colS[1] = EEPROM.read(247); col[1] = colS[1];
  colS[2] = EEPROM.read(248); col[2] = colS[2];
  briS = EEPROM.read(249); bri = briS;
  if (!EEPROM.read(369) && first)
  {
    bri = 0; briLast = briS;
  }
  receiveNotificationBrightness = EEPROM.read(250);
  fadeTransition = EEPROM.read(251);
  strip.reverseMode = EEPROM.read(252);
  transitionDelayDefault = EEPROM.read(253) + ((EEPROM.read(254) << 8) & 0xFF00);
  transitionDelay = transitionDelayDefault;
  briMultiplier = EEPROM.read(255);

  readStringFromEEPROM(256, otaPass, 32);
  
  nightlightTargetBri = EEPROM.read(288);
  otaLock = EEPROM.read(289);
  udpPort = EEPROM.read(290) + ((EEPROM.read(291) << 8) & 0xFF00);

  readStringFromEEPROM(292, serverDescription, 32);
  
  effectDefault = EEPROM.read(324); effectCurrent = effectDefault;
  effectSpeedDefault = EEPROM.read(325); effectSpeed = effectSpeedDefault;
  ntpEnabled = EEPROM.read(327);
  currentTimezone = EEPROM.read(328);
  useAMPM = EEPROM.read(329);
  strip.gammaCorrectBri = EEPROM.read(330);
  strip.gammaCorrectCol = EEPROM.read(331);
  overlayDefault = EEPROM.read(332);
  if (lastEEPROMversion < 8 && overlayDefault > 0) overlayDefault--; //overlay mode 1 (solid) was removed
  
  alexaEnabled = EEPROM.read(333);

  readStringFromEEPROM(334, alexaInvocationName, 32);
  
  notifyAlexa = EEPROM.read(366);
  arlsOffset = EEPROM.read(368);
  if (!EEPROM.read(367)) arlsOffset = -arlsOffset;
  turnOnAtBoot = EEPROM.read(369);
  useHSBDefault = EEPROM.read(370);
  colS[3] = EEPROM.read(371); col[3] = colS[3];
  useRGBW = EEPROM.read(372);
  effectPaletteDefault = EEPROM.read(373); effectPalette = effectPaletteDefault;
  //374 - strip.paletteFade

  if (lastEEPROMversion > 0) { 
    apWaitTimeSecs = EEPROM.read(375);
    recoveryAPDisabled = EEPROM.read(376);
  }
  //377 = lastEEPROMversion
  if (lastEEPROMversion > 1) {
    for (byte i=0; i<4; i++)
    {
      colSecS[i] = EEPROM.read(378+i); colSec[i] = colSecS[i];
    }
  }
  if (lastEEPROMversion > 3) {
    effectIntensityDefault = EEPROM.read(326); effectIntensity = effectIntensityDefault; 
    aOtaEnabled = EEPROM.read(390);
    receiveNotificationColor = EEPROM.read(391);
    receiveNotificationEffects = EEPROM.read(392);

    readStringFromEEPROM(950, cssFont, 32);
  } else //keep receiving notification behavior from pre0.5.0 after update
  {
    receiveNotificationColor = receiveNotificationBrightness;
    receiveNotificationEffects = receiveNotificationBrightness;
  }
  receiveNotifications = (receiveNotificationBrightness || receiveNotificationColor || receiveNotificationEffects);
  if (lastEEPROMversion > 4) {
    huePollingEnabled = EEPROM.read(2048);
    //hueUpdatingEnabled = EEPROM.read(2049);
    for (int i = 2050; i < 2054; ++i)
    {
      hueIP[i-2050] = EEPROM.read(i);
    }

    readStringFromEEPROM(2054, hueApiKey, 46);
    
    huePollIntervalMs = EEPROM.read(2100) + ((EEPROM.read(2101) << 8) & 0xFF00);
    notifyHue = EEPROM.read(2102);
    hueApplyOnOff = EEPROM.read(2103);
    hueApplyBri = EEPROM.read(2104);
    hueApplyColor = EEPROM.read(2105);
    huePollLightId = EEPROM.read(2106);
  }
  if (lastEEPROMversion > 5) {
    overlayMin = EEPROM.read(2150);
    overlayMax = EEPROM.read(2151);
    analogClock12pixel = EEPROM.read(2152);
    analogClock5MinuteMarks = EEPROM.read(2153);
    analogClockSecondsTrail = EEPROM.read(2154);
    countdownMode = EEPROM.read(2155);
    countdownYear = EEPROM.read(2156);
    countdownMonth = EEPROM.read(2157);
    countdownDay = EEPROM.read(2158);
    countdownHour = EEPROM.read(2159);
    countdownMin = EEPROM.read(2160);
    countdownSec = EEPROM.read(2161);
    setCountdown();

    readStringFromEEPROM(2165, cronixieDisplay, 6);
    cronixieBacklight = EEPROM.read(2171);
    
    macroBoot = EEPROM.read(2175);
    macroAlexaOn = EEPROM.read(2176);
    macroAlexaOff = EEPROM.read(2177);
    macroButton = EEPROM.read(2178);
    macroLongPress = EEPROM.read(2179);
    macroCountdown = EEPROM.read(2180);
    macroNl = EEPROM.read(2181);
    macroDoublePress = EEPROM.read(2182);
    if (macroDoublePress > 16) macroDoublePress = 0;
  }

  if (lastEEPROMversion > 6)
  {
    e131Universe = EEPROM.read(2190) + ((EEPROM.read(2191) << 8) & 0xFF00);
    e131Multicast = EEPROM.read(2192);
    realtimeTimeoutMs = EEPROM.read(2193) + ((EEPROM.read(2194) << 8) & 0xFF00);
    arlsForceMaxBri = EEPROM.read(2195);
    arlsDisableGammaCorrection = EEPROM.read(2196);
  }

  if (lastEEPROMversion > 7)
  {
    strip.paletteFade  = EEPROM.read(374);
    strip.paletteBlend = EEPROM.read(382);

    for (int i = 0; i < 8; ++i)
    {
      timerHours[i]   = EEPROM.read(2260 + i);
      timerMinutes[i] = EEPROM.read(2270 + i);
      timerWeekday[i] = EEPROM.read(2280 + i);
      timerMacro[i]   = EEPROM.read(2290 + i);
      if (timerWeekday[i] == 0) timerWeekday[i] = 255;
    }
  }

  if (lastEEPROMversion > 8)
  {
    readStringFromEEPROM(2300,      mqttServer, 32);
    readStringFromEEPROM(2333, mqttDeviceTopic, 32);
    readStringFromEEPROM(2366,  mqttGroupTopic, 32);
  }

  if (lastEEPROMversion > 9)
  {
    strip.colorOrder = EEPROM.read(383);
    irEnabled = EEPROM.read(385);
    strip.ablMilliampsMax = EEPROM.read(387) + ((EEPROM.read(388) << 8) & 0xFF00);
  } else if (lastEEPROMversion > 1) //ABL is off by default when updating from version older than 0.8.2
  {
    strip.ablMilliampsMax = 65000;
  } else {
    strip.ablMilliampsMax = ABL_MILLIAMPS_DEFAULT;
  }
  
  receiveDirect = !EEPROM.read(2200);
  notifyMacro = EEPROM.read(2201);
  uiConfiguration = EEPROM.read(2202);
  
  #ifdef WLED_DISABLE_MOBILE_UI
  uiConfiguration = 1;
  //force default UI since mobile is unavailable
  #endif
  
  autoRGBtoRGBW = EEPROM.read(2203);
  skipFirstLed = EEPROM.read(2204);

  if (EEPROM.read(2210) || EEPROM.read(2211) || EEPROM.read(2212))
  {
    presetCyclingEnabled = EEPROM.read(2205);
    presetCycleTime = EEPROM.read(2206) + ((EEPROM.read(2207) << 8) & 0xFF00);
    presetCycleMin = EEPROM.read(2208);
    presetCycleMax = EEPROM.read(2209);
    presetApplyBri = EEPROM.read(2210);
    presetApplyCol = EEPROM.read(2211);
    presetApplyFx = EEPROM.read(2212);
  }
  
  bootPreset = EEPROM.read(389);
  wifiLock = EEPROM.read(393);
  utcOffsetSecs = EEPROM.read(394) + ((EEPROM.read(395) << 8) & 0xFF00);
  if (EEPROM.read(396)) utcOffsetSecs = -utcOffsetSecs; //negative
  enableSecTransition = !EEPROM.read(399);

  //favorite setting (preset) memory (25 slots/ each 20byte)
  //400 - 899 reserved

  currentTheme = EEPROM.read(948);
  for (int k=0;k<6;k++){
    int in=900+k*8;
    readStringFromEEPROM(in, cssCol[k], 8);
  }

  //custom macro memory (16 slots/ each 64byte)
  //1024-2047 reserved

  readStringFromEEPROM(2220, blynkApiKey, 35);
  
  //user MOD memory
  //2944 - 3071 reserved
  
  useHSB = useHSBDefault;

  overlayCurrent = overlayDefault;
}


//PRESET PROTOCOL 20 bytes
//0: preset purpose byte 0:invalid 1:valid preset 1.0
//1:a 2:r 3:g 4:b 5:w 6:er 7:eg 8:eb 9:ew 10:fx 11:sx | custom chase 12:numP 13:numS 14:(0:fs 1:both 2:fe) 15:step 16:ix 17: fp 18-19:Zeros

bool applyPreset(byte index, bool loadBri = true, bool loadCol = true, bool loadFX = true)
{
  if (index == 255 || index == 0)
  {
    loadSettingsFromEEPROM(false);//load boot defaults
    return true;
  }
  if (index > 25 || index < 1) return false;
  uint16_t i = 380 + index*20;
  if (EEPROM.read(i) == 0) return false;
  if (loadBri) bri = EEPROM.read(i+1);
  if (loadCol)
  {
    for (byte j=0; j<4; j++)
    {
      col[j] = EEPROM.read(i+j+2);
      colSec[j] = EEPROM.read(i+j+6);
    }
  }
  if (loadFX)
  {
    effectCurrent = EEPROM.read(i+10);
    effectSpeed = EEPROM.read(i+11);
    effectIntensity = EEPROM.read(i+16);
    effectPalette = EEPROM.read(i+17);
  }
  return true;
}

void savePreset(byte index)
{
  if (index > 25) return;
  if (index < 1) {saveSettingsToEEPROM();return;}
  uint16_t i = 380 + index*20;//min400
  EEPROM.write(i, 1);
  EEPROM.write(i+1, bri);
  for (uint16_t j=0; j<4; j++)
  {
    EEPROM.write(i+j+2, col[j]);
    EEPROM.write(i+j+6, colSec[j]);
  }
  EEPROM.write(i+10, effectCurrent);
  EEPROM.write(i+11, effectSpeed);
  
  EEPROM.write(i+16, effectIntensity);
  EEPROM.write(i+17, effectPalette);
  EEPROM.commit();
}


void loadMacro(byte index, char* m)
{
  index-=1;
  if (index > 15) return;
  readStringFromEEPROM(1024+64*index, m, 64);
}


void applyMacro(byte index)
{
  index-=1;
  if (index > 15) return;
  String mc="win&";
  char m[65];
  loadMacro(index+1, m);
  mc += m;
  mc += "&IN"; //internal, no XML response
  if (!notifyMacro) mc += "&NN";
  String forbidden = "&M="; //dont apply if called by the macro itself to prevent loop
  /*
   * NOTE: loop is still possible if you call a different macro from a macro, which then calls the first macro again. 
   * To prevent that, but also disable calling macros within macros, comment the next line out.
   */
  forbidden = forbidden + index;
  if (mc.indexOf(forbidden) >= 0) return;
  handleSet(nullptr, mc);
}


void saveMacro(byte index, String mc, bool sing=true) //only commit on single save, not in settings
{
  index-=1;
  if (index > 15) return;
  int s = 1024+index*64;
  for (int i = s; i < s+64; i++)
  {
    EEPROM.write(i, mc.charAt(i-s));
  }
  if (sing) EEPROM.commit();
}
#line 1 "/home/topota/Arduino/WLED/wled00/wled02_xml.ino"
/*
 * Sending XML status files to client
 */

//build XML response to HTTP /win API request
char* XML_response(AsyncWebServerRequest *request, bool includeTheme, char* dest = nullptr)
{
  char sbuf[(dest == nullptr)?1024:1]; //allocate local buffer if none passed
  obuf = (dest == nullptr)? sbuf:dest;
  
  olen = 0;
  oappend("<?xml version=\"1.0\" ?><vs><ac>");
  oappendi((nightlightActive && nightlightFade) ? briT : bri);
  oappend("</ac>");
  
  for (int i = 0; i < 3; i++)
  {
   oappend("<cl>");
   oappendi(col[i]);
   oappend("</cl>");
  }
  for (int i = 0; i < 3; i++)
  {
   oappend("<cs>");
   oappendi(colSec[i]);
   oappend("</cs>");
  }
  oappend("<ns>");
  oappendi(notifyDirect);
  oappend("</ns><nr>");
  oappendi(receiveNotifications);
  oappend("</nr><nl>");
  oappendi(nightlightActive);
  oappend("</nl><nf>");
  oappendi(nightlightFade);
  oappend("</nf><nd>");
  oappendi(nightlightDelayMins);
  oappend("</nd><nt>");
  oappendi(nightlightTargetBri);
  oappend("</nt><fx>");
  oappendi(effectCurrent);
  oappend("</fx><sx>");
  oappendi(effectSpeed);
  oappend("</sx><ix>");
  oappendi(effectIntensity);
  oappend("</ix><fp>");
  oappendi(effectPalette);
  oappend("</fp><wv>");
  if (useRGBW && !autoRGBtoRGBW) {
   oappendi(col[3]);
  } else {
   oappend("-1");
  }
  oappend("</wv><ws>");
  oappendi(colSec[3]);
  oappend("</ws><md>");
  oappendi(useHSB);
  oappend("</md><cy>");
  oappendi(presetCyclingEnabled);
  oappend("</cy><ds>");
  if (realtimeActive)
  {
    String mesg = "Live ";
    if (realtimeIP[0] == 0)
    {
      mesg += "E1.31 mode";
    } else {
      mesg += "UDP from ";
      mesg += realtimeIP[0];
      for (int i = 1; i < 4; i++)
      {
        mesg += ".";
        mesg += realtimeIP[i];
      }
    }
    oappend((char*)mesg.c_str());
  } else {
    oappend(serverDescription);
  }
  
  oappend("</ds>");
  if (includeTheme)
  {
    char cs[6][9];
    getThemeColors(cs);
    oappend("<th><ca>#");
    oappend(cs[0]);
    oappend("</ca><cb>#");
    oappend(cs[1]);
    oappend("</cb><cc>#");
    oappend(cs[2]);
    oappend("</cc><cd>#");
    oappend(cs[3]);
    oappend("</cd><cu>#");
    oappend(cs[4]);
    oappend("</cu><ct>#");
    oappend(cs[5]);
    oappend("</ct><cf>");
    oappend(cssFont);
    oappend("</cf></th>");
  }
  oappend("</vs>");
  if (request != nullptr) request->send(200, "text/xml", obuf);
}

//append a numeric setting to string buffer
void sappend(char stype, char* key, int val)
{
  char ds[] = "d.Sf.";
  
  switch(stype)
  {
    case 'c': //checkbox
      oappend(ds);
      oappend(key);
      oappend(".checked=");
      oappendi(val);
      oappend(";");
      break;
    case 'v': //numeric
      oappend(ds);
      oappend(key);
      oappend(".value=");
      oappendi(val);
      oappend(";");
      break;
    case 'i': //selectedIndex
      oappend(ds);
      oappend(key);
      oappend(".selectedIndex=");
      oappendi(val);
      oappend(";");
      break;
  }
}

//append a string setting to buffer
void sappends(char stype, char* key, char* val)
{
  switch(stype)
  {
    case 's': //string (we can interpret val as char*)
      oappend("d.Sf.");
      oappend(key);
      oappend(".value=\"");
      oappend(val);
      oappend("\";");
      break;
    case 'm': //message
      oappend("d.getElementsByClassName");
      oappend(key);
      oappend(".innerHTML=\"");
      oappend(val);
      oappend("\";");
      break;
  }
}


//get values for settings form in javascript
void getSettingsJS(byte subPage, char* dest)
{
  //0: menu 1: wifi 2: leds 3: ui 4: sync 5: time 6: sec
  DEBUG_PRINT("settings resp");
  DEBUG_PRINTLN(subPage);
  obuf = dest;
  olen = 0;
  
  if (subPage <1 || subPage >6) return;

  if (subPage == 1) {
    sappends('s',"CS",clientSSID);

    byte l = strlen(clientPass);
    char fpass[l+1]; //fill password field with ***
    fpass[l] = 0;
    memset(fpass,'*',l); 
    sappends('s',"CP",fpass);

    char k[3]; k[2] = 0; //IP addresses
    for (int i = 0; i<4; i++)
    {
      k[1] = 48+i; //ascii 0,1,2,3
      k[0] = 'I'; sappend('v',k,staticIP[i]);
      k[0] = 'G'; sappend('v',k,staticGateway[i]);
      k[0] = 'S'; sappend('v',k,staticSubnet[i]);
    }

    sappends('s',"CM",cmDNS);
    sappend('v',"AT",apWaitTimeSecs);
    sappends('s',"AS",apSSID);
    sappend('c',"AH",apHide);
    
    l = strlen(apPass);
    char fapass[l+1]; //fill password field with ***
    fapass[l] = 0;
    memset(fapass,'*',l); 
    sappends('s',"AP",fapass);
    
    sappend('v',"AC",apChannel);

    if (WiFi.localIP()[0] != 0) //is connected
    {
      char s[16];
      IPAddress localIP = WiFi.localIP();
      sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
      sappends('m',"(\"sip\")[0]",s);
    } else
    {
      sappends('m',"(\"sip\")[0]","Not connected");
    }
    
    if (WiFi.softAPIP()[0] != 0) //is active
    {
      char s[16];
      IPAddress apIP = WiFi.softAPIP();
      sprintf(s, "%d.%d.%d.%d", apIP[0], apIP[1], apIP[2], apIP[3]);
      sappends('m',"(\"sip\")[1]",s);
    } else
    {
      sappends('m',"(\"sip\")[1]","Not active");
    }
  }
  
  if (subPage == 2) {
    sappend('v',"LC",ledCount);
    sappend('v',"MA",strip.ablMilliampsMax);
    if (strip.currentMilliamps)
    {
      sappends('m',"(\"pow\")[0]","");
      olen -= 2; //delete ";
      oappendi(strip.currentMilliamps);
      oappend("mA\";");
    }
    sappend('v',"CR",colS[0]);
    sappend('v',"CG",colS[1]);
    sappend('v',"CB",colS[2]);
    sappend('v',"CA",briS);
    sappend('c',"EW",useRGBW);
    sappend('i',"CO",strip.colorOrder);
    sappend('c',"AW",autoRGBtoRGBW);
    sappend('v',"CW",colS[3]);
    sappend('v',"SR",colSecS[0]);
    sappend('v',"SG",colSecS[1]);
    sappend('v',"SB",colSecS[2]);
    sappend('v',"SW",colSecS[3]);
    sappend('c',"BO",turnOnAtBoot);
    sappend('v',"BP",bootPreset);
    oappend("f=");
    oappendi(effectDefault);
    oappend(";p=");
    oappendi(effectPaletteDefault);
    oappend(";");
    sappend('v',"SX",effectSpeedDefault);
    sappend('v',"IX",effectIntensityDefault);
    sappend('c',"GB",strip.gammaCorrectBri);
    sappend('c',"GC",strip.gammaCorrectCol);
    sappend('c',"TF",fadeTransition);
    sappend('v',"TD",transitionDelay);
    sappend('c',"PF",strip.paletteFade);
    sappend('c',"T2",enableSecTransition);
    sappend('v',"BF",briMultiplier);
    sappend('v',"TB",nightlightTargetBri);
    sappend('v',"TL",nightlightDelayMinsDefault);
    sappend('c',"TW",nightlightFade);
    sappend('i',"PB",strip.paletteBlend);
    sappend('c',"RV",strip.reverseMode);
    sappend('c',"SL",skipFirstLed);
  }

  if (subPage == 3)
  { 
    sappend('i',"UI",uiConfiguration);
    sappends('s',"DS",serverDescription);
    sappend('c',"MD",useHSBDefault);
    sappend('i',"TH",currentTheme);
    char k[3]; k[0] = 'C'; k[2] = 0; //keys
    for (int i=0; i<6; i++)
    {
      k[1] = 48+i; //ascii 0,1,2,3,4,5
      sappends('s',k,cssCol[i]);
    }
    sappends('s',"CF",cssFont);
  }

  if (subPage == 4)
  {
    sappend('c',"BT",buttonEnabled);
    sappend('c',"IR",irEnabled);
    sappend('v',"UP",udpPort);
    sappend('c',"RB",receiveNotificationBrightness);
    sappend('c',"RC",receiveNotificationColor);
    sappend('c',"RX",receiveNotificationEffects);
    sappend('c',"SD",notifyDirectDefault);
    sappend('c',"SB",notifyButton);
    sappend('c',"SH",notifyHue);
    sappend('c',"SM",notifyMacro);
    sappend('c',"S2",notifyTwice);
    sappend('c',"RD",receiveDirect);
    sappend('c',"EM",e131Multicast);
    sappend('v',"EU",e131Universe);
    sappend('v',"ET",realtimeTimeoutMs);
    sappend('c',"FB",arlsForceMaxBri);
    sappend('c',"RG",arlsDisableGammaCorrection);
    sappend('v',"WO",arlsOffset);
    sappend('c',"AL",alexaEnabled);
    sappends('s',"AI",alexaInvocationName);
    sappend('c',"SA",notifyAlexa);
    sappends('s',"BK",(char*)((blynkEnabled)?"Hidden":""));
    sappends('s',"MS",mqttServer);
    sappends('s',"MD",mqttDeviceTopic);
    sappends('s',"MG",mqttGroupTopic);
    sappends('s',"MUs",MQTTuser);
    sappends('s',"MPs",MQTTpass);
    sappend('c',"MCr",mqttcredential);
    sappend('v',"H0",hueIP[0]);
    sappend('v',"H1",hueIP[1]);
    sappend('v',"H2",hueIP[2]);
    sappend('v',"H3",hueIP[3]);
    sappend('v',"HL",huePollLightId);
    sappend('v',"HI",huePollIntervalMs);
    sappend('c',"HP",huePollingEnabled);
    sappend('c',"HO",hueApplyOnOff);
    sappend('c',"HB",hueApplyBri);
    sappend('c',"HC",hueApplyColor);
    sappends('m',"(\"hms\")[0]",hueError);
  }

  if (subPage == 5)
  {
    sappend('c',"NT",ntpEnabled);
    sappend('c',"CF",!useAMPM);
    sappend('i',"TZ",currentTimezone);
    sappend('v',"UO",utcOffsetSecs);
    char tm[32];
    getTimeString(tm); 
    sappends('m',"(\"times\")[0]",tm);
    sappend('i',"OL",overlayCurrent);
    sappend('v',"O1",overlayMin);
    sappend('v',"O2",overlayMax);
    sappend('v',"OM",analogClock12pixel);
    sappend('c',"OS",analogClockSecondsTrail);
    sappend('c',"O5",analogClock5MinuteMarks);
    sappends('s',"CX",cronixieDisplay);
    sappend('c',"CB",cronixieBacklight);
    sappend('c',"CE",countdownMode);
    sappend('v',"CY",countdownYear);
    sappend('v',"CI",countdownMonth);
    sappend('v',"CD",countdownDay);
    sappend('v',"CH",countdownHour);
    sappend('v',"CM",countdownMin);
    sappend('v',"CS",countdownSec);
    char k[4]; k[0]= 'M';
    for (int i=1;i<17;i++)
    {
      char m[65];
      loadMacro(i, m);
      sprintf(k+1,"%i",i);
      sappends('s',k,m);
    }
    
    sappend('v',"MB",macroBoot);
    sappend('v',"A0",macroAlexaOn);
    sappend('v',"A1",macroAlexaOff);
    sappend('v',"MP",macroButton);
    sappend('v',"ML",macroLongPress);
    sappend('v',"MC",macroCountdown);
    sappend('v',"MN",macroNl);
    sappend('v',"MD",macroDoublePress);

    k[2] = 0; //Time macros
    for (int i = 0; i<8; i++)
    {
      k[1] = 48+i; //ascii 0,1,2,3
      k[0] = 'H'; sappend('v',k,timerHours[i]);
      k[0] = 'N'; sappend('v',k,timerMinutes[i]);
      k[0] = 'T'; sappend('v',k,timerMacro[i]);
      k[0] = 'W'; sappend('v',k,timerWeekday[i]);
    }
  }

  if (subPage == 6)
  {
    sappend('c',"NO",otaLock);
    sappend('c',"OW",wifiLock);
    sappend('c',"AO",aOtaEnabled);
    sappend('c',"NA",recoveryAPDisabled);
    sappends('m',"(\"msg\")[0]","WLED ");
    olen -= 2; //delete ";
    oappend(versionString);
    oappend(" (build ");
    oappendi(VERSION);
    oappend(") OK\";");
  }
  oappend("}</script>");
}


//get colors from current theme as c strings
void getThemeColors(char o[][9])
{
  switch (currentTheme)
  {
    //       accent color (aCol)     background (bCol)       panel (cCol)            controls (dCol)         shadows (sCol)          text (tCol)    
    default: strcpy(o[0], "D9B310"); strcpy(o[1], "0B3C5D"); strcpy(o[2], "1D2731"); strcpy(o[3], "328CC1"); strcpy(o[4], "000");    strcpy(o[5], "328CC1"); break; //night
    case 1:  strcpy(o[0], "eee");    strcpy(o[1], "ddd");    strcpy(o[2], "b9b9b9"); strcpy(o[3], "049");    strcpy(o[4], "777");    strcpy(o[5], "049");    break; //modern
    case 2:  strcpy(o[0], "abb");    strcpy(o[1], "fff");    strcpy(o[2], "ddd");    strcpy(o[3], "000");    strcpy(o[4], "0004");   strcpy(o[5], "000");    break; //bright
    case 3:  strcpy(o[0], "c09f80"); strcpy(o[1], "d7cec7"); strcpy(o[2], "76323f"); strcpy(o[3], "888");    strcpy(o[4], "3334");   strcpy(o[5], "888");    break; //wine
    case 4:  strcpy(o[0], "3cc47c"); strcpy(o[1], "828081"); strcpy(o[2], "d9a803"); strcpy(o[3], "1e392a"); strcpy(o[4], "000a");   strcpy(o[5], "1e392a"); break; //electric
    case 5:  strcpy(o[0], "57bc90"); strcpy(o[1], "a5a5af"); strcpy(o[2], "015249"); strcpy(o[3], "88c9d4"); strcpy(o[4], "0004");   strcpy(o[5], "88c9d4"); break; //mint
    case 6:  strcpy(o[0], "f7c331"); strcpy(o[1], "dca");    strcpy(o[2], "6b7a8f"); strcpy(o[3], "f7882f"); strcpy(o[4], "0007");   strcpy(o[5], "f7882f"); break; //amber
    case 7:  strcpy(o[0], "fff");    strcpy(o[1], "333");    strcpy(o[2], "222");    strcpy(o[3], "666");    strcpy(o[4], "");       strcpy(o[5], "fff");    break; //dark
    case 8:  strcpy(o[0], "0ac");    strcpy(o[1], "124");    strcpy(o[2], "224");    strcpy(o[3], "003eff"); strcpy(o[4], "003eff"); strcpy(o[5], "003eff"); break; //air
    case 9:  strcpy(o[0], "f70");    strcpy(o[1], "421");    strcpy(o[2], "221");    strcpy(o[3], "a50");    strcpy(o[4], "f70");    strcpy(o[5], "f70");    break; //nixie
    case 10: strcpy(o[0], "2d2");    strcpy(o[1], "010");    strcpy(o[2], "121");    strcpy(o[3], "060");    strcpy(o[4], "040");    strcpy(o[5], "3f3");    break; //terminal
    case 11: strcpy(o[0], "867ADE"); strcpy(o[1], "4033A3"); strcpy(o[2], "483AAA"); strcpy(o[3], "483AAA"); strcpy(o[4], "");       strcpy(o[5], "867ADE"); break; //c64
    case 12: strcpy(o[0], "fbe8a6"); strcpy(o[1], "d2fdff"); strcpy(o[2], "b4dfe5"); strcpy(o[3], "f4976c"); strcpy(o[4], "");       strcpy(o[5], "303c6c"); break; //easter
    case 13: strcpy(o[0], "d4af37"); strcpy(o[1], "173305"); strcpy(o[2], "308505"); strcpy(o[3], "f21313"); strcpy(o[4], "f002");   strcpy(o[5], "d4af37"); break; //christmas
    case 14: strcpy(o[0], "fc7");    strcpy(o[1], "49274a"); strcpy(o[2], "94618e"); strcpy(o[3], "f4decb"); strcpy(o[4], "0008");   strcpy(o[5], "f4decb"); break; //end
    case 15: for (int i=0;i<6;i++) strcpy(o[i], cssCol[i]); //custom
  }
}

#line 1 "/home/topota/Arduino/WLED/wled00/wled03_set.ino"
/*
 * Receives client input
 */

void _setRandomColor(bool _sec,bool fromButton=false)
{
  lastRandomIndex = strip.get_random_wheel_index(lastRandomIndex);
  if (_sec){
    colorHStoRGB(lastRandomIndex*256,255,colSec);
  } else {
    colorHStoRGB(lastRandomIndex*256,255,col);
  }
  if (fromButton) colorUpdated(2);
}


//called upon POST settings form submit
void handleSettingsSet(AsyncWebServerRequest *request, byte subPage)
{
  //0: menu 1: wifi 2: leds 3: ui 4: sync 5: time 6: sec
  if (subPage <1 || subPage >6) return;
  
  //WIFI SETTINGS
  if (subPage == 1)
  {
    strcpy(clientSSID,request->arg("CS").c_str());
    if (request->arg("CP").charAt(0) != '*') strcpy(clientPass, request->arg("CP").c_str());

    strcpy(cmDNS, request->arg("CM").c_str());
    
    int t = request->arg("AT").toInt(); if (t > 9 && t <= 255) apWaitTimeSecs = t;
    strcpy(apSSID, request->arg("AS").c_str());
    apHide = request->hasArg("AH");
    int passlen = request->arg("AP").length();
    if (passlen == 0 || (passlen > 7 && request->arg("AP").charAt(0) != '*')) strcpy(apPass, request->arg("AP").c_str());
    t = request->arg("AC").toInt(); if (t > 0 && t < 14) apChannel = t;
    
    char k[3]; k[2] = 0;
    for (int i = 0; i<4; i++)
    {
      k[1] = i+48;//ascii 0,1,2,3
      
      k[0] = 'I'; //static IP
      staticIP[i] = request->arg(k).toInt();
      
      k[0] = 'G'; //gateway
      staticGateway[i] = request->arg(k).toInt();
      
      k[0] = 'S'; //subnet
      staticSubnet[i] = request->arg(k).toInt();
    }
  }

  //LED SETTINGS
  if (subPage == 2)
  {
    int t = request->arg("LC").toInt();
    if (t > 0 && t <= 1200) ledCount = t;
    #ifndef ARDUINO_ARCH_ESP32
    #if LEDPIN == 3
    if (ledCount > 300) ledCount = 300; //DMA method uses too much ram
    #endif
    #endif
    strip.ablMilliampsMax = request->arg("MA").toInt();
    useRGBW = request->hasArg("EW");
    strip.colorOrder = request->arg("CO").toInt(); 
    autoRGBtoRGBW = request->hasArg("AW");

    //ignore settings and save current brightness, colors and fx as default
    if (request->hasArg("IS"))
    {
      for (byte i=0; i<4; i++)
      {
        colS[i] = col[i];
        colSecS[i] = colSec[i];
      }
      briS = bri;
      effectDefault = effectCurrent;
      effectSpeedDefault = effectSpeed;
      effectIntensityDefault = effectIntensity;
      effectPaletteDefault = effectPalette;
    } else {
      colS[0] = request->arg("CR").toInt();
      colS[1] = request->arg("CG").toInt();
      colS[2] = request->arg("CB").toInt();
      colSecS[0] = request->arg("SR").toInt();
      colSecS[1] = request->arg("SG").toInt();
      colSecS[2] = request->arg("SB").toInt();
      colS[3] = request->arg("CW").toInt();
      colSecS[3] = request->arg("SW").toInt();
      briS = request->arg("CA").toInt();
      effectDefault = request->arg("FX").toInt();
      effectSpeedDefault = request->arg("SX").toInt();
      effectIntensityDefault = request->arg("IX").toInt();
      effectPaletteDefault = request->arg("FP").toInt();
    }
    saveCurrPresetCycConf = request->hasArg("PC");
    turnOnAtBoot = request->hasArg("BO");
    t = request->arg("BP").toInt();
    if (t <= 25) bootPreset = t;
    strip.gammaCorrectBri = request->hasArg("GB");
    strip.gammaCorrectCol = request->hasArg("GC");
    
    fadeTransition = request->hasArg("TF");
    t = request->arg("TD").toInt();
    if (t > 0) transitionDelay = t;
    transitionDelayDefault = t;
    strip.paletteFade = request->hasArg("PF");
    enableSecTransition = request->hasArg("T2");
    
    nightlightTargetBri = request->arg("TB").toInt();
    t = request->arg("TL").toInt();
    if (t > 0) nightlightDelayMinsDefault = t;
    nightlightFade = request->hasArg("TW");
    
    t = request->arg("PB").toInt();
    if (t >= 0 && t < 4) strip.paletteBlend = t;
    strip.reverseMode = request->hasArg("RV");
    skipFirstLed = request->hasArg("SL");
    t = request->arg("BF").toInt();
    if (t > 0) briMultiplier = t;
  }

  //UI
  if (subPage == 3)
  {
    int t = request->arg("UI").toInt();
    if (t >= 0 && t < 3) uiConfiguration = t;
    strcpy(serverDescription, request->arg("DS").c_str());
    useHSBDefault = request->hasArg("MD");
    useHSB = useHSBDefault;
    currentTheme = request->arg("TH").toInt();
    char k[3]; k[0]='C'; k[2]=0;
    for(int i=0;i<6;i++)
    {
      k[1] = i+48;
      strcpy(cssCol[i],request->arg(k).c_str());
    }
    strcpy(cssFont,request->arg("CF").c_str());
  }

  //SYNC
  if (subPage == 4)
  {
    buttonEnabled = request->hasArg("BT");
    irEnabled = request->hasArg("IR");
    int t = request->arg("UP").toInt();
    if (t > 0) udpPort = t;
    receiveNotificationBrightness = request->hasArg("RB");
    receiveNotificationColor = request->hasArg("RC");
    receiveNotificationEffects = request->hasArg("RX");
    receiveNotifications = (receiveNotificationBrightness || receiveNotificationColor || receiveNotificationEffects);
    notifyDirectDefault = request->hasArg("SD");
    notifyDirect = notifyDirectDefault;
    notifyButton = request->hasArg("SB");
    notifyAlexa = request->hasArg("SA");
    notifyHue = request->hasArg("SH");
    notifyMacro = request->hasArg("SM");
    notifyTwice = request->hasArg("S2");
    
    receiveDirect = request->hasArg("RD");
    e131Multicast = request->hasArg("EM");
    t = request->arg("EU").toInt();
    if (t > 0  && t <= 63999) e131Universe = t;
    t = request->arg("ET").toInt();
    if (t > 99  && t <= 65000) realtimeTimeoutMs = t;
    arlsForceMaxBri = request->hasArg("FB");
    arlsDisableGammaCorrection = request->hasArg("RG");
    t = request->arg("WO").toInt();
    if (t >= -255  && t <= 255) arlsOffset = t;
    
    alexaEnabled = request->hasArg("AL");
    strcpy(alexaInvocationName, request->arg("AI").c_str());
    
    if (request->hasArg("BK") && !request->arg("BK").equals("Hidden")) {
      strcpy(blynkApiKey,request->arg("BK").c_str()); initBlynk(blynkApiKey);
    }

    strcpy(mqttServer, request->arg("MS").c_str());
    strcpy(MQTTuser, request->arg("MUs").c_str());
    strcpy(MQTTpass, request->arg("MPs").c_str());
    strcpy(mqttDeviceTopic, request->arg("MD").c_str());
    strcpy(mqttGroupTopic, request->arg("MG").c_str());
    mqttcredential = request->hasArg("MCr");


    for (int i=0;i<4;i++){
      String a = "H"+String(i);
      hueIP[i] = request->arg(a).toInt();
    }

    t = request->arg("HL").toInt();
    if (t > 0) huePollLightId = t;

    t = request->arg("HI").toInt();
    if (t > 50) huePollIntervalMs = t;

    hueApplyOnOff = request->hasArg("HO");
    hueApplyBri = request->hasArg("HB");
    hueApplyColor = request->hasArg("HC");
    huePollingEnabled = request->hasArg("HP");
    hueStoreAllowed = true;
    reconnectHue();
  }

  //TIME
  if (subPage == 5)
  {
    ntpEnabled = request->hasArg("NT");
    useAMPM = !request->hasArg("CF");
    currentTimezone = request->arg("TZ").toInt();
    utcOffsetSecs = request->arg("UO").toInt();

    //start ntp if not already connected
    if (ntpEnabled && WiFi.status() == WL_CONNECTED && !ntpConnected) ntpConnected = ntpUdp.begin(ntpLocalPort);
    
    if (request->hasArg("OL")){
      overlayDefault = request->arg("OL").toInt();
      if (overlayCurrent != overlayDefault) strip.unlockAll();
      overlayCurrent = overlayDefault;
    }
    
    overlayMin = request->arg("O1").toInt();
    overlayMax = request->arg("O2").toInt();
    analogClock12pixel = request->arg("OM").toInt();
    analogClock5MinuteMarks = request->hasArg("O5");
    analogClockSecondsTrail = request->hasArg("OS");
    
    strcpy(cronixieDisplay,request->arg("CX").c_str());
    bool cbOld = cronixieBacklight;
    cronixieBacklight = request->hasArg("CB");
    if (cbOld != cronixieBacklight && overlayCurrent == 3)
    {
      strip.setCronixieBacklight(cronixieBacklight); overlayRefreshedTime = 0;
    }
    countdownMode = request->hasArg("CE");
    countdownYear = request->arg("CY").toInt();
    countdownMonth = request->arg("CI").toInt();
    countdownDay = request->arg("CD").toInt();
    countdownHour = request->arg("CH").toInt();
    countdownMin = request->arg("CM").toInt();
    countdownSec = request->arg("CS").toInt();
    
    for (int i=1;i<17;i++)
    {
      String a = "M"+String(i);
      if (request->hasArg(a.c_str())) saveMacro(i,request->arg(a),false);
    }
    
    macroBoot = request->arg("MB").toInt();
    macroAlexaOn = request->arg("A0").toInt();
    macroAlexaOff = request->arg("A1").toInt();
    macroButton = request->arg("MP").toInt();
    macroLongPress = request->arg("ML").toInt();
    macroCountdown = request->arg("MC").toInt();
    macroNl = request->arg("MN").toInt();
    macroDoublePress = request->arg("MD").toInt();

    char k[3]; k[2] = 0;
    for (int i = 0; i<8; i++)
    {
      k[1] = i+48;//ascii 0,1,2,3
      
      k[0] = 'H'; //timer hours
      timerHours[i] = request->arg(k).toInt();
      
      k[0] = 'N'; //minutes
      timerMinutes[i] = request->arg(k).toInt();
      
      k[0] = 'T'; //macros
      timerMacro[i] = request->arg(k).toInt();

      k[0] = 'W'; //weekdays
      timerWeekday[i] = request->arg(k).toInt();
    }
  }

  //SECURITY
  if (subPage == 6)
  {
    if (request->hasArg("RS")) //complete factory reset
    {
      clearEEPROM();
      serveMessage(request, 200, "All Settings erased.", "Connect to WLED-AP to setup again",255);
      doReboot = true;
    }

    bool pwdCorrect = !otaLock; //always allow access if ota not locked
    if (request->hasArg("OP"))
    {
      if (otaLock && strcmp(otaPass,request->arg("OP").c_str()) == 0)
      {
        pwdCorrect = true;
      }
      if (!otaLock && request->arg("OP").length() > 0)
      {
        strcpy(otaPass,request->arg("OP").c_str());
      }
    }
    
    if (pwdCorrect) //allow changes if correct pwd or no ota active
    {
      otaLock = request->hasArg("NO");
      wifiLock = request->hasArg("OW");
      recoveryAPDisabled = request->hasArg("NA");
      aOtaEnabled = request->hasArg("AO");
    }
  }
  if (subPage != 6 || !doReboot) saveSettingsToEEPROM(); //do not save if factory reset
  if (subPage == 2) strip.init(useRGBW,ledCount,skipFirstLed);
  if (subPage == 4) alexaInit();
}



//helper to get int value at a position in string
int getNumVal(const String* req, uint16_t pos)
{
  return req->substring(pos+3).toInt();
}


//helper to get int value at a position in string
bool updateVal(const String* req, const char* key, byte* val, byte minv=0, byte maxv=255)
{
  int pos = req->indexOf(key);
  if (pos < 1) return false;
  
  if (req->charAt(pos+3) == '~') {
    int out = getNumVal(req, pos+1);
    if (out == 0)
    {
      if (req->charAt(pos+4) == '-')
      {
        *val = (*val <= minv)? maxv : *val -1;
      } else {
        *val = (*val >= maxv)? minv : *val +1;
      }
    } else {
      out += *val;
      if (out > maxv) out = maxv;
      if (out < minv) out = minv;
      *val = out;
    }
  } else
  {
    *val = getNumVal(req, pos);
  }
  return true;
}


//HTTP API request parser
bool handleSet(AsyncWebServerRequest *request, const String& req)
{
  if (!(req.indexOf("win") >= 0)) return false;

  int pos = 0;
  DEBUG_PRINT("API req: ");
  DEBUG_PRINTLN(req);
  
  //save macro, requires &MS=<slot>(<macro>) format
  pos = req.indexOf("&MS=");
  if (pos > 0) {
    int i = req.substring(pos + 4).toInt();
    pos = req.indexOf('(') +1;
    if (pos > 0) { 
      int en = req.indexOf(')');
      String mc = req.substring(pos);
      if (en > 0) mc = req.substring(pos, en);
      saveMacro(i, mc); 
    }
    
    pos = req.indexOf("IN");
    if (pos < 1) XML_response(request, false);
    return true;
    //if you save a macro in one request, other commands in that request are ignored due to unwanted behavior otherwise
  }
   
  //set brightness
  updateVal(&req, "&A=", &bri);

  //set colors
  updateVal(&req, "&R=", &col[0]);
  updateVal(&req, "&G=", &col[1]);
  updateVal(&req, "&B=", &col[2]);
  updateVal(&req, "&W=", &col[3]);
  updateVal(&req, "R2=", &colSec[0]);
  updateVal(&req, "G2=", &colSec[1]);
  updateVal(&req, "B2=", &colSec[2]);
  updateVal(&req, "W2=", &colSec[3]);

  //set hue
  pos = req.indexOf("HU=");
  if (pos > 0) {
    uint16_t temphue = getNumVal(&req, pos);
    byte tempsat = 255;
    pos = req.indexOf("SA=");
    if (pos > 0) {
      tempsat = getNumVal(&req, pos);
    }
    colorHStoRGB(temphue,tempsat,(req.indexOf("H2")>0)? colSec:col);
  }
   
  //set color from HEX or 32bit DEC
  pos = req.indexOf("CL=");
  if (pos > 0) {
    colorFromDecOrHexString(col, (char*)req.substring(pos + 3).c_str());
  }
  pos = req.indexOf("C2=");
  if (pos > 0) {
    colorFromDecOrHexString(colSec, (char*)req.substring(pos + 3).c_str());
  }
   
  //set 2nd to white
  pos = req.indexOf("SW");
  if (pos > 0) {
    if(useRGBW) {
      colSec[3] = 255;
      colSec[0] = 0;
      colSec[1] = 0;
      colSec[2] = 0;
    } else {
      colSec[0] = 255;
      colSec[1] = 255;
      colSec[2] = 255;
    }
  }
   
  //set 2nd to black
  pos = req.indexOf("SB");
  if (pos > 0) {
    colSec[3] = 0;
    colSec[0] = 0;
    colSec[1] = 0;
    colSec[2] = 0;
  }
   
  //set to random hue SR=0->1st SR=1->2nd
  pos = req.indexOf("SR");
  if (pos > 0) {
    _setRandomColor(getNumVal(&req, pos));
  }
  
  //set 2nd to 1st
  pos = req.indexOf("SP");
  if (pos > 0) {
    colSec[0] = col[0];
    colSec[1] = col[1];
    colSec[2] = col[2];
    colSec[3] = col[3];
  }
  
  //swap 2nd & 1st
  pos = req.indexOf("SC");
  if (pos > 0) {
    byte temp;
    for (uint8_t i=0; i<4; i++)
    {
      temp = col[i];
      col[i] = colSec[i];
      colSec[i] = temp;
    }
  }
   
  //set effect parameters
  if (updateVal(&req, "FX=", &effectCurrent, 0, strip.getModeCount()-1)) presetCyclingEnabled = false;
  updateVal(&req, "SX=", &effectSpeed);
  updateVal(&req, "IX=", &effectIntensity);
  updateVal(&req, "FP=", &effectPalette, 0, strip.getPaletteCount()-1);

  //set hue polling light: 0 -off
  #ifndef WLED_DISABLE_HUESYNC
  pos = req.indexOf("HP=");
  if (pos > 0) {
    int id = getNumVal(&req, pos);
    if (id > 0)
    {
      if (id < 100) huePollLightId = id;
      reconnectHue();
    } else {
      huePollingEnabled = false;
    }
  }
  #endif
   
  //set default control mode (0 - RGB, 1 - HSB)
  pos = req.indexOf("MD=");
  if (pos > 0) {
    useHSB = getNumVal(&req, pos);
  }
  
  //set advanced overlay
  pos = req.indexOf("OL=");
  if (pos > 0) {
    overlayCurrent = getNumVal(&req, pos);
    strip.unlockAll();
  }
  
  //(un)lock pixel (ranges)
  pos = req.indexOf("&L=");
  if (pos > 0) {
    uint16_t index = getNumVal(&req, pos);
    pos = req.indexOf("L2=");
    bool unlock = req.indexOf("UL") > 0;
    if (pos > 0) {
      uint16_t index2 = getNumVal(&req, pos);
      if (unlock) {
        strip.unlockRange(index, index2);
      } else {
        strip.lockRange(index, index2);
      }
    } else {
      if (unlock) {
        strip.unlock(index);
      } else {
        strip.lock(index);
      }
    }
  }

  //apply macro
  pos = req.indexOf("&M=");
  if (pos > 0) {
    applyMacro(getNumVal(&req, pos));
  }
  
  //toggle send UDP direct notifications
  pos = req.indexOf("SN=");
  if (pos > 0) notifyDirect = (req.charAt(pos+3) != '0');
   
  //toggle receive UDP direct notifications
  pos = req.indexOf("RN=");
  if (pos > 0) receiveNotifications = (req.charAt(pos+3) != '0');

  //receive live data via UDP/Hyperion
  pos = req.indexOf("RD=");
  if (pos > 0) receiveDirect = (req.charAt(pos+3) != '0');
   
  //toggle nightlight mode
  bool aNlDef = false;
  if (req.indexOf("&ND") > 0) aNlDef = true;
  pos = req.indexOf("NL=");
  if (pos > 0)
  {
    if (req.charAt(pos+3) == '0')
    {
      nightlightActive = false;
      bri = briT;
    } else {
      nightlightActive = true;
      if (!aNlDef) nightlightDelayMins = getNumVal(&req, pos);
      nightlightStartTime = millis();
    }
  } else if (aNlDef)
  {
    nightlightActive = true;
    nightlightStartTime = millis();
  }
   
  //set nightlight target brightness
  pos = req.indexOf("NT=");
  if (pos > 0) {
    nightlightTargetBri = getNumVal(&req, pos);
    nightlightActiveOld = false; //re-init
  }
   
  //toggle nightlight fade
  pos = req.indexOf("NF=");
  if (pos > 0)
  {
    nightlightFade = (req.charAt(pos+3) != '0');
    nightlightActiveOld = false; //re-init
  }

  #if AUXPIN >= 0
  //toggle general purpose output
  pos = req.indexOf("AX=");
  if (pos > 0) {
    auxTime = getNumVal(&req, pos);
    auxActive = true;
    if (auxTime == 0) auxActive = false;
  }
  #endif
  
  pos = req.indexOf("TT=");
  if (pos > 0) transitionDelay = getNumVal(&req, pos);

  //main toggle on/off
  pos = req.indexOf("&T=");
  if (pos > 0) {
    nightlightActive = false; //always disable nightlight when toggling
    switch (getNumVal(&req, pos))
    {
      case 0: if (bri != 0){briLast = bri; bri = 0;} break; //off
      case 1: bri = briLast; break; //on
      default: toggleOnOff(); //toggle
    }
  }

  //Segment reverse
  pos = req.indexOf("RV=");
  if (pos > 0) strip.getSegment(0).setOption(1, req.charAt(pos+3) != '0');
   
  //deactivate nightlight if target brightness is reached
  if (bri == nightlightTargetBri) nightlightActive = false;
  //set time (unix timestamp)
  pos = req.indexOf("ST=");
  if (pos > 0) {
    setTime(getNumVal(&req, pos));
  }
   
  //set countdown goal (unix timestamp)
  pos = req.indexOf("CT=");
  if (pos > 0) {
    countdownTime = getNumVal(&req, pos);
    if (countdownTime - now() > 0) countdownOverTriggered = false;
  }
   
  //set presets
  pos = req.indexOf("P1="); //sets first preset for cycle
  if (pos > 0) presetCycleMin = getNumVal(&req, pos);
  
  pos = req.indexOf("P2="); //sets last preset for cycle
  if (pos > 0) presetCycleMax = getNumVal(&req, pos);

  //preset cycle
  pos = req.indexOf("CY=");
  if (pos > 0)
  {
    presetCyclingEnabled = (req.charAt(pos+3) != '0');
    presetCycCurr = presetCycleMin;
  }
  
  pos = req.indexOf("PT="); //sets cycle time in ms
  if (pos > 0) {
    int v = getNumVal(&req, pos);
    if (v > 49) presetCycleTime = v;
  }

  pos = req.indexOf("PA="); //apply brightness from preset
  if (pos > 0) presetApplyBri = (req.charAt(pos+3) != '0');

  pos = req.indexOf("PC="); //apply color from preset
  if (pos > 0) presetApplyCol = (req.charAt(pos+3) != '0'); 

  pos = req.indexOf("PX="); //apply effects from preset
  if (pos > 0) presetApplyFx = (req.charAt(pos+3) != '0');
  
  pos = req.indexOf("PS="); //saves current in preset
  if (pos > 0) savePreset(getNumVal(&req, pos));

  //apply preset
  if (updateVal(&req, "PL=", &presetCycCurr, presetCycleMin, presetCycleMax)) {
    applyPreset(presetCycCurr, presetApplyBri, presetApplyCol, presetApplyFx);
  }
  
  //cronixie
  #ifndef WLED_DISABLE_CRONIXIE
  pos = req.indexOf("NX="); //sets digits to code
  if (pos > 0) {
    strcpy(cronixieDisplay,req.substring(pos + 3, pos + 9).c_str());
    setCronixie();
  }
  
  if (req.indexOf("NB=") > 0) //sets backlight
  {
    cronixieBacklight = true;
    if (req.indexOf("NB=0") > 0)
    {
      cronixieBacklight = false;
    }
    if (overlayCurrent == 3) strip.setCronixieBacklight(cronixieBacklight);
    overlayRefreshedTime = 0;
  }
  #endif
  //mode, 1 countdown
  pos = req.indexOf("NM=");
  if (pos > 0) countdownMode = (req.charAt(pos+3) != '0');
  
  pos = req.indexOf("U0="); //user var 0
  if (pos > 0) {
    userVar0 = getNumVal(&req, pos);
  }
  
  pos = req.indexOf("U1="); //user var 1
  if (pos > 0) {
    userVar1 = getNumVal(&req, pos);
  }
  //you can add more if you need
   
  //internal call, does not send XML response
  pos = req.indexOf("IN");
  if (pos < 1) XML_response(request, (req.indexOf("&IT") > 0)); //include theme if firstload
  
  pos = req.indexOf("&NN"); //do not send UDP notifications this time
  colorUpdated((pos > 0) ? 5:1);
  
  return true;
}

#line 1 "/home/topota/Arduino/WLED/wled00/wled04_file.ino"
/*
 * Utility for SPIFFS filesystem & Serial console
 */
void handleSerial()
{
  if (Serial.available() > 0) //support for Adalight protocol to high-speed control LEDs over serial
  {
    if (!Serial.find("Ada")) return;
     
    if (!realtimeActive && bri == 0) strip.setBrightness(briLast);
    arlsLock(realtimeTimeoutMs);
    
    yield();
    byte hi = Serial.read();
    byte ledc = Serial.read();
    byte chk = Serial.read();
    if(chk != (hi ^ ledc ^ 0x55)) return;
    if (ledCount < ledc) ledc = ledCount;
    
    byte sc[3]; int t =-1; int to = 0;
    for (int i=0; i < ledc; i++)
    {
      for (byte j=0; j<3; j++)
      {
        while (Serial.peek()<0) //no data yet available
        {
          yield();
          to++;
          if (to>15) {strip.show(); return;} //unexpected end of transmission
        }
        to = 0;
        sc[j] = Serial.read();
      }
      setRealtimePixel(i,sc[0],sc[1],sc[2],0);
    }
    strip.show();
  }
}


#if !defined WLED_DISABLE_FILESYSTEM && defined WLED_ENABLE_FS_SERVING
//Un-comment any file types you need
String getContentType(AsyncWebServerRequest* request, String filename){
  if(request->hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
//  else if(filename.endsWith(".css")) return "text/css";
//  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".json")) return "application/json";
  else if(filename.endsWith(".png")) return "image/png";
//  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
//  else if(filename.endsWith(".xml")) return "text/xml";
//  else if(filename.endsWith(".pdf")) return "application/x-pdf";
//  else if(filename.endsWith(".zip")) return "application/x-zip";
//  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(AsyncWebServerRequest* request, String path){
  DEBUG_PRINTLN("FileRead: " + path);
  if(path.endsWith("/")) path += "index.htm";
  String contentType = getContentType(request, path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz)){
    request->send(SPIFFS, pathWithGz, contentType);
    return true;
  }
  if(SPIFFS.exists(path)) {
    request->send(SPIFFS, path, contentType);
    return true;
  }
  return false;
}

#else
bool handleFileRead(AsyncWebServerRequest*, String path){return false;}
#endif

#line 1 "/home/topota/Arduino/WLED/wled00/wled05_init.ino"
/*
 * Setup code
 */

void wledInit()
{ 
  EEPROM.begin(EEPSIZE);
  ledCount = EEPROM.read(229) + ((EEPROM.read(398) << 8) & 0xFF00); 
  if (ledCount > 1200 || ledCount == 0) ledCount = 30;
  #ifndef ARDUINO_ARCH_ESP32
  #if LEDPIN == 3
  if (ledCount > 300) ledCount = 300; //DMA method uses too much ram
  #endif
  #endif
  Serial.begin(115200);
  Serial.setTimeout(50);
  DEBUG_PRINTLN();
  DEBUG_PRINT("---WLED "); DEBUG_PRINT(versionString); DEBUG_PRINT(" "); DEBUG_PRINT(VERSION); DEBUG_PRINTLN(" INIT---");
  #ifdef ARDUINO_ARCH_ESP32
  DEBUG_PRINT("esp32 ");   DEBUG_PRINTLN(ESP.getSdkVersion());
  #else
  DEBUG_PRINT("esp8266 "); DEBUG_PRINTLN(ESP.getCoreVersion());
  #endif
  int heapPreAlloc = ESP.getFreeHeap();
  DEBUG_PRINT("heap ");
  DEBUG_PRINTLN(ESP.getFreeHeap());
  
  strip.init(EEPROM.read(372),ledCount,EEPROM.read(2204)); //init LEDs quickly

  DEBUG_PRINT("LEDs inited. heap usage ~");
  DEBUG_PRINTLN(heapPreAlloc - ESP.getFreeHeap());

  #ifndef WLED_DISABLE_FILESYSTEM
   #ifdef ARDUINO_ARCH_ESP32
    SPIFFS.begin(true);
   #endif
    SPIFFS.begin();
  #endif
  
  DEBUG_PRINTLN("Load EEPROM");
  loadSettingsFromEEPROM(true);
  beginStrip();
  DEBUG_PRINT("CSSID: ");
  DEBUG_PRINT(clientSSID);
  userBeginPreConnection();
  if (strcmp(clientSSID,"Your_Network") == 0) showWelcomePage = true;
  WiFi.persistent(false);
  initCon();

  DEBUG_PRINTLN("");
  DEBUG_PRINT("Connected! IP address: ");
  DEBUG_PRINTLN(WiFi.localIP());

  if (hueIP[0] == 0)
  {
    hueIP[0] = WiFi.localIP()[0];
    hueIP[1] = WiFi.localIP()[1];
    hueIP[2] = WiFi.localIP()[2];
  }

  if (udpPort > 0 && udpPort != ntpLocalPort)
  {
    udpConnected = notifierUdp.begin(udpPort);
    if (udpConnected && udpRgbPort != udpPort) udpRgbConnected = rgbUdp.begin(udpRgbPort);
  }
  if (ntpEnabled && WiFi.status() == WL_CONNECTED)
  ntpConnected = ntpUdp.begin(ntpLocalPort);

  //start captive portal if AP active
  if (onlyAP || strlen(apSSID) > 0) 
  {
    dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
    dnsServer.start(53, "wled.me", WiFi.softAPIP());
    dnsActive = true;
  }

  prepareIds(); //UUID from MAC (for Alexa and MQTT)
  if (strcmp(cmDNS,"x") == 0) //fill in unique mdns default
  {
    strcpy(cmDNS, "wled-");
    strcat(cmDNS, escapedMac.c_str());
  }
  if (mqttDeviceTopic[0] == 0)
  {
    strcpy(mqttDeviceTopic, "wled/");
    strcat(mqttDeviceTopic, escapedMac.c_str());
  }
   
  strip.service();

  //HTTP server page init
  initServer();
  
  strip.service();
  //init Alexa hue emulation
  if (alexaEnabled && !onlyAP) alexaInit();

  server.begin();
  DEBUG_PRINTLN("HTTP server started");

  //init ArduinoOTA
  if (!onlyAP) {
    #ifndef WLED_DISABLE_OTA
    if (aOtaEnabled)
    {
      ArduinoOTA.onStart([]() {
        #ifndef ARDUINO_ARCH_ESP32
        wifi_set_sleep_type(NONE_SLEEP_T);
        #endif
        DEBUG_PRINTLN("Start ArduinoOTA");
      });
      if (strlen(cmDNS) > 0) ArduinoOTA.setHostname(cmDNS);
      ArduinoOTA.begin();
    }
    #endif
  
    strip.service();
    // Set up mDNS responder:
    if (strlen(cmDNS) > 0 && !onlyAP)
    {
      MDNS.begin(cmDNS);
      DEBUG_PRINTLN("mDNS responder started");
      // Add service to MDNS
      MDNS.addService("http", "tcp", 80);
      MDNS.addService("wled", "tcp", 80);
    }
    strip.service();

    initBlynk(blynkApiKey);
    initE131();
    reconnectHue();
  } else {
    e131Enabled = false;
  }

  userBegin();

  if (macroBoot>0) applyMacro(macroBoot);
  Serial.println("La Virgen");
}


void beginStrip()
{
  // Initialize NeoPixel Strip and button
  strip.setColor(0, 0);
  strip.setBrightness(255);

#ifdef BTNPIN
  pinMode(BTNPIN, INPUT_PULLUP);
#endif

  if (bootPreset>0) applyPreset(bootPreset, turnOnAtBoot, true, true);
  colorUpdated(0);

  //init relay pin
  #if RLYPIN >= 0
    pinMode(RLYPIN, OUTPUT);
    #if RLYMDE
      digitalWrite(RLYPIN, bri);
    #else
      digitalWrite(RLYPIN, !bri);
    #endif
  #endif

  //disable button if it is "pressed" unintentionally
#ifdef BTNPIN
  if(digitalRead(BTNPIN) == LOW) buttonEnabled = false;
#else
  buttonEnabled = false;
#endif
}


void initAP(){
  bool set = apSSID[0];
  if (!set) strcpy(apSSID,"WLED-AP");
  WiFi.softAP(apSSID, apPass, apChannel, apHide);
  if (!set) apSSID[0] = 0;
}


void initCon()
{
  WiFi.disconnect(); //close old connections

  if (staticIP[0] != 0)
  {
    WiFi.config(staticIP, staticGateway, staticSubnet, IPAddress(8,8,8,8));
  } else
  {
    WiFi.config(0U, 0U, 0U);
  }

  if (strlen(apSSID)>0)
  {
    DEBUG_PRINT(" USING AP");
    DEBUG_PRINTLN(strlen(apSSID));
    initAP();
  } else
  {
    DEBUG_PRINTLN(" NO AP");
    WiFi.softAPdisconnect(true);
  }
  int fail_count = 0;
  if (strlen(clientSSID) <1 || strcmp(clientSSID,"Your_Network") == 0)
    fail_count = apWaitTimeSecs*2; //instantly go to ap mode
  #ifndef ARDUINO_ARCH_ESP32
   WiFi.hostname(serverDescription);
  #endif
   WiFi.begin(clientSSID, clientPass);
  #ifdef ARDUINO_ARCH_ESP32
   WiFi.setHostname(serverDescription);
  #endif
  unsigned long lastTry = 0;
  bool con = false;
  while(!con)
  {
    yield();
    handleTransitions();
    handleButton();
    handleOverlays();
    if (briT) strip.service();
    if (millis()-lastTry > 499) {
      con = (WiFi.status() == WL_CONNECTED);
      lastTry = millis();
      DEBUG_PRINTLN("C_NC");
      if (!recoveryAPDisabled && fail_count > apWaitTimeSecs*2)
      {
        WiFi.disconnect();
        DEBUG_PRINTLN("Can't connect. Opening AP...");
        onlyAP = true;
        initAP();
        return;
      }
      fail_count++;
    }
  }
}


bool checkClientIsMobile(String useragent)
{
  //to save complexity this function is not comprehensive
  if (useragent.indexOf("Android") >= 0) return true;
  if (useragent.indexOf("iPhone") >= 0) return true;
  if (useragent.indexOf("iPod") >= 0) return true;
  return false;
}

#line 1 "/home/topota/Arduino/WLED/wled00/wled06_usermod.ino"
/*
 * This file allows you to add own functionality to WLED more easily
 * See: https://github.com/Aircoookie/WLED/wiki/Add-own-functionality
 * EEPROM bytes 2750+ are reserved for your custom use case. (if you extend #define EEPSIZE in wled01_eeprom.h)
 * bytes 2400+ are currently ununsed, but might be used for future wled features
 */

//Use userVar0 and userVar1 (API calls &U0=,&U1=, uint16_t)

void userBeginPreConnection()
{
  
}

void userBegin()
{

}

void userLoop()
{
  
}

#line 1 "/home/topota/Arduino/WLED/wled00/wled07_notify.ino"
/*
 * UDP notifier
 */

#define WLEDPACKETSIZE 24
#define UDP_IN_MAXSIZE 1472


void notify(byte callMode, bool followUp=false)
{
  if (!udpConnected) return;
  switch (callMode)
  {
    case 0: return;
    case 1: if (!notifyDirect) return; break;
    case 2: if (!notifyButton) return; break;
    case 4: if (!notifyDirect) return; break;
    case 6: if (!notifyDirect) return; break; //fx change
    case 7: if (!notifyHue)    return; break;
    case 8: if (!notifyDirect) return; break;
    case 9: if (!notifyDirect) return; break;
    case 10: if (!notifyAlexa) return; break;
    default: return;
  }
  byte udpOut[WLEDPACKETSIZE];
  udpOut[0] = 0; //0: wled notifier protocol 1: WARLS protocol
  udpOut[1] = callMode;
  udpOut[2] = bri;
  udpOut[3] = col[0];
  udpOut[4] = col[1];
  udpOut[5] = col[2];
  udpOut[6] = nightlightActive;
  udpOut[7] = nightlightDelayMins;
  udpOut[8] = effectCurrent;
  udpOut[9] = effectSpeed;
  udpOut[10] = col[3];
  //compatibilityVersionByte: 
  //0: old 1: supports white 2: supports secondary color
  //3: supports FX intensity, 24 byte packet 4: supports transitionDelay 5: sup palette
  //6: supports tertiary color
  udpOut[11] = 5; 
  udpOut[12] = colSec[0];
  udpOut[13] = colSec[1];
  udpOut[14] = colSec[2];
  udpOut[15] = colSec[3];
  udpOut[16] = effectIntensity;
  udpOut[17] = (transitionDelay >> 0) & 0xFF;
  udpOut[18] = (transitionDelay >> 8) & 0xFF;
  udpOut[19] = effectPalette;
  /*udpOut[20] = colTer[0];
  udpOut[21] = colTer[1];
  udpOut[22] = colTer[2];
  udpOut[23] = colTer[3];*/
  
  IPAddress broadcastIp;
  broadcastIp = ~uint32_t(WiFi.subnetMask()) | uint32_t(WiFi.gatewayIP());

  notifierUdp.beginPacket(broadcastIp, udpPort);
  notifierUdp.write(udpOut, WLEDPACKETSIZE);
  notifierUdp.endPacket();
  notificationSentCallMode = callMode;
  notificationSentTime = millis();
  notificationTwoRequired = (followUp)? false:notifyTwice;
}


void arlsLock(uint32_t timeoutMs)
{
  if (!realtimeActive){
    for (uint16_t i = 0; i < ledCount; i++)
    {
      strip.setPixelColor(i,0,0,0,0);
    }
    strip.unlockAll();
    realtimeActive = true;
  }
  realtimeTimeout = millis() + timeoutMs;
  if (timeoutMs == 255001 || timeoutMs == 65000) realtimeTimeout = UINT32_MAX;
  if (arlsForceMaxBri) strip.setBrightness(255);
}


void initE131(){
  if (WiFi.status() == WL_CONNECTED && e131Enabled)
  {
    e131 = new E131();
    e131->begin((e131Multicast) ? E131_MULTICAST : E131_UNICAST , e131Universe);
  } else {
    e131Enabled = false;
  }
}


void handleE131(){
  //E1.31 protocol support
  if(e131Enabled) {
    uint16_t len = e131->parsePacket();
    if (!len || e131->universe < e131Universe || e131->universe > e131Universe +4) return;
    len /= 3; //one LED is 3 DMX channels
    
    uint16_t multipacketOffset = (e131->universe - e131Universe)*170; //if more than 170 LEDs (510 channels), client will send in next higher universe 
    if (ledCount <= multipacketOffset) return;

    arlsLock(realtimeTimeoutMs);
    if (len + multipacketOffset > ledCount) len = ledCount - multipacketOffset;
    
    for (uint16_t i = 0; i < len; i++) {
      int j = i * 3;
      setRealtimePixel(i + multipacketOffset, e131->data[j], e131->data[j+1], e131->data[j+2], 0);
    }
    strip.show();
  }
}


void handleNotifications()
{
  //send second notification if enabled
  if(udpConnected && notificationTwoRequired && millis()-notificationSentTime > 250){
    notify(notificationSentCallMode,true);
  }

  handleE131();

  //unlock strip when realtime UDP times out
  if (realtimeActive && millis() > realtimeTimeout)
  {
    //strip.unlockAll();
    strip.setBrightness(bri);
    realtimeActive = false;
    //strip.setMode(effectCurrent);
    realtimeIP[0] = 0;
  }

  //receive UDP notifications
  if (!udpConnected || !(receiveNotifications || receiveDirect)) return;
    
  uint16_t packetSize = notifierUdp.parsePacket();

  //hyperion / raw RGB
  if (!packetSize && udpRgbConnected) {
    packetSize = rgbUdp.parsePacket();
    if (!receiveDirect) return;
    if (packetSize > UDP_IN_MAXSIZE || packetSize < 3) return;
    realtimeIP = rgbUdp.remoteIP();
    DEBUG_PRINTLN(rgbUdp.remoteIP());
    uint8_t lbuf[packetSize];
    rgbUdp.read(lbuf, packetSize);
    arlsLock(realtimeTimeoutMs);
    uint16_t id = 0;
    for (uint16_t i = 0; i < packetSize -2; i += 3)
    {
      setRealtimePixel(id, lbuf[i], lbuf[i+1], lbuf[i+2], 0);
      
      id++; if (id >= ledCount) break;
    }
    strip.show();
    return;
  }

  //notifier and UDP realtime
  if (packetSize > UDP_IN_MAXSIZE) return;
  if(packetSize && notifierUdp.remoteIP() != WiFi.localIP()) //don't process broadcasts we send ourselves
  {
    uint8_t udpIn[packetSize];
    notifierUdp.read(udpIn, packetSize);

    //wled notifier, block if realtime packets active
    if (udpIn[0] == 0 && !realtimeActive && receiveNotifications)
    {
      bool someSel = (receiveNotificationBrightness || receiveNotificationColor || receiveNotificationEffects);
      //apply colors from notification
      if (receiveNotificationColor || !someSel)
      {
        col[0] = udpIn[3];
        col[1] = udpIn[4];
        col[2] = udpIn[5];
        if (udpIn[11] > 0) //check if sending modules white val is inteded
        {
          col[3] = udpIn[10];
          if (udpIn[11] > 1)
          {
            colSec[0] = udpIn[12];
            colSec[1] = udpIn[13];
            colSec[2] = udpIn[14];
            colSec[3] = udpIn[15];
          }
          /*if (udpIn[11] > 5)
          {
            colTer[0] = udpIn[20];
            colTer[1] = udpIn[21];
            colTer[2] = udpIn[22];
            colSec[3] = udpIn[23];
          }*/
        }
      }

      //apply effects from notification
      if (udpIn[11] < 200 && (receiveNotificationEffects || !someSel))
      {
        if (udpIn[8] < strip.getModeCount()) effectCurrent = udpIn[8];
        effectSpeed   = udpIn[9];
        if (udpIn[11] > 2) effectIntensity = udpIn[16];
        if (udpIn[11] > 4 && udpIn[19] < strip.getPaletteCount()) effectPalette = udpIn[19];
      }
      
      if (udpIn[11] > 3)
      {
        transitionDelayTemp = ((udpIn[17] << 0) & 0xFF) + ((udpIn[18] << 8) & 0xFF00);
      }

      nightlightActive = udpIn[6];
      if (nightlightActive) nightlightDelayMins = udpIn[7];
      
      if (receiveNotificationBrightness || !someSel) bri = udpIn[2];
      colorUpdated(3);
      
    }  else if (udpIn[0] > 0 && udpIn[0] < 4 && receiveDirect) //1 warls //2 drgb //3 drgbw
    {
      realtimeIP = notifierUdp.remoteIP();
      DEBUG_PRINTLN(notifierUdp.remoteIP());
      if (packetSize > 1) {
        if (udpIn[1] == 0)
        {
          realtimeTimeout = 0;
          return;
        } else {
          arlsLock(udpIn[1]*1000 +1);
        }
        if (udpIn[0] == 1) //warls
        {
          for (uint16_t i = 2; i < packetSize -3; i += 4)
          {
            setRealtimePixel(udpIn[i], udpIn[i+1], udpIn[i+2], udpIn[i+3], 0);
          }
        } else if (udpIn[0] == 2) //drgb
        {
          uint16_t id = 0;
          for (uint16_t i = 2; i < packetSize -2; i += 3)
          {
            setRealtimePixel(id, udpIn[i], udpIn[i+1], udpIn[i+2], 0);

            id++; if (id >= ledCount) break;
          }
        } else if (udpIn[0] == 3) //drgbw
        {
          uint16_t id = 0;
          for (uint16_t i = 2; i < packetSize -3; i += 4)
          {
            setRealtimePixel(id, udpIn[i], udpIn[i+1], udpIn[i+2], udpIn[i+3]);
            
            id++; if (id >= ledCount) break;
          }
        } else if (udpIn[0] == 4) //dnrgb
        {
          uint16_t id = ((udpIn[3] << 0) & 0xFF) + ((udpIn[2] << 8) & 0xFF00);
          for (uint16_t i = 4; i < packetSize -2; i += 3)
          {
             if (id >= ledCount) break;
            setRealtimePixel(id, udpIn[i], udpIn[i+1], udpIn[i+2], udpIn[i+3]);
            id++;
          }
        }
        strip.show();
      }
    }
  }
}


void setRealtimePixel(uint16_t i, byte r, byte g, byte b, byte w)
{
  uint16_t pix = i + arlsOffset;
  if (pix < ledCount)
  {
    if (!arlsDisableGammaCorrection && strip.gammaCorrectCol)
    {
      strip.setPixelColor(pix, strip.gamma8(r), strip.gamma8(g), strip.gamma8(b), strip.gamma8(w));
    } else {
      strip.setPixelColor(pix, r, g, b, w);
    }
  }
}

#line 1 "/home/topota/Arduino/WLED/wled00/wled08_led.ino"
/*
 * LED methods
 */

void toggleOnOff()
{
  if (bri == 0)
  {
    bri = briLast;
  } else
  {
    briLast = bri;
    bri = 0;
  }
}


void setAllLeds() {
  if (!realtimeActive || !arlsForceMaxBri)
  {
    double d = briT*briMultiplier;
    int val = d/100;
    if (val > 255) val = 255;
    strip.setBrightness(val);
  }
  if (!enableSecTransition)
  {
    for (byte i = 0; i<4; i++)
    {
      colSecT[i] = colSec[i];
    }
  }
  if (useRGBW && autoRGBtoRGBW)
  {
    colorRGBtoRGBW(colT);
    colorRGBtoRGBW(colSecT);
  }
  strip.setColor(0, colT[0], colT[1], colT[2], colT[3]);
  strip.setColor(1, colSecT[0], colSecT[1], colSecT[2], colSecT[3]);
}


void setLedsStandard()
{
  for (byte i=0; i<4; i++)
  {
    colOld[i] = col[i];
    colT[i] = col[i];
    colSecOld[i] = colSec[i];
    colSecT[i] = colSec[i];
  }
  briOld = bri;
  briT = bri;
  setAllLeds();
}


bool colorChanged()
{
  for (byte i=0; i<4; i++)
  {
    if (col[i] != colIT[i]) return true;
    if (colSec[i] != colSecIT[i]) return true;
  }
  if (bri != briIT) return true;
  return false;
}


void colorUpdated(int callMode)
{
  //call for notifier -> 0: init 1: direct change 2: button 3: notification 4: nightlight 5: other (No notification)
  //                     6: fx changed 7: hue 8: preset cycle 9: blynk 10: alexa
  bool fxChanged = strip.setEffectConfig(effectCurrent, effectSpeed, effectIntensity, effectPalette);
  if (!colorChanged())
  {
    if (nightlightActive && !nightlightActiveOld && callMode != 3 && callMode != 5)
    {
      notify(4); interfaceUpdateCallMode = 4; return;
    }
    else if (fxChanged) {
      notify(6);
      if (callMode != 8) interfaceUpdateCallMode = 6;
      if (realtimeTimeout == UINT32_MAX) realtimeTimeout = 0;
    }
    return; //no change
  }
  if (realtimeTimeout == UINT32_MAX) realtimeTimeout = 0;
  if (callMode != 5 && nightlightActive && nightlightFade)
  {
    briNlT = bri;
    nightlightDelayMs -= (millis() - nightlightStartTime);
    nightlightStartTime = millis();
  }
  for (byte i=0; i<4; i++)
  {
    colIT[i] = col[i];
    colSecIT[i] = colSec[i];
  }
  briIT = bri;
  if (bri > 0) briLast = bri;
  
  notify(callMode);
  
  if (fadeTransition)
  {
    //set correct delay if not using notification delay
    if (callMode != 3) transitionDelayTemp = transitionDelay;
    if (transitionDelayTemp == 0) {setLedsStandard(); strip.trigger(); return;}
    
    if (transitionActive)
    {
      for (byte i=0; i<4; i++)
      {
        colOld[i] = colT[i];
        colSecOld[i] = colSecT[i];
      }
      briOld = briT;
      tperLast = 0;
    }
    strip.setTransitionMode(true);
    transitionActive = true;
    transitionStartTime = millis();
  } else
  {
    setLedsStandard();
    strip.trigger();
  }

  if (callMode == 8) return;
  //set flag to update blynk and mqtt
  interfaceUpdateCallMode = callMode;
}


void updateInterfaces(uint8_t callMode)
{
  #ifndef WLED_DISABLE_ALEXA
  if (espalexaDevice != nullptr && callMode != 10) {
    espalexaDevice->setValue(bri);
    espalexaDevice->setColor(col[0], col[1], col[2]);
  }
  #endif
  if (callMode != 9 && callMode != 5) updateBlynk();
  publishMqtt();
  lastInterfaceUpdate = millis();
}


void handleTransitions()
{
  //handle still pending interface update
  if (interfaceUpdateCallMode && millis() - lastInterfaceUpdate > 2000)
  {
    updateInterfaces(interfaceUpdateCallMode);
    interfaceUpdateCallMode = 0; //disable
  }
  
  if (transitionActive && transitionDelayTemp > 0)
  {
    float tper = (millis() - transitionStartTime)/(float)transitionDelayTemp;
    if (tper >= 1.0)
    {
      strip.setTransitionMode(false);
      transitionActive = false;
      tperLast = 0;
      setLedsStandard();
      return;
    }
    if (tper - tperLast < 0.004) return;
    tperLast = tper;
    for (byte i=0; i<4; i++)
    {
      colT[i] = colOld[i]+((col[i] - colOld[i])*tper);
      colSecT[i] = colSecOld[i]+((colSec[i] - colSecOld[i])*tper);
    }
    briT    = briOld   +((bri    - briOld   )*tper);
    
    setAllLeds();
  }
}


void handleNightlight()
{
  if (nightlightActive)
  {
    if (!nightlightActiveOld) //init
    {
      nightlightStartTime = millis();
      nightlightDelayMs = (int)(nightlightDelayMins*60000);
      nightlightActiveOld = true;
      briNlT = bri;
    }
    float nper = (millis() - nightlightStartTime)/((float)nightlightDelayMs);
    if (nightlightFade)
    {
      bri = briNlT + ((nightlightTargetBri - briNlT)*nper);
      colorUpdated(5);
    }
    if (nper >= 1)
    {
      nightlightActive = false;
      if (!nightlightFade)
      {
        bri = nightlightTargetBri;
        colorUpdated(5);
      }
      updateBlynk();
      if (bri == 0) briLast = briNlT;
    }
  } else if (nightlightActiveOld) //early de-init
  {
    nightlightActiveOld = false;
  }

  //also handle preset cycle here
  if (presetCyclingEnabled && (millis() - presetCycledTime > presetCycleTime))
  {
    applyPreset(presetCycCurr,presetApplyBri,presetApplyCol,presetApplyFx);
    presetCycCurr++; if (presetCycCurr > presetCycleMax) presetCycCurr = presetCycleMin;
    if (presetCycCurr > 25) presetCycCurr = 1;
    colorUpdated(8);
    presetCycledTime = millis();
  }
}

#line 1 "/home/topota/Arduino/WLED/wled00/wled09_button.ino"
/*
 * Physical IO
 */

void shortPressAction()
{
  if (!macroButton)
  {
    toggleOnOff();
    colorUpdated(2);
  } else {
    applyMacro(macroButton);
  }
}


void handleButton()
{
#ifdef BTNPIN
  if (!buttonEnabled) return;
  
  if (digitalRead(BTNPIN) == LOW && !buttonPressedBefore) //pressed
  {
    buttonPressedTime = millis();
    buttonPressedBefore = true;
  }
  else if (digitalRead(BTNPIN) == HIGH && buttonPressedBefore) //released
  {
    long dur = millis() - buttonPressedTime;
    if (dur < 50) {buttonPressedBefore = false; return;} //too short "press", debounce
    bool doublePress = buttonWaitTime;
    buttonWaitTime = 0;

    if (dur > 6000) {initAP();}
    else if (dur > 600) //long press
    {
      if (macroLongPress) {applyMacro(macroLongPress);}
      else _setRandomColor(false,true);
    }
    else { //short press
      if (macroDoublePress)
      {
        if (doublePress) applyMacro(macroDoublePress);
        else buttonWaitTime = millis();
      } else shortPressAction();
    }
    buttonPressedBefore = false;
  }

  if (buttonWaitTime && millis() - buttonWaitTime > 450 && !buttonPressedBefore)
  {
    buttonWaitTime = 0;
    shortPressAction();
  }
#endif
}

void handleIO()
{
  handleButton();
  
  //set relay when LEDs turn on
  if (strip.getBrightness())
  {
    lastOnTime = millis();
    if (offMode)
    { 
      #if RLYPIN >= 0
       digitalWrite(RLYPIN, RLYMDE);
      #endif
      offMode = false;
    }
  } else if (millis() - lastOnTime > 600)
  {
    #if RLYPIN >= 0
     if (!offMode) digitalWrite(RLYPIN, !RLYMDE);
    #endif
    offMode = true;
  }

  #if AUXPIN >= 0
  //output
  if (auxActive || auxActiveBefore)
  {
    if (!auxActiveBefore)
    {
      auxActiveBefore = true;
      switch (auxTriggeredState)
      {
        case 0: pinMode(AUXPIN, INPUT); break;
        case 1: pinMode(AUXPIN, OUTPUT); digitalWrite(AUXPIN, HIGH); break;
        case 2: pinMode(AUXPIN, OUTPUT); digitalWrite(AUXPIN, LOW); break;
      }
      auxStartTime = millis();
    }
    if ((millis() - auxStartTime > auxTime*1000 && auxTime != 255) || !auxActive)
    {
      auxActive = false;
      auxActiveBefore = false;
      switch (auxDefaultState)
      {
        case 0: pinMode(AUXPIN, INPUT); break;
        case 1: pinMode(AUXPIN, OUTPUT); digitalWrite(AUXPIN, HIGH); break;
        case 2: pinMode(AUXPIN, OUTPUT); digitalWrite(AUXPIN, LOW); break;
      }
    }
  }
  #endif
}

#line 1 "/home/topota/Arduino/WLED/wled00/wled10_ntp.ino"
/*
 * Acquires time from NTP server
 */

TimeChangeRule UTCr = {Last, Sun, Mar, 1, 0};     // UTC
Timezone tzUTC(UTCr, UTCr);

TimeChangeRule BST = {Last, Sun, Mar, 1, 60};        // British Summer Time
TimeChangeRule GMT = {Last, Sun, Oct, 2, 0};         // Standard Time
Timezone tzUK(BST, GMT);

TimeChangeRule CEST = {Last, Sun, Mar, 2, 120};     //Central European Summer Time
TimeChangeRule CET = {Last, Sun, Oct, 3, 60};       //Central European Standard Time
Timezone tzEUCentral(CEST, CET);

TimeChangeRule EEST = {Last, Sun, Mar, 3, 180};     //Central European Summer Time
TimeChangeRule EET = {Last, Sun, Oct, 4, 120};       //Central European Standard Time
Timezone tzEUEastern(EEST, EET);

TimeChangeRule EDT = {Second, Sun, Mar, 2, -240 };    //Daylight time = UTC - 4 hours
TimeChangeRule EST = {First, Sun, Nov, 2, -300 };     //Standard time = UTC - 5 hours
Timezone tzUSEastern(EDT, EST);

TimeChangeRule CDT = {Second, Sun, Mar, 2, -300 };    //Daylight time = UTC - 5 hours
TimeChangeRule CST = {First, Sun, Nov, 2, -360 };     //Standard time = UTC - 6 hours
Timezone tzUSCentral(CDT, CST);

TimeChangeRule MDT = {Second, Sun, Mar, 2, -360 };    //Daylight time = UTC - 6 hours
TimeChangeRule MST = {First, Sun, Nov, 2, -420 };     //Standard time = UTC - 7 hours
Timezone tzUSMountain(MDT, MST);

Timezone tzUSArizona(MST, MST); //Mountain without DST

TimeChangeRule PDT = {Second, Sun, Mar, 2, -420 };    //Daylight time = UTC - 7 hours
TimeChangeRule PST = {First, Sun, Nov, 2, -480 };     //Standard time = UTC - 8 hours
Timezone tzUSPacific(PDT, PST);

TimeChangeRule ChST = {Last, Sun, Mar, 1, 480};     // China Standard Time = UTC + 8 hours
Timezone tzChina(ChST, ChST);

TimeChangeRule JST = {Last, Sun, Mar, 1, 540};     // Japan Standard Time = UTC + 9 hours
Timezone tzJapan(JST, JST);

TimeChangeRule AEDT = {Second, Sun, Oct, 2, 660 };    //Daylight time = UTC + 11 hours
TimeChangeRule AEST = {First, Sun, Apr, 3, 600 };     //Standard time = UTC + 10 hours
Timezone tzAUEastern(AEDT, AEST);

TimeChangeRule NZDT = {Second, Sun, Sep, 2, 780 };    //Daylight time = UTC + 13 hours
TimeChangeRule NZST = {First, Sun, Apr, 3, 720 };     //Standard time = UTC + 12 hours
Timezone tzNZ(NZDT, NZST);

TimeChangeRule NKST = {Last, Sun, Mar, 1, 510};     //Pyongyang Time = UTC + 8.5 hours
Timezone tzNK(NKST, NKST);

Timezone* timezones[] = {&tzUTC, &tzUK, &tzEUCentral, &tzEUEastern, &tzUSEastern, &tzUSCentral, &tzUSMountain, &tzUSArizona, &tzUSPacific, &tzChina, &tzJapan, &tzAUEastern, &tzNZ, &tzNK};  

void handleNetworkTime()
{
  if (ntpEnabled && ntpConnected && millis() - ntpLastSyncTime > 50000000L && WiFi.status() == WL_CONNECTED)
  {
    if (millis() - ntpPacketSentTime > 10000)
    {
      sendNTPPacket();
      ntpPacketSentTime = millis();
    }
    if (checkNTPResponse())
    {
      ntpLastSyncTime = millis();
    }
  }
}

void sendNTPPacket()
{
  WiFi.hostByName(ntpServerName, ntpServerIP);
  DEBUG_PRINTLN("send NTP");
  byte pbuf[NTP_PACKET_SIZE];
  memset(pbuf, 0, NTP_PACKET_SIZE);

  pbuf[0] = 0b11100011;   // LI, Version, Mode
  pbuf[1] = 0;     // Stratum, or type of clock
  pbuf[2] = 6;     // Polling Interval
  pbuf[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  pbuf[12]  = 49;
  pbuf[13]  = 0x4E;
  pbuf[14]  = 49;
  pbuf[15]  = 52;

  ntpUdp.beginPacket(ntpServerIP, 123); //NTP requests are to port 123
  ntpUdp.write(pbuf, NTP_PACKET_SIZE);
  ntpUdp.endPacket();
}

bool checkNTPResponse()
{
  int cb = ntpUdp.parsePacket();
  if (cb) {
    DEBUG_PRINT("NTP recv, l=");
    DEBUG_PRINTLN(cb);
    byte pbuf[NTP_PACKET_SIZE];
    ntpUdp.read(pbuf, NTP_PACKET_SIZE); // read the packet into the buffer

    unsigned long highWord = word(pbuf[40], pbuf[41]);
    unsigned long lowWord = word(pbuf[42], pbuf[43]);
    if (highWord == 0 && lowWord == 0) return false;
    
    unsigned long secsSince1900 = highWord << 16 | lowWord;
 
    DEBUG_PRINT("Unix time = ");
    unsigned long epoch = secsSince1900 - 2208988799UL; //subtract 70 years -1sec (on avg. more precision)
    setTime(epoch);
    DEBUG_PRINTLN(epoch);
    if (countdownTime - now() > 0) countdownOverTriggered = false;
    return true;
  }
  return false;
}

void updateLocalTime()
{
  unsigned long tmc = now()+ utcOffsetSecs;
  local = timezones[currentTimezone]->toLocal(tmc);
}

void getTimeString(char* out)
{
  updateLocalTime();
  byte hr = hour(local);
  if (useAMPM)
  {
    if (hr > 11) hr -= 12;
    if (hr == 0) hr  = 12;
  }
  sprintf(out,"%i-%i-%i, %i:%s%i:%s%i",year(local), month(local), day(local), 
                                       hr,(minute(local)<10)?"0":"",minute(local),
                                       (second(local)<10)?"0":"",second(local));
  if (useAMPM)
  {
    strcat(out,(hour(local) > 11)? " PM":" AM");
  }
}

void setCountdown()
{
  countdownTime = timezones[currentTimezone]->toUTC(getUnixTime(countdownHour, countdownMin, countdownSec, countdownDay, countdownMonth, countdownYear));
  if (countdownTime - now() > 0) countdownOverTriggered = false;
}

//returns true if countdown just over
bool checkCountdown()
{
  long diff = countdownTime - now();
  local = abs(diff);
  if (diff <0 && !countdownOverTriggered)
  {
    if (macroCountdown != 0) applyMacro(macroCountdown);
    countdownOverTriggered = true;
    return true;
  }
  return false;
}

byte weekdayMondayFirst()
{
  byte wd = weekday(local) -1;
  if (wd == 0) wd = 7;
  return wd;
}

void checkTimers()
{
  if (lastTimerMinute != minute(local)) //only check once a new minute begins
  {
    lastTimerMinute = minute(local);
    for (uint8_t i = 0; i < 8; i++)
    {
      if (timerMacro[i] != 0
          && (timerHours[i] == hour(local) || timerHours[i] == 24) //if hour is set to 24, activate every hour 
          && timerMinutes[i] == minute(local)
          && (timerWeekday[i] & 0x01) //timer is enabled
          && timerWeekday[i] >> weekdayMondayFirst() & 0x01) //timer should activate at current day of week
      {
        applyMacro(timerMacro[i]);
      }
    }
  }
}

#line 1 "/home/topota/Arduino/WLED/wled00/wled11_ol.ino"
/*
 * Used to draw clock overlays over the strip
 */
void initCronixie()
{
  if (overlayCurrent == 3 && !cronixieInit)
  {
    strip.driverModeCronixie(true);
    strip.setCronixieBacklight(cronixieBacklight);
    setCronixie();
    cronixieInit = true;
  } else if (cronixieInit && overlayCurrent != 3)
  {
    strip.driverModeCronixie(false);
    cronixieInit = false; 
  }
}


void _nixieDisplay(int num[], uint16_t dur[], uint16_t pausedur[], byte cnt)
{
  strip.setRange(overlayMin, overlayMax, 0);
  if (num[nixieClockI] >= 0 && !nixiePause)
  {
    strip.setIndividual(num[nixieClockI],((uint32_t)colT[3] << 24)| ((uint32_t)colT[0] << 16) | ((uint32_t)colT[1] << 8) | colT[2]);
    strip.unlock(num[nixieClockI]);
  }
  if (!nixiePause)
  {
    overlayRefreshMs = dur[nixieClockI];
  } else
  {
    overlayRefreshMs = pausedur[nixieClockI];
  }
  if (pausedur[nixieClockI] > 0 && !nixiePause)
  {
    nixiePause = true;
  } else {
    if (nixieClockI < cnt -1)
    {
      nixieClockI++;
    } else
    {
      nixieClockI = -1;
    }
    nixiePause = false;
  }
}

void _nixieNumber(int number, int dur) 
{
  if (nixieClockI < 0)
  {
    DEBUG_PRINT(number);
    int digitCnt = -1;
    int digits[4];
    digits[3] = number/1000;
    digits[2] = (number/100)%10;
    digits[1] = (number/10)%10;
    digits[0] = number%10;
    if (number > 999) //four digits
    {
      digitCnt = 4;
    } else if (number > 99) //three digits
    {
      digitCnt = 3;
    } else if (number > 9) //two digits
    {
      digitCnt = 2;
    } else { //single digit
      digitCnt = 1;
    }
    DEBUG_PRINT(" ");
    for (int i = 0; i < digitCnt; i++)
    {
      DEBUG_PRINT(digits[i]);
      overlayArr[digitCnt-1-i] = digits[i];
      overlayDur[digitCnt-1-i] = ((dur/4)*3)/digitCnt;
      overlayPauseDur[digitCnt-1-i] = 0;
    }
    DEBUG_PRINTLN(" ");
    for (int i = 1; i < digitCnt; i++)
    {
      if (overlayArr[i] == overlayArr[i-1])
      {
        overlayPauseDur[i-1] = dur/12;
        overlayDur[i-1] = overlayDur[i-1]-dur/12;
      }
    }
    for (int i = digitCnt; i < 6; i++)
    {
      overlayArr[i] = -1;
      overlayDur[i] = 0;
      overlayPauseDur[i] = 0;
    }
    overlayPauseDur[5] = dur/4;
    for (int i = 0; i < 6; i++)
    {
      if (overlayArr[i] != -1)
      {
        overlayArr[i] = overlayArr[i] + overlayMin;
      }
    }
    for (int i = 0; i <6; i++)
    {
      DEBUG_PRINT(overlayArr[i]);
      DEBUG_PRINT(" ");
      DEBUG_PRINT(overlayDur[i]);
      DEBUG_PRINT(" ");
      DEBUG_PRINT(overlayPauseDur[i]);
      DEBUG_PRINT(" ");
    }
    DEBUG_PRINTLN(" ");
    nixieClockI = 0;
  } else {
    _nixieDisplay(overlayArr, overlayDur, overlayPauseDur, 6);
  }
}


void handleOverlays()
{
  if (millis() - overlayRefreshedTime > overlayRefreshMs)
  {
    initCronixie();
    updateLocalTime();
    checkTimers();
    switch (overlayCurrent)
    {
      case 0: break;//no overlay
      case 1: _overlayAnalogClock(); break;//2 analog clock
      case 2: _overlayNixieClock(); break;//nixie 1-digit
      case 3: _overlayCronixie();//Diamex cronixie clock kit
    }
    if (!countdownMode || overlayCurrent < 2) checkCountdown(); //countdown macro activation must work
    overlayRefreshedTime = millis();
  }
}

void _overlayAnalogClock()
{
  int overlaySize = overlayMax - overlayMin +1;
  strip.unlockAll();
  if (countdownMode)
  {
    _overlayAnalogCountdown(); return;
  }
  double hourP = ((double)(hour(local)%12))/12;
  double minuteP = ((double)minute(local))/60;
  hourP = hourP + minuteP/12;
  double secondP = ((double)second(local))/60;
  int hourPixel = floor(analogClock12pixel + overlaySize*hourP);
  if (hourPixel > overlayMax) hourPixel = overlayMin -1 + hourPixel - overlayMax;
  int minutePixel = floor(analogClock12pixel + overlaySize*minuteP);
  if (minutePixel > overlayMax) minutePixel = overlayMin -1 + minutePixel - overlayMax; 
  int secondPixel = floor(analogClock12pixel + overlaySize*secondP);
  if (secondPixel > overlayMax) secondPixel = overlayMin -1 + secondPixel - overlayMax;
  if (analogClockSecondsTrail)
  {
    if (secondPixel < analogClock12pixel)
    {
      strip.setRange(analogClock12pixel, overlayMax, 0xFF0000);
      strip.setRange(overlayMin, secondPixel, 0xFF0000);
    } else
    {
      strip.setRange(analogClock12pixel, secondPixel, 0xFF0000);
    }
  }
  if (analogClock5MinuteMarks)
  {
    int pix;
    for (int i = 0; i <= 12; i++)
    {
      pix = analogClock12pixel + round((overlaySize / 12.0) *i);
      if (pix > overlayMax) pix -= overlaySize;
      strip.setIndividual(pix, 0x00FFAA);
    }
  }
  if (!analogClockSecondsTrail) strip.setIndividual(secondPixel, 0xFF0000);
  strip.setIndividual(minutePixel, 0x00FF00);
  strip.setIndividual(hourPixel, 0x0000FF);
  overlayRefreshMs = 998;
}

void _overlayNixieClock()
{
  #ifdef WLED_DISABLE_CRONIXIE
  if (countdownMode) checkCountdown();
  #else
  
  if (countdownMode)
  {
    _overlayNixieCountdown(); return;
  }
  if (nixieClockI < 0)
  {
      overlayArr[0] = hour(local);
      if (useAMPM) overlayArr[0] = overlayArr[0]%12;
      overlayArr[1] = -1;
      if (overlayArr[0] > 9)
      {
        overlayArr[1] = overlayArr[0]%10;
        overlayArr[0] = overlayArr[0]/10;
      }
      overlayArr[2] = minute(local);
      overlayArr[3] = overlayArr[2]%10;
      overlayArr[2] = overlayArr[2]/10;
      overlayArr[4] = -1;
      overlayArr[5] = -1;
      if (analogClockSecondsTrail)
      {
        overlayArr[4] = second(local);
        overlayArr[5] = overlayArr[4]%10;
        overlayArr[4] = overlayArr[4]/10;
      }
      for (int i = 0; i < 6; i++)
      {
        if (overlayArr[i] != -1)
        {
          overlayArr[i] = overlayArr[i] + overlayMin;
        }
      }
      overlayDur[0] = 12 + 12*(255 - overlaySpeed);
      if (overlayArr[1] == overlayArr[0])
      {
        overlayPauseDur[0] = 3 + 3*(255 - overlaySpeed);
      } else
      {
        overlayPauseDur[0] = 0;
      }
      if (overlayArr[1] == -1)
      {
        overlayDur[1] = 0;
      } else
      {
        overlayDur[1] = 12 + 12*(255 - overlaySpeed);
      }
      overlayPauseDur[1] = 9 + 9*(255 - overlaySpeed);

      overlayDur[2] = 12 + 12*(255 - overlaySpeed);
      if (overlayArr[2] == overlayArr[3])
      {
        overlayPauseDur[2] = 3 + 3*(255 - overlaySpeed);
      } else
      {
        overlayPauseDur[2] = 0;
      }
      overlayDur[3] = 12 + 12*(255 - overlaySpeed);
      overlayPauseDur[3] = 9 + 9*(255 - overlaySpeed);
      
      if (overlayArr[4] == -1)
      {
        overlayDur[4] = 0;
        overlayPauseDur[4] = 0;
        overlayDur[5] = 0;
      } else
      {
        overlayDur[4] = 12 + 12*(255 - overlaySpeed);
        if (overlayArr[5] == overlayArr[4])
        {
          overlayPauseDur[4] = 3 + 3*(255 - overlaySpeed);
        } else
        {
          overlayPauseDur[4] = 0;
        }
        overlayDur[5] = 12 + 12*(255 - overlaySpeed);
      }
      overlayPauseDur[5] = 22 + 22*(255 - overlaySpeed);
      
      nixieClockI = 0;   
  } else
  {
    _nixieDisplay(overlayArr, overlayDur, overlayPauseDur, 6);
  }
  #endif
}

void _overlayAnalogCountdown()
{
  strip.unlockAll();
  if (now() >= countdownTime)
  {
    checkCountdown();
  } else
  {
    long diff = countdownTime - now();
    double pval = 60;
    if (diff > 31557600L) //display in years if more than 365 days
    {
      pval = 315576000L; //10 years
    } else if (diff > 2592000L) //display in months if more than a month
    {
      pval = 31557600L; //1 year
    } else if (diff > 604800) //display in weeks if more than a week
    {
      pval = 2592000L; //1 month
    } else if (diff > 86400) //display in days if more than 24 hours
    {
      pval = 604800; //1 week
    } else if (diff > 3600) //display in hours if more than 60 minutes
    {
      pval = 86400; //1 day
    } else if (diff > 60) //display in minutes if more than 60 seconds
    {
      pval = 3600; //1 hour
    }
    int overlaySize = overlayMax - overlayMin +1;
    double perc = (pval-(double)diff)/pval;
    if (perc > 1.0) perc = 1.0;
    byte pixelCnt = perc*overlaySize;
    if (analogClock12pixel + pixelCnt > overlayMax)
    {
      strip.setRange(analogClock12pixel, overlayMax, ((uint32_t)colSec[3] << 24)| ((uint32_t)colSec[0] << 16) | ((uint32_t)colSec[1] << 8) | colSec[2]);
      strip.setRange(overlayMin, overlayMin +pixelCnt -(1+ overlayMax -analogClock12pixel), ((uint32_t)colSec[3] << 24)| ((uint32_t)colSec[0] << 16) | ((uint32_t)colSec[1] << 8) | colSec[2]);
    } else
    {
      strip.setRange(analogClock12pixel, analogClock12pixel + pixelCnt, ((uint32_t)colSec[3] << 24)| ((uint32_t)colSec[0] << 16) | ((uint32_t)colSec[1] << 8) | colSec[2]);
    }
  }
  overlayRefreshMs = 998;
}


void _overlayNixieCountdown()
{
  if (now() >= countdownTime)
  {
    if (checkCountdown())
    {
      _nixieNumber(2019, 2019);
    }
  } else
  {
    long diff = countdownTime - now();
    if (diff > 86313600L) //display in years if more than 999 days
    {
      diff = diff/31557600L;
    } else if (diff > 3596400) //display in days if more than 999 hours
    {
      diff = diff/86400;
    } else if (diff > 59940) //display in hours if more than 999 minutes
    {
      diff = diff/1440;
    } else if (diff > 999) //display in minutes if more than 999 seconds
    {
      diff = diff/60;
    }
    _nixieNumber(diff, 800);
  }
  overlayRefreshMs = 998;
}

#line 1 "/home/topota/Arduino/WLED/wled00/wled12_alexa.ino"
/*
 * Alexa Voice On/Off/Brightness Control. Emulates a Philips Hue bridge to Alexa.
 * 
 * This was put together from these two excellent projects:
 * https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch
 * https://github.com/probonopd/ESP8266HueEmulator
 */
void prepareIds() {
  escapedMac = WiFi.macAddress();
  escapedMac.replace(":", "");
  escapedMac.toLowerCase();
}

#ifndef WLED_DISABLE_ALEXA
void onAlexaChange(EspalexaDevice* dev);

void alexaInit()
{
  if (alexaEnabled && WiFi.status() == WL_CONNECTED)
  {
    if (espalexaDevice == nullptr) //only init once
    {
      espalexaDevice = new EspalexaDevice(alexaInvocationName, onAlexaChange, EspalexaDeviceType::extendedcolor);
      espalexa.addDevice(espalexaDevice);
      espalexa.begin(&server);
    } else {
      espalexaDevice->setName(alexaInvocationName);
    }
  }
}

void handleAlexa()
{
  if (!alexaEnabled || WiFi.status() != WL_CONNECTED) return;
  espalexa.loop();
}

void onAlexaChange(EspalexaDevice* dev)
{
  EspalexaDeviceProperty m = espalexaDevice->getLastChangedProperty();
  
  if (m == EspalexaDeviceProperty::on)
  {
    if (!macroAlexaOn)
    {
      if (bri == 0)
      {
        bri = briLast;
        colorUpdated(10);
      }
    } else applyMacro(macroAlexaOn);
  } else if (m == EspalexaDeviceProperty::off)
  {
    if (!macroAlexaOff)
    {
      if (bri > 0)
      {
        briLast = bri;
        bri = 0;
        colorUpdated(10);
      }
    } else applyMacro(macroAlexaOff);
  } else if (m == EspalexaDeviceProperty::bri)
  {
    bri = espalexaDevice->getValue();
    colorUpdated(10);
  } else //color
  {
    uint32_t color = espalexaDevice->getRGB();
    col[0] = ((color >> 16) & 0xFF);
    col[1] = ((color >>  8) & 0xFF);
    col[2] = (color & 0xFF);
    if (useRGBW) colorRGBtoRGBW(col);
    colorUpdated(10);
  }
}


#else
 void alexaInit(){}
 void handleAlexa(){}
#endif

#line 1 "/home/topota/Arduino/WLED/wled00/wled13_cronixie.ino"
/*
 * Support for the Cronixie clock
 */
byte getSameCodeLength(char code, int index, char const cronixieDisplay[])
{
  byte counter = 0;
  
  for (int i = index+1; i < 6; i++)
  {
    if (cronixieDisplay[i] == code)
    {
      counter++;
    } else {
      return counter;
    }
  }
  return counter;
}

void setCronixie()
{
  #ifndef WLED_DISABLE_CRONIXIE
  /*
   * digit purpose index
   * 0-9 | 0-9 (incl. random)
   * 10 | blank
   * 11 | blank, bg off
   * 12 | test upw.
   * 13 | test dnw.
   * 14 | binary AM/PM
   * 15 | BB upper +50 for no trailing 0
   * 16 | BBB
   * 17 | BBBB
   * 18 | BBBBB
   * 19 | BBBBBB
   * 20 | H
   * 21 | HH
   * 22 | HHH
   * 23 | HHHH
   * 24 | M
   * 25 | MM
   * 26 | MMM
   * 27 | MMMM
   * 28 | MMMMM
   * 29 | MMMMMM
   * 30 | S
   * 31 | SS
   * 32 | SSS
   * 33 | SSSS
   * 34 | SSSSS
   * 35 | SSSSSS
   * 36 | Y
   * 37 | YY
   * 38 | YYYY
   * 39 | I
   * 40 | II
   * 41 | W
   * 42 | WW
   * 43 | D
   * 44 | DD
   * 45 | DDD
   * 46 | V
   * 47 | VV
   * 48 | VVV
   * 49 | VVVV
   * 50 | VVVVV
   * 51 | VVVVVV
   * 52 | v
   * 53 | vv
   * 54 | vvv
   * 55 | vvvv
   * 56 | vvvvv
   * 57 | vvvvvv
   */

  //H HourLower | HH - Hour 24. | AH - Hour 12. | HHH Hour of Month | HHHH Hour of Year
  //M MinuteUpper | MM Minute of Hour | MMM Minute of 12h | MMMM Minute of Day | MMMMM Minute of Month | MMMMMM Minute of Year
  //S SecondUpper | SS Second of Minute | SSS Second of 10 Minute | SSSS Second of Hour | SSSSS Second of Day | SSSSSS Second of Week
  //B AM/PM | BB 0-6/6-12/12-18/18-24 | BBB 0-3... | BBBB 0-1.5... | BBBBB 0-1 | BBBBBB 0-0.5
  
  //Y YearLower | YY - Year LU | YYYY - Std.
  //I MonthLower | II - Month of Year 
  //W Week of Month | WW Week of Year
  //D Day of Week | DD Day Of Month | DDD Day Of Year

  DEBUG_PRINT("cset ");
  DEBUG_PRINTLN(cronixieDisplay);

  overlayRefreshMs = 1997; //Only refresh every 2secs if no seconds are displayed
  
  for (int i = 0; i < 6; i++)
  {
    dP[i] = 10;
    switch (cronixieDisplay[i])
    {
      case '_': dP[i] = 10; break; 
      case '-': dP[i] = 11; break; 
      case 'r': dP[i] = random(1,7); break; //random btw. 1-6
      case 'R': dP[i] = random(0,10); break; //random btw. 0-9
      case 't': break; //Test upw.
      case 'T': break; //Test dnw.
      case 'b': dP[i] = 14 + getSameCodeLength('b',i,cronixieDisplay); i = i+dP[i]-14; break; 
      case 'B': dP[i] = 14 + getSameCodeLength('B',i,cronixieDisplay); i = i+dP[i]-14; break;
      case 'h': dP[i] = 70 + getSameCodeLength('h',i,cronixieDisplay); i = i+dP[i]-70; break;
      case 'H': dP[i] = 20 + getSameCodeLength('H',i,cronixieDisplay); i = i+dP[i]-20; break;
      case 'A': dP[i] = 108; i++; break;
      case 'a': dP[i] = 58; i++; break;
      case 'm': dP[i] = 74 + getSameCodeLength('m',i,cronixieDisplay); i = i+dP[i]-74; break;
      case 'M': dP[i] = 24 + getSameCodeLength('M',i,cronixieDisplay); i = i+dP[i]-24; break;
      case 's': dP[i] = 80 + getSameCodeLength('s',i,cronixieDisplay); i = i+dP[i]-80; overlayRefreshMs = 497; break; //refresh more often bc. of secs
      case 'S': dP[i] = 30 + getSameCodeLength('S',i,cronixieDisplay); i = i+dP[i]-30; overlayRefreshMs = 497; break;
      case 'Y': dP[i] = 36 + getSameCodeLength('Y',i,cronixieDisplay); i = i+dP[i]-36; break; 
      case 'y': dP[i] = 86 + getSameCodeLength('y',i,cronixieDisplay); i = i+dP[i]-86; break; 
      case 'I': dP[i] = 39 + getSameCodeLength('I',i,cronixieDisplay); i = i+dP[i]-39; break;  //Month. Don't ask me why month and minute both start with M.
      case 'i': dP[i] = 89 + getSameCodeLength('i',i,cronixieDisplay); i = i+dP[i]-89; break; 
      case 'W': break;
      case 'w': break;
      case 'D': dP[i] = 43 + getSameCodeLength('D',i,cronixieDisplay); i = i+dP[i]-43; break;
      case 'd': dP[i] = 93 + getSameCodeLength('d',i,cronixieDisplay); i = i+dP[i]-93; break;
      case '0': dP[i] = 0; break;
      case '1': dP[i] = 1; break;
      case '2': dP[i] = 2; break;
      case '3': dP[i] = 3; break;
      case '4': dP[i] = 4; break;
      case '5': dP[i] = 5; break;
      case '6': dP[i] = 6; break;
      case '7': dP[i] = 7; break;
      case '8': dP[i] = 8; break;
      case '9': dP[i] = 9; break;
      case 'V': break; //user var0
      case 'v': break; //user var1
    }
  }
  DEBUG_PRINT("result ");
  for (int i = 0; i < 5; i++)
  {
    DEBUG_PRINT((int)dP[i]);
    DEBUG_PRINT(" ");
  }
  DEBUG_PRINTLN((int)dP[5]);

  _overlayCronixie(); //refresh
  #endif
}

void _overlayCronixie()
{
  if (countdownMode) checkCountdown();
  #ifndef WLED_DISABLE_CRONIXIE
  
  byte h = hour(local);
  byte h0 = h;
  byte m = minute(local);
  byte s = second(local);
  byte d = day(local);
  byte mi = month(local);
  int y = year(local);
  //this has to be changed in time for 22nd century
  y -= 2000; if (y<0) y += 30; //makes countdown work

  if (useAMPM && !countdownMode)
  {
    if (h>12) h-=12;
    else if (h==0) h+=12;
  }
  byte _digitOut[]{10,10,10,10,10,10};
  for (int i = 0; i < 6; i++)
  {
    if (dP[i] < 12) _digitOut[i] = dP[i];
    else {
      if (dP[i] < 65)
      {
        switch(dP[i])
        {
          case 21: _digitOut[i] = h/10; _digitOut[i+1] = h- _digitOut[i]*10; i++; break; //HH
          case 25: _digitOut[i] = m/10; _digitOut[i+1] = m- _digitOut[i]*10; i++; break; //MM
          case 31: _digitOut[i] = s/10; _digitOut[i+1] = s- _digitOut[i]*10; i++; break; //SS

          case 20: _digitOut[i] = h- (h/10)*10; break; //H
          case 24: _digitOut[i] = m/10; break; //M
          case 30: _digitOut[i] = s/10; break; //S
          
          case 43: _digitOut[i] = weekday(local); _digitOut[i]--; if (_digitOut[i]<1) _digitOut[i]= 7; break; //D
          case 44: _digitOut[i] = d/10; _digitOut[i+1] = d- _digitOut[i]*10; i++; break; //DD
          case 40: _digitOut[i] = mi/10; _digitOut[i+1] = mi- _digitOut[i]*10; i++; break; //II
          case 37: _digitOut[i] = y/10; _digitOut[i+1] = y- _digitOut[i]*10; i++; break; //YY
          case 39: _digitOut[i] = 2; _digitOut[i+1] = 0; _digitOut[i+2] = y/10; _digitOut[i+3] = y- _digitOut[i+2]*10; i+=3; break; //YYYY
          
          case 16: _digitOut[i+2] = ((h0/3)&1)?1:0; i++; //BBB (BBBB NI)
          case 15: _digitOut[i+1] = (h0>17 || (h0>5 && h0<12))?1:0; i++; //BB
          case 14: _digitOut[i] = (h0>11)?1:0; break; //B
        }
      } else
      {
        switch(dP[i])
        {
          case 71: _digitOut[i] = h/10; _digitOut[i+1] = h- _digitOut[i]*10; if(_digitOut[i] == 0) _digitOut[i]=10; i++; break; //hh
          case 75: _digitOut[i] = m/10; _digitOut[i+1] = m- _digitOut[i]*10; if(_digitOut[i] == 0) _digitOut[i]=10; i++; break; //mm
          case 81: _digitOut[i] = s/10; _digitOut[i+1] = s- _digitOut[i]*10; if(_digitOut[i] == 0) _digitOut[i]=10; i++; break; //ss
          case 66: _digitOut[i+2] = ((h0/3)&1)?1:10; i++; //bbb (bbbb NI)
          case 65: _digitOut[i+1] = (h0>17 || (h0>5 && h0<12))?1:10; i++; //bb
          case 64: _digitOut[i] = (h0>11)?1:10; break; //b

          case 93: _digitOut[i] = weekday(local); _digitOut[i]--; if (_digitOut[i]<1) _digitOut[i]= 7; break; //d
          case 94: _digitOut[i] = d/10; _digitOut[i+1] = d- _digitOut[i]*10; if(_digitOut[i] == 0) _digitOut[i]=10; i++; break; //dd
          case 90: _digitOut[i] = mi/10; _digitOut[i+1] = mi- _digitOut[i]*10; if(_digitOut[i] == 0) _digitOut[i]=10; i++; break; //ii
          case 87: _digitOut[i] = y/10; _digitOut[i+1] = y- _digitOut[i]*10; i++; break; //yy
          case 89: _digitOut[i] = 2; _digitOut[i+1] = 0; _digitOut[i+2] = y/10; _digitOut[i+3] = y- _digitOut[i+2]*10; i+=3; break; //yyyy
        }
      }
    }
  }
  strip.setCronixieDigits(_digitOut);
  //strip.trigger(); //this has a drawback, no effects slower than RefreshMs. advantage: Quick update, not dependant on effect time
  #endif
}

#line 1 "/home/topota/Arduino/WLED/wled00/wled14_colors.ino"
/*
 * Color conversion methods
 */

void colorFromUint32(uint32_t in)
{
  col[3] = in >> 24 & 0xFF;
  col[0] = in >> 16 & 0xFF;
  col[1] = in >> 8  & 0xFF;
  col[2] = in       & 0xFF;
}

void colorHStoRGB(uint16_t hue, byte sat, byte* rgb) //hue, sat to rgb
{
  float h = ((float)hue)/65535.0;
  float s = ((float)sat)/255.0;
  byte i = floor(h*6);
  float f = h * 6-i;
  float p = 255 * (1-s);
  float q = 255 * (1-f*s);
  float t = 255 * (1-(1-f)*s);
  switch (i%6) {
    case 0: rgb[0]=255,rgb[1]=t,rgb[2]=p;break;
    case 1: rgb[0]=q,rgb[1]=255,rgb[2]=p;break;
    case 2: rgb[0]=p,rgb[1]=255,rgb[2]=t;break;
    case 3: rgb[0]=p,rgb[1]=q,rgb[2]=255;break;
    case 4: rgb[0]=t,rgb[1]=p,rgb[2]=255;break;
    case 5: rgb[0]=255,rgb[1]=p,rgb[2]=q;
  }
}

#ifndef WLED_DISABLE_HUESYNC
void colorCTtoRGB(uint16_t mired, byte* rgb) //white spectrum to rgb
{
  //this is only an approximation using WS2812B with gamma correction enabled
  if (mired > 475) {
    rgb[0]=255;rgb[1]=199;rgb[2]=92;//500
  } else if (mired > 425) {
    rgb[0]=255;rgb[1]=213;rgb[2]=118;//450
  } else if (mired > 375) {
    rgb[0]=255;rgb[1]=216;rgb[2]=118;//400
  } else if (mired > 325) {
    rgb[0]=255;rgb[1]=234;rgb[2]=140;//350
  } else if (mired > 275) {
    rgb[0]=255;rgb[1]=243;rgb[2]=160;//300
  } else if (mired > 225) {
    rgb[0]=250;rgb[1]=255;rgb[2]=188;//250
  } else if (mired > 175) {
    rgb[0]=247;rgb[1]=255;rgb[2]=215;//200
  } else {
    rgb[0]=237;rgb[1]=255;rgb[2]=239;//150
  }
}

void colorXYtoRGB(float x, float y, byte* rgb) //coordinates to rgb (https://www.developers.meethue.com/documentation/color-conversions-rgb-xy)
{
  float z = 1.0f - x - y;
  float X = (1.0f / y) * x;
  float Z = (1.0f / y) * z;
  float r = (int)255*(X * 1.656492f - 0.354851f - Z * 0.255038f);
  float g = (int)255*(-X * 0.707196f + 1.655397f + Z * 0.036152f);
  float b = (int)255*(X * 0.051713f - 0.121364f + Z * 1.011530f);
  if (r > b && r > g && r > 1.0f) {
    // red is too big
    g = g / r;
    b = b / r;
    r = 1.0f;
  } else if (g > b && g > r && g > 1.0f) {
    // green is too big
    r = r / g;
    b = b / g;
    g = 1.0f;
  } else if (b > r && b > g && b > 1.0f) {
    // blue is too big
    r = r / b;
    g = g / b;
    b = 1.0f;
  }
  // Apply gamma correction
  r = r <= 0.0031308f ? 12.92f * r : (1.0f + 0.055f) * pow(r, (1.0f / 2.4f)) - 0.055f;
  g = g <= 0.0031308f ? 12.92f * g : (1.0f + 0.055f) * pow(g, (1.0f / 2.4f)) - 0.055f;
  b = b <= 0.0031308f ? 12.92f * b : (1.0f + 0.055f) * pow(b, (1.0f / 2.4f)) - 0.055f;

  if (r > b && r > g) {
    // red is biggest
    if (r > 1.0f) {
      g = g / r;
      b = b / r;
      r = 1.0f;
    }
  } else if (g > b && g > r) {
    // green is biggest
    if (g > 1.0f) {
      r = r / g;
      b = b / g;
      g = 1.0f;
    }
  } else if (b > r && b > g) {
    // blue is biggest
    if (b > 1.0f) {
      r = r / b;
      g = g / b;
      b = 1.0f;
    }
  }
  rgb[0] = 255.0*r;
  rgb[1] = 255.0*g;
  rgb[2] = 255.0*b;
}

void colorRGBtoXY(byte* rgb, float* xy) //rgb to coordinates (https://www.developers.meethue.com/documentation/color-conversions-rgb-xy)
{
  float X = rgb[0] * 0.664511f + rgb[1] * 0.154324f + rgb[2] * 0.162028f;
  float Y = rgb[0] * 0.283881f + rgb[1] * 0.668433f + rgb[2] * 0.047685f;
  float Z = rgb[0] * 0.000088f + rgb[1] * 0.072310f + rgb[2] * 0.986039f;
  xy[0] = X / (X + Y + Z);
  xy[1] = Y / (X + Y + Z);
}
#endif

void colorFromDecOrHexString(byte* rgb, char* in)
{
  if (in[0] == 0) return;
  char first = in[0];
  uint32_t c = 0;
  
  if (first == '#' || first == 'h' || first == 'H') //is HEX encoded
  {
    c = strtoul(in +1, NULL, 16);
  } else
  {
    c = strtoul(in, NULL, 10);
  }

  rgb[3] = (c >> 24) & 0xFF;
  rgb[0] = (c >> 16) & 0xFF;
  rgb[1] = (c >>  8) & 0xFF;
  rgb[2] =  c        & 0xFF;
}

float minf (float v, float w)
{
  if (w > v) return v;
  return w;
}

float maxf (float v, float w)
{
  if (w > v) return w;
  return v;
}

void colorRGBtoRGBW(byte* rgb) //rgb to rgbw (http://codewelt.com/rgbw)
{
  float low = minf(rgb[0],minf(rgb[1],rgb[2]));
  float high = maxf(rgb[0],maxf(rgb[1],rgb[2]));
  if (high < 0.1f) return;
  float sat = 255.0f * ((high - low) / high);
  rgb[3] = (byte)((255.0f - sat) / 255.0f * (rgb[0] + rgb[1] + rgb[2]) / 3);
}

#line 1 "/home/topota/Arduino/WLED/wled00/wled15_hue.ino"
/*
 * Sync to Philips hue lights
 */
#ifndef WLED_DISABLE_HUESYNC
void handleHue()
{
  if (hueClient != nullptr && millis() - hueLastRequestSent > huePollIntervalMs && WiFi.status() == WL_CONNECTED)
  {
    hueLastRequestSent = millis();
    if (huePollingEnabled)
    {
      reconnectHue();
    } else {
      hueClient->close();
      if (hueError[0] == 'A') strcpy(hueError,"Inactive");
    }
  }
  if (hueReceived)
  {
    colorUpdated(7); hueReceived = false;
    if (hueStoreAllowed && hueNewKey)
    {
      saveSettingsToEEPROM(); //save api key
      hueStoreAllowed = false;
      hueNewKey = false;
    }
  }
}

void reconnectHue()
{
  if (WiFi.status() != WL_CONNECTED || !huePollingEnabled) return;
  DEBUG_PRINTLN("Hue reconnect");
  if (hueClient == nullptr) {
    hueClient = new AsyncClient();
    hueClient->onConnect(&onHueConnect, hueClient);
    hueClient->onData(&onHueData, hueClient);
    hueClient->onError(&onHueError, hueClient);
    hueAuthRequired = (strlen(hueApiKey)<20);
  }
  hueClient->connect(hueIP, 80);
}

void onHueError(void* arg, AsyncClient* client, int8_t error)
{
  DEBUG_PRINTLN("Hue err");
  strcpy(hueError,"Request timeout");
}

void onHueConnect(void* arg, AsyncClient* client)
{
  DEBUG_PRINTLN("Hue connect");
  sendHuePoll();
}

void sendHuePoll()
{
  if (hueClient == nullptr || !hueClient->connected()) return;
  String req = "";
  if (hueAuthRequired)
  {
    req += "POST /api HTTP/1.1\r\nHost: ";
    req += hueIP.toString();
    req += "\r\nContent-Length: 25\r\n\r\n{\"devicetype\":\"wled#esp\"}";
  } else
  {
    req += "GET /api/";
    req += hueApiKey;
    req += "/lights/" + String(huePollLightId);
    req += " HTTP/1.1\r\nHost: ";
    req += hueIP.toString();
    req += "\r\n\r\n";
  }
  hueClient->add(req.c_str(), req.length());
  hueClient->send();
  hueLastRequestSent = millis();
}

void onHueData(void* arg, AsyncClient* client, void *data, size_t len)
{
  if (!len) return;
  char* str = (char*)data;
  DEBUG_PRINTLN(hueApiKey);
  DEBUG_PRINTLN(str);
  //only get response body
  str = strstr(str,"\r\n\r\n");
  if (str == nullptr) return;
  str += 4;

  StaticJsonDocument<512> root;
  if (str[0] == '[') //is JSON array
  {
    auto error = deserializeJson(root, str);
    if (error)
    {
      strcpy(hueError,"JSON parsing error"); return;
    }
    
    int hueErrorCode = root[0]["error"]["type"];
    if (hueErrorCode)//hue bridge returned error
    {
      switch (hueErrorCode)
      {
        case 1: strcpy(hueError,"Unauthorized"); hueAuthRequired = true; break;
        case 3: strcpy(hueError,"Invalid light ID"); huePollingEnabled = false; break;
        case 101: strcpy(hueError,"Link button not pressed"); hueAuthRequired = true; break;
        default:
          char coerr[18];
          sprintf(coerr,"Bridge Error %i",hueErrorCode);
          strcpy(hueError,coerr);
      }
      return;
    }
    
    if (hueAuthRequired)
    {
      const char* apikey = root[0]["success"]["username"];
      if (apikey != nullptr && strlen(apikey) < sizeof(hueApiKey))
      {
        strcpy(hueApiKey, apikey);
        hueAuthRequired = false;
        hueNewKey = true;
      }
    }
    return;
  }

  //else, assume it is JSON object, look for state and only parse that
  str = strstr(str,"state");
  if (str == nullptr) return;
  str = strstr(str,"{");
  
  auto error = deserializeJson(root, str);
  if (error)
  {
    strcpy(hueError,"JSON parsing error"); return;
  }

  float hueX=0, hueY=0;
  uint16_t hueHue=0, hueCt=0;
  byte hueBri=0, hueSat=0, hueColormode=0;

  if (root["on"]) {
    if (root.containsKey("bri")) //Dimmable device
    {
      hueBri = root["bri"];
      hueBri++;
      const char* cm =root["colormode"];
      if (cm != nullptr) //Color device
      {
        if (strstr(cm,"ct") != nullptr) //ct mode
        {
          hueCt = root["ct"];
          hueColormode = 3;
        } else if (strstr(cm,"xy") != nullptr) //xy mode
        {
          hueX = root["xy"][0]; // 0.5051
          hueY = root["xy"][1]; // 0.4151
          hueColormode = 1;
        } else //hs mode
        {
          hueHue = root["hue"];
          hueSat = root["sat"];
          hueColormode = 2;
        }
      }
    } else //On/Off device
    {
      hueBri = briLast;
    }
  } else
  {
    hueBri = 0;
  }

  strcpy(hueError,"Active");
  
  //apply vals
  if (hueBri != hueBriLast)
  {
    if (hueApplyOnOff)
    {
      if (hueBri==0) {bri = 0;}
      else if (bri==0 && hueBri>0) bri = briLast;
    }
    if (hueApplyBri)
    {
      if (hueBri>0) bri = hueBri;
    }
    hueBriLast = hueBri;
  }
  if (hueApplyColor)
  {
    switch(hueColormode)
    {
      case 1: if (hueX != hueXLast || hueY != hueYLast) colorXYtoRGB(hueX,hueY,col); hueXLast = hueX; hueYLast = hueY; break;
      case 2: if (hueHue != hueHueLast || hueSat != hueSatLast) colorHStoRGB(hueHue,hueSat,col); hueHueLast = hueHue; hueSatLast = hueSat; break;
      case 3: if (hueCt != hueCtLast) colorCTtoRGB(hueCt,col); hueCtLast = hueCt; break;
    }
  }
  hueReceived = true;
}
#else
void handleHue(){}
void reconnectHue(){}
#endif

#line 1 "/home/topota/Arduino/WLED/wled00/wled16_blynk.ino"
/*
 * Remote light control with the free Blynk app
 */

uint16_t blHue = 0;
byte blSat = 255;

void initBlynk(const char* auth)
{
  #ifndef WLED_DISABLE_BLYNK
  if (WiFi.status() != WL_CONNECTED) return;
  blynkEnabled = (auth[0] != 0);
  if (blynkEnabled) Blynk.config(auth);
  #endif
}

void handleBlynk()
{
  #ifndef WLED_DISABLE_BLYNK
  if (WiFi.status() == WL_CONNECTED && blynkEnabled)
  Blynk.run();
  #endif
}

void updateBlynk()
{
  #ifndef WLED_DISABLE_BLYNK
  if (onlyAP) return;
  Blynk.virtualWrite(V0, bri);
  //we need a RGB -> HSB convert here
  Blynk.virtualWrite(V3, bri? 1:0);
  Blynk.virtualWrite(V4, effectCurrent);
  Blynk.virtualWrite(V5, effectSpeed);
  Blynk.virtualWrite(V6, effectIntensity);
  Blynk.virtualWrite(V7, nightlightActive);
  Blynk.virtualWrite(V8, notifyDirect);
  #endif
}

#ifndef WLED_DISABLE_BLYNK
BLYNK_WRITE(V0)
{
  bri = param.asInt();//bri
  colorUpdated(9);
}

BLYNK_WRITE(V1)
{
  blHue = param.asInt();//hue
  colorHStoRGB(blHue*10,blSat,(false)? colSec:col);
  colorUpdated(9);
}

BLYNK_WRITE(V2)
{
  blSat = param.asInt();//sat
  colorHStoRGB(blHue*10,blSat,(false)? colSec:col);
  colorUpdated(9);
}

BLYNK_WRITE(V3)
{
  bool on = (param.asInt()>0);
  if (!on != !bri) {toggleOnOff(); colorUpdated(9);}
}

BLYNK_WRITE(V4)
{
  effectCurrent = param.asInt()-1;//fx
  colorUpdated(9);
}

BLYNK_WRITE(V5)
{
  effectSpeed = param.asInt();//sx
  colorUpdated(9);
}

BLYNK_WRITE(V6)
{
  effectIntensity = param.asInt();//ix
  colorUpdated(9);
}

BLYNK_WRITE(V7)
{
  nightlightActive = (param.asInt()>0);
}

BLYNK_WRITE(V8)
{
  notifyDirect = (param.asInt()>0); //send notifications
}
#endif

#line 1 "/home/topota/Arduino/WLED/wled00/wled17_mqtt.ino"
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

  sendHADiscoveryMQTT();
  publishMqtt();
  DEBUG_PRINTLN("MQTT ready");
}


void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {

  DEBUG_PRINT("MQTT callb rec: ");
  DEBUG_PRINTLN(topic);
  DEBUG_PRINTLN(payload);

  //no need to check the topic because we only get topics we are subscribed to

  if (strstr(topic, "/col"))
  {
    colorFromDecOrHexString(col, (char*)payload);
    colorUpdated(1);
  } else if (strstr(topic, "/api"))
  {
    String apireq = "win&";
    apireq += (char*)payload;
    handleSet(nullptr, apireq);
  }
  else if(strstr(topic, "/speed")){
    char s[10];
    effectSpeed=(uint8_t)atoi(payload);
    sprintf (s, "[FX=%02d] %s", strip.getMode(), efectos[strip.getMode()]);
    String apireq = "win&";
    apireq += (char*)s;
    handleSet(nullptr, apireq);
  }
  else if(strstr(topic, "/intent")){
    char s[10];
    effectIntensity=(uint8_t)atoi(payload);
    sprintf (s, "[FX=%02d] %s", strip.getMode(), efectos[strip.getMode()]);
    String apireq = "win&";
    apireq += (char*)s;
    handleSet(nullptr, apireq);
  }else parseMQTTBriPayload(payload);
}


void publishMqtt()
{
  if (mqtt == NULL) return;
  if (!mqtt->connected()) return;
  DEBUG_PRINTLN("Publish MQTT");

  char s[10];
  char subuf[38];
  
  sprintf(s, "%ld", bri);
  strcpy(subuf, mqttDeviceTopic);
  strcat(subuf, "/g");
  mqtt->publish(subuf, 0, true, s);

  // sprintf(s, "#%06X", col[3]*16777216 + col[0]*65536 + col[1]*256 + col[2]);
  // strcpy(subuf, mqttDeviceTopic);
  // strcat(subuf, "/c");
  // mqtt->publish(subuf, 0, true, s);
  sprintf(s, "", col[0] + col[1] + col[2]);
  strcpy(subuf, mqttDeviceTopic);
  strcat(subuf, "/c");
  mqtt->publish(subuf, 0, true, s);

  //Envio del estado encendido/apagado
  strcpy(subuf, mqttDeviceTopic);
  strcat(subuf, "/state");
  if (bri>0){strcpy(s, "ON");}else{strcpy(s, "OFF");ticker.once(1, Restore_Solid);}
  mqtt->publish(subuf, 0, true, s);
  DEBUG_PRINTLN(strip.getSpeed());

  //Envio del Efecto seleccionado
  strcpy(subuf, mqttDeviceTopic);
  strcat(subuf, "/api/state");
  sprintf (s, "[FX=%02d] %s", strip.getMode(), efectos[strip.getMode()]);
  mqtt->publish(subuf, 0, true, s);

  //Envio la velocidad
  strcpy(subuf, mqttDeviceTopic);
  strcat(subuf, "/speed/state");
  sprintf (s, "%d", strip.getSpeed());
  mqtt->publish(subuf, 0, true, s);

  //Envio la intensidad
  strcpy(subuf, mqttDeviceTopic);
  strcat(subuf, "/intent/state");
  sprintf (s, "%d", effectIntensity);
  mqtt->publish(subuf, 0, true, s);

  // char apires[1024];
  // XML_response(nullptr, false, apires);
  // strcpy(subuf, mqttDeviceTopic);
  // strcat(subuf, "/v");
  // mqtt->publish(subuf, 0, true, apires);
}

const char HA_static_JSON[] PROGMEM = R"=====(,"bri_val_tpl":"{{value}}","rgb_cmd_tpl":"{{'#%02x%02x%02x' | format(red, green, blue)}}","rgb_val_tpl":"{{value[1:3]|int(base=16)}},{{value[3:5]|int(base=16)}},{{value[5:7]|int(base=16)}}","qos":0,"opt":true,"pl_on":"ON","pl_off":"OFF","fx_val_tpl":"{{value}}","fx_list":[)=====";

void sendHADiscoveryMQTT()
{
  
#if ARDUINO_ARCH_ESP32 || LEDPIN != 3
/*

YYYY is discovery tipic
XXXX is device name

Send out HA MQTT Discovery message on MQTT connect (~2.4kB):
{
"name": "XXXX",
"stat_t":"YYYY/c",
"cmd_t":"YYYY",
"rgb_stat_t":"YYYY/c",
"rgb_cmd_t":"YYYY/col",
"bri_cmd_t":"YYYY",
"bri_stat_t":"YYYY/g",
"bri_val_tpl":"{{value}}",
"rgb_cmd_tpl":"{{'#%02x%02x%02x' | format(red, green, blue)}}",
"rgb_val_tpl":"{{value[1:3]|int(base=16)}},{{value[3:5]|int(base=16)}},{{value[5:7]|int(base=16)}}",
"qos": 0,
"opt":true,
"pl_on": "ON",
"pl_off": "OFF",
"fx_cmd_t":"YYYY/api",
"fx_stat_t":"YYYY/api",
"fx_val_tpl":"{{value}}",
"fx_list":[
"[FX=00] Solid",
"[FX=01] Blink", 
"[FX=02] ...",
"[FX=79] Ripple"
]
}

  */
  char bufc[36], bufcc[4], bufcol[38], bufg[36], bufapi[38], buffer[2500];

  strcpy(bufc, mqttDeviceTopic);
  strcpy(bufcol, mqttDeviceTopic);
  strcpy(bufg, mqttDeviceTopic);
  strcpy(bufapi, mqttDeviceTopic);
  if (bri=0){strcpy(bufcc, "OFF");}else{(bufcc, "ON");}
  strcat(bufc, "/c");
  strcat(bufcol, "/col");
  strcat(bufg, "/g");
  strcat(bufapi, "/api");

  StaticJsonDocument<JSON_OBJECT_SIZE(9) +512> root;
  root["name"] = serverDescription;
  root["stat_t"] = bufc;
  root["cmd_t"] = mqttDeviceTopic;
  root["rgb_stat_t"] = bufc;
  root["rgb_cmd_t"] = bufcol;
  root["bri_cmd_t"] = mqttDeviceTopic;
  root["bri_stat_t"] = bufg;
  root["fx_cmd_t"] = bufapi;
  root["fx_stat_t"] = bufapi;

  size_t jlen = measureJson(root);
  DEBUG_PRINTLN(jlen);
  serializeJson(root, buffer, jlen);

  //add values which don't change
  strcpy_P(buffer + jlen -1, HA_static_JSON);

  olen = 0;
  obuf = buffer + jlen -1 + strlen_P(HA_static_JSON);

  //add fx_list
  uint16_t jmnlen = strlen_P(JSON_mode_names);
  uint16_t nameStart = 0, nameEnd = 0;
  int i = 0;
  bool isNameStart = true;

  for (uint16_t j = 0; j < jmnlen; j++)
  {
    if (pgm_read_byte(JSON_mode_names + j) == '\"' || j == jmnlen -1)
    {
      if (isNameStart) 
      {
        nameStart = j +1;
      }
      else 
      {
        nameEnd = j;
        char mdnfx[64], mdn[56];
        uint16_t namelen = nameEnd - nameStart;
        strncpy_P(mdn, JSON_mode_names + nameStart, namelen);
        mdn[namelen] = 0;
        snprintf(mdnfx, 64, "\"[FX=%02d] %s\",", i, mdn);
        oappend(mdnfx);
        DEBUG_PRINTLN(mdnfx);
        i++;
      }
      isNameStart = !isNameStart;
    }   
  }
  olen--;
  oappend("]}");

  DEBUG_PRINT("HA Discovery Sending >>");
  DEBUG_PRINTLN(buffer);

  char pubt[25 + 12 + 8];
  strcpy(pubt, "homeassistant/light/WLED_");
  strcat(pubt, escapedMac.c_str());
  strcat(pubt, "/config");
  mqtt->publish(pubt, 0, true, buffer);
#endif
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

#line 1 "/home/topota/Arduino/WLED/wled00/wled18_server.ino"
/*
 * Server page definitions
 */

void initServer()
{
  //CORS compatiblity
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");
  
  //settings page
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    serveSettings(request);
  });
  
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!handleFileRead(request, "/favicon.ico"))
    {
      request->send_P(200, "image/x-icon", favicon, 156);
    }
  });
  
  server.on("/sliders", HTTP_GET, [](AsyncWebServerRequest *request){
    serveIndex(request);
  });
  
  server.on("/welcome", HTTP_GET, [](AsyncWebServerRequest *request){
    serveSettings(request);
  });
  
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    serveMessage(request, 200,"Rebooting now...","Please wait ~10 seconds...",129);
    doReboot = true;
  });
  
  server.on("/settings/wifi", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!(wifiLock && otaLock)) handleSettingsSet(request, 1);
    serveMessage(request, 200,"WiFi settings saved.","Rebooting now...",255);
    doReboot = true;
  });

  server.on("/settings/leds", HTTP_POST, [](AsyncWebServerRequest *request){
    handleSettingsSet(request, 2);
    serveMessage(request, 200,"LED settings saved.","Redirecting...",1);
  });

  server.on("/settings/ui", HTTP_POST, [](AsyncWebServerRequest *request){
    handleSettingsSet(request, 3);
    serveMessage(request, 200,"UI settings saved.","Reloading to apply theme...",122);
  });

  server.on("/settings/sync", HTTP_POST, [](AsyncWebServerRequest *request){
    handleSettingsSet(request, 4);
    serveMessage(request, 200,"Sync settings saved.","Redirecting...",1);
  });

  server.on("/settings/time", HTTP_POST, [](AsyncWebServerRequest *request){
    handleSettingsSet(request, 5);
    serveMessage(request, 200,"Time settings saved.","Redirecting...",1);
  });

  server.on("/settings/sec", HTTP_POST, [](AsyncWebServerRequest *request){
    handleSettingsSet(request, 6);
    if (!doReboot) serveMessage(request, 200,"Security settings saved.","Rebooting now, please wait ~10 seconds...",129);
    doReboot = true;
  });

  server.on("/json", HTTP_GET, [](AsyncWebServerRequest *request){
    serveJson(request);
  });

  AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler("/json", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject root = json.to<JsonObject>();
    if (root.isNull()){request->send(500, "application/json", "{\"error\":\"Parsing failed\"}"); return;}
    if (deserializeState(root)) { serveJson(request); return; } //if JSON contains "v" (verbose response)
    request->send(200, "application/json", "{\"success\":true}");
  });
  server.addHandler(handler);

  //*******DEPRECATED*******
  server.on("/version", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", (String)VERSION);
    });
    
  server.on("/uptime", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", (String)millis());
    });
    
  server.on("/freeheap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", (String)ESP.getFreeHeap());
    });
  //*******END*******/
  
  server.on("/u", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", PAGE_usermod);
    });
    
  server.on("/teapot", HTTP_GET, [](AsyncWebServerRequest *request){
    serveMessage(request, 418, "418. I'm a teapot.", "(Tangible Embedded Advanced Project Of Twinkling)", 254);
    });
    
  //if OTA is allowed
  if (!otaLock){
    #if !defined WLED_DISABLE_FILESYSTEM && defined WLED_ENABLE_FS_EDITOR
     #ifdef ARDUINO_ARCH_ESP32
      server.addHandler(new SPIFFSEditor(SPIFFS));//http_username,http_password));
     #else
      server.addHandler(new SPIFFSEditor());//http_username,http_password));
     #endif
    #else
    server.on("/edit", HTTP_GET, [](AsyncWebServerRequest *request){
      serveMessage(request, 501, "Not implemented", "The SPIFFS editor is disabled in this build.", 254);
    });
    #endif
    //init ota page
    #ifndef WLED_DISABLE_OTA
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
      olen = 0;
      getCSSColors();
      request->send_P(200, "text/html", PAGE_update, msgProcessor);
    });
    
    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
      if (Update.hasError())
      {
        serveMessage(request, 500, "Failed updating firmware!", "Please check your file and retry!", 254); return;
      }
      serveMessage(request, 200, "Successfully updated firmware!", "Please wait while the module reboots...", 131); 
      doReboot = true;
    },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
      if(!index){
        DEBUG_PRINTLN("OTA Update Start");
        #ifndef ARDUINO_ARCH_ESP32
        Update.runAsync(true);
        #endif
        Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000);
      }
      if(!Update.hasError()) Update.write(data, len);
      if(final){
        if(Update.end(true)){
          DEBUG_PRINTLN("Update Success");
        } else {
          DEBUG_PRINTLN("Update Failed");
        }
      }
    });
    
    #else
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
      serveMessage(request, 501, "Not implemented", "OTA updates are disabled in this build.", 254);
    });
    #endif
  } else
  {
    server.on("/edit", HTTP_GET, [](AsyncWebServerRequest *request){
      serveMessage(request, 500, "Access Denied", "Please unlock OTA in security settings!", 254);
    });
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
      serveMessage(request, 500, "Access Denied", "Please unlock OTA in security settings!", 254);
    });
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    serveIndexOrWelcome(request);
  });
  
  //called when the url is not defined here, ajax-in; get-settings
  server.onNotFound([](AsyncWebServerRequest *request){
    DEBUG_PRINTLN("Not-Found HTTP call:");
    DEBUG_PRINTLN("URI: " + request->url());

    //make API CORS compatible
    if (request->method() == HTTP_OPTIONS)
    {
      request->send(200); return;
    }
    
    if(handleSet(request, request->url())) return;
    #ifndef WLED_DISABLE_ALEXA
    if(espalexa.handleAlexaApiCall(request)) return;
    #endif
    #ifdef WLED_ENABLE_FS_SERVING
    if(handleFileRead(request, request->url())) return;
    #endif
    request->send(404, "text/plain", "Not Found");
  });
}


void serveIndexOrWelcome(AsyncWebServerRequest *request)
{
  if (!showWelcomePage){
    serveIndex(request);
  } else {
    serveSettings(request);
  }
}


void getCSSColors()
{
  char cs[6][9];
  getThemeColors(cs);
  oappend("<style>:root{--aCol:#"); oappend(cs[0]);
  oappend(";--bCol:#");             oappend(cs[1]);
  oappend(";--cCol:#");             oappend(cs[2]);
  oappend(";--dCol:#");             oappend(cs[3]);
  oappend(";--sCol:#");             oappend(cs[4]);
  oappend(";--tCol:#");             oappend(cs[5]);
  oappend(";--cFn:");               oappend(cssFont);
  oappend(";}");
}


void serveIndex(AsyncWebServerRequest* request)
{
  bool serveMobile = false;
  if (uiConfiguration == 0 && request->hasHeader("User-Agent")) serveMobile = checkClientIsMobile(request->getHeader("User-Agent")->value());
  else if (uiConfiguration == 2) serveMobile = true;

  #ifdef WLED_ENABLE_FS_SERVING
  if (serveMobile)
  {
    if (handleFileRead(request, "/index_mobile.htm")) return;
  } else
  {
    if (handleFileRead(request, "/index.htm")) return;
  }
  #endif

  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", 
                                      (serveMobile) ? (uint8_t*)PAGE_indexM : PAGE_index,
                                      (serveMobile) ? PAGE_indexM_L : PAGE_index_L);

  //error message is not gzipped
  #ifdef WLED_DISABLE_MOBILE_UI
  if (!serveMobile) response->addHeader("Content-Encoding","gzip");
  #else
  response->addHeader("Content-Encoding","gzip");
  #endif
  
  request->send(response);
}


String msgProcessor(const String& var)
{
  if (var == "CSS") {
    char css[512];
    obuf = css;
    olen = 0;
    getCSSColors();
    return String(obuf);
  }
  if (var == "MSG") {
    String messageBody = messageHead;
    messageBody += "</h2>";
    messageBody += messageSub;
    uint32_t optt = optionType;

    if (optt < 60) //redirect to settings after optionType seconds
    {
      messageBody += "<script>setTimeout(RS," + String(optt*1000) + ")</script>";
    } else if (optt < 120) //redirect back after optionType-60 seconds, unused
    {
      //messageBody += "<script>setTimeout(B," + String((optt-60)*1000) + ")</script>";
    } else if (optt < 180) //reload parent after optionType-120 seconds
    {
      messageBody += "<script>setTimeout(RP," + String((optt-120)*1000) + ")</script>";
    } else if (optt == 253)
    {
      messageBody += "<br><br><form action=/settings><button class=\"bt\" type=submit>Back</button></form>"; //button to settings
    } else if (optt == 254)
    {
      messageBody += "<br><br><button type=\"button\" class=\"bt\" onclick=\"B()\">Back</button>";
    }
    return messageBody;
  }
  return String();
}


void serveMessage(AsyncWebServerRequest* request, uint16_t code, String headl, String subl="", byte optionT=255)
{
  #ifndef ARDUINO_ARCH_ESP32
  char buf[256];
  obuf = buf;
  #endif
  olen = 0;
  getCSSColors();
  messageHead = headl;
  messageSub = subl;
  optionType = optionT;
  
  request->send_P(code, "text/html", PAGE_msg, msgProcessor);
}


String settingsProcessor(const String& var)
{
  if (var == "CSS") {
    char buf[2048];
    getSettingsJS(optionType, buf);
    getCSSColors();
    return String(buf);
  }
  if (var == "SCSS") return String(PAGE_settingsCss);
  return String();
}


void serveSettings(AsyncWebServerRequest* request)
{
  byte subPage = 0;
  const String& url = request->url();
  if (url.indexOf("sett") >= 0) 
  {
    if      (url.indexOf("wifi") > 0) subPage = 1;
    else if (url.indexOf("leds") > 0) subPage = 2;
    else if (url.indexOf("ui")   > 0) subPage = 3;
    else if (url.indexOf("sync") > 0) subPage = 4;
    else if (url.indexOf("time") > 0) subPage = 5;
    else if (url.indexOf("sec")  > 0) subPage = 6;
  } else subPage = 255; //welcome page

  if (subPage == 1 && wifiLock && otaLock)
  {
    serveMessage(request, 500, "Access Denied", "Please unlock OTA in security settings!", 254); return;
  }
  
  #ifdef WLED_DISABLE_MOBILE_UI //disable welcome page if not enough storage
   if (subPage == 255) {serveIndex(request); return;}
  #endif

  optionType = subPage;
  
  switch (subPage)
  {
    case 1:   request->send_P(200, "text/html", PAGE_settings_wifi, settingsProcessor); break;
    case 2:   request->send_P(200, "text/html", PAGE_settings_leds, settingsProcessor); break;
    case 3:   request->send_P(200, "text/html", PAGE_settings_ui  , settingsProcessor); break;
    case 4:   request->send_P(200, "text/html", PAGE_settings_sync, settingsProcessor); break;
    case 5:   request->send_P(200, "text/html", PAGE_settings_time, settingsProcessor); break;
    case 6:   request->send_P(200, "text/html", PAGE_settings_sec , settingsProcessor); break;
    case 255: request->send_P(200, "text/html", PAGE_welcome      , settingsProcessor); break;
    default:  request->send_P(200, "text/html", PAGE_settings     , settingsProcessor); 
  }
}

#line 1 "/home/topota/Arduino/WLED/wled00/wled19_json.ino"
/*
 * JSON API (De)serialization
 */

bool deserializeState(JsonObject root)
{
  bool stateResponse = root["v"] | false;
  
  bri = root["bri"] | bri;
  
  bool on = root["on"] | (bri > 0);
  if (!on != !bri) toggleOnOff();
  
  if (root.containsKey("transition"))
  {
    transitionDelay = root["transition"];
    transitionDelay *= 100;
  }

  int ps = root["ps"] | -1;
  if (ps >= 0) applyPreset(ps);
  
  int cy = root["pl"] | -1;
  presetCyclingEnabled = (cy >= 0);

  JsonObject nl = root["nl"];
  nightlightActive    = nl["on"]   | nightlightActive;
  nightlightDelayMins = nl["dur"]  | nightlightDelayMins;
  nightlightFade      = nl["fade"] | nightlightFade;
  nightlightTargetBri = nl["tbri"] | nightlightTargetBri;

  JsonObject udpn = root["udpn"];
  notifyDirect         = udpn["send"] | notifyDirect;
  receiveNotifications = udpn["recv"] | receiveNotifications;
  bool noNotification  = udpn["nn"]; //send no notification just for this request

  int timein = root["time"] | -1;
  if (timein != -1) setTime(timein);

  int it = 0;
  JsonArray segs = root["seg"];
  for (JsonObject elem : segs)
  {
    byte id = elem["id"] | it;
    if (id < strip.getMaxSegments())
    {
      WS2812FX::Segment& seg = strip.getSegment(id);
      uint16_t start = elem["start"] | seg.start;
      int stop = elem["stop"] | -1;

      if (stop < 0) {
        uint16_t len = elem["len"];
        stop = (len > 0) ? start + len : seg.stop;
      }
      strip.setSegment(id, start, stop);
      
      JsonArray colarr = elem["col"];
      if (!colarr.isNull())
      {
        for (uint8_t i = 0; i < 3; i++)
        {
          JsonArray colX = colarr[i];
          if (colX.isNull()) break;
          byte sz = colX.size();
          if (sz > 0 && sz < 5)
          {
            int rgbw[] = {0,0,0,0};
            byte cp = copyArray(colX, rgbw);
            seg.colors[i] = ((rgbw[3] << 24) | ((rgbw[0]&0xFF) << 16) | ((rgbw[1]&0xFF) << 8) | ((rgbw[2]&0xFF)));
            if (cp == 1 && rgbw[0] == 0) seg.colors[i] = 0;
            if (id == 0) //temporary
            { 
              if (i == 0) {col[0] = rgbw[0]; col[1] = rgbw[1]; col[2] = rgbw[2]; col[3] = rgbw[3];}
              if (i == 1) {colSec[0] = rgbw[0]; colSec[1] = rgbw[1]; colSec[2] = rgbw[2]; colSec[3] = rgbw[3];}
            }
          }
        }
      }
      
      byte fx = elem["fx"] | seg.mode;
      if (fx != seg.mode && fx < strip.getModeCount()) strip.setMode(id, fx);
      seg.speed = elem["sx"] | seg.speed;
      seg.intensity = elem["ix"] | seg.intensity;
      seg.palette = elem["pal"] | seg.palette;
      //if (pal != seg.palette && pal < strip.getPaletteCount()) strip.setPalette(pal);
      seg.setOption(0, elem["sel"] | seg.getOption(0)); //selected
      seg.setOption(1, elem["rev"] | seg.getOption(1)); //reverse
      //int cln = seg_0["cln"];
      //temporary
      if (id == 0) {
        effectCurrent = seg.mode;
        effectSpeed = seg.speed;
        effectIntensity = seg.intensity;
        effectPalette = seg.palette;
      }
    }
    it++;
  }
  colorUpdated(noNotification ? 5:1);

  return stateResponse;
}

void serializeState(JsonObject root)
{
  root["on"] = (bri > 0);
  root["bri"] = briLast;
  root["transition"] = transitionDelay/100; //in 100ms

  root["ps"] = -1; //
  root["pl"] = (presetCyclingEnabled) ? 0: -1;
  
  JsonObject nl = root.createNestedObject("nl");
  nl["on"] = nightlightActive;
  nl["dur"] = nightlightDelayMins;
  nl["fade"] = nightlightFade;
  nl["tbri"] = nightlightTargetBri;
  
  JsonObject udpn = root.createNestedObject("udpn");
  udpn["send"] = notifyDirect;
  udpn["recv"] = receiveNotifications;
  
  JsonArray seg = root.createNestedArray("seg");
  for (byte s = 0; s < strip.getMaxSegments(); s++)
  {
    WS2812FX::Segment sg = strip.getSegment(s);
    if (sg.isActive())
    {
      JsonObject seg0 = seg.createNestedObject();
      serializeSegment(seg0, sg, s);
    }
  }
}

void serializeSegment(JsonObject& root, WS2812FX::Segment& seg, byte id)
{
  root["id"] = id;
  root["start"] = seg.start;
  root["stop"] = seg.stop;
  root["len"] = seg.stop - seg.start;
  
  JsonArray colarr = root.createNestedArray("col");

  for (uint8_t i = 0; i < 3; i++)
  {
    JsonArray colX = colarr.createNestedArray();
    colX.add((seg.colors[i] >> 16) & 0xFF);
    colX.add((seg.colors[i] >>  8) & 0xFF);
    colX.add((seg.colors[i]      ) & 0xFF);
    if (useRGBW)
    colX.add((seg.colors[i] >> 24) & 0xFF);
  }
  
  root["fx"] = seg.mode;
  root["sx"] = seg.speed;
  root["ix"] = seg.intensity;
  root["pal"] = seg.palette;
  root["sel"] = seg.isSelected();
  root["rev"] = seg.getOption(1);
  root["cln"] = -1;
}

void serializeInfo(JsonObject root)
{
  root["ver"] = versionString;
  root["vid"] = VERSION;
  
  JsonObject leds = root.createNestedObject("leds");
  leds["count"] = ledCount;
  leds["rgbw"] = useRGBW;
  JsonArray leds_pin = leds.createNestedArray("pin");
  leds_pin.add(LEDPIN);
  
  leds["pwr"] = strip.currentMilliamps;
  leds["maxpwr"] = strip.ablMilliampsMax;
  leds["maxseg"] = strip.getMaxSegments();
  
  root["name"] = serverDescription;
  root["udpport"] = udpPort;
  root["live"] = realtimeActive;
  root["fxcount"] = strip.getModeCount();
  root["palcount"] = strip.getPaletteCount();
  #ifdef ARDUINO_ARCH_ESP32
  root["arch"] = "esp32";
  root["core"] = ESP.getSdkVersion();
  //root["maxalloc"] = ESP.getMaxAllocHeap();
  #else
  root["arch"] = "esp8266";
  root["core"] = ESP.getCoreVersion();
  //root["maxalloc"] = ESP.getMaxFreeBlockSize();
  #endif
  root["freeheap"] = ESP.getFreeHeap();
  root["uptime"] = millis()/1000;
  
  byte os = 0;
  #ifdef WLED_DEBUG
  os  = 0x80;
  #endif
  #ifndef WLED_DISABLE_ALEXA
  os += 0x40;
  #endif
  #ifndef WLED_DISABLE_BLYNK
  os += 0x20;
  #endif
  #ifndef WLED_DISABLE_CRONIXIE
  os += 0x10;
  #endif
  #ifndef WLED_DISABLE_FILESYSTEM
  os += 0x08;
  #endif
  #ifndef WLED_DISABLE_HUESYNC
  os += 0x04;
  #endif
  #ifndef WLED_DISABLE_MOBILE_UI
  os += 0x02;
  #endif
  #ifndef WLED_DISABLE_OTA 
  os += 0x01;
  #endif
  root["opt"] = os;
  
  root["brand"] = "WLED";
  root["product"] = "DIY light";
  root["btype"] = "dev";
  root["mac"] = escapedMac;
}

void serveJson(AsyncWebServerRequest* request)
{
  byte subJson = 0;
  const String& url = request->url();
  if      (url.indexOf("state") > 0) subJson = 1;
  else if (url.indexOf("info")  > 0) subJson = 2;
  else if (url.indexOf("eff")   > 0) {
    request->send_P(200, "application/json", JSON_mode_names);
    return;
  }
  else if (url.indexOf("pal")   > 0) {
    request->send_P(200, "application/json", JSON_palette_names);
    return;
  }
  else if (url.length() > 6) { //not just /json
    request->send(  501, "application/json", "{\"error\":\"Not implemented\"}");
    return;
  }
  
  AsyncJsonResponse* response = new AsyncJsonResponse();
  JsonObject doc = response->getRoot();

  switch (subJson)
  {
    case 1: //state
      serializeState(doc); break;
    case 2: //info
      serializeInfo(doc); break;
    default: //all
      JsonObject state = doc.createNestedObject("state");
      serializeState(state);
      JsonObject info  = doc.createNestedObject("info");
      serializeInfo(info);
      doc["effects"]  = serialized((const __FlashStringHelper*)JSON_mode_names);
      doc["palettes"] = serialized((const __FlashStringHelper*)JSON_palette_names);
  }
  
  response->setLength();
  request->send(response);
}

#line 1 "/home/topota/Arduino/WLED/wled00/wled20_ir.ino"
/*
 * Infrared sensor support for generic 24 key RGB remote
 */

#if defined(WLED_DISABLE_INFRARED) || defined(ARDUINO_ARCH_ESP32)
void handleIR(){}
#else

IRrecv* irrecv;
//change pin in NpbWrapper.h

decode_results results;

unsigned long irCheckedTime = 0;
uint32_t lastValidCode = 0;
uint16_t irTimesRepeated = 0;


//Add what your custom IR codes should trigger here. Guide: https://github.com/Aircoookie/WLED/wiki/Infrared-Control
//IR codes themselves can be defined directly after "case" or in "ir_codes.h"
bool decodeIRCustom(uint32_t code)
{
  switch (code)
  {
    //just examples, feel free to modify or remove
    case IRCUSTOM_ONOFF : toggleOnOff(); break;
    case IRCUSTOM_MACRO1 : applyMacro(1); break;

    default: return false;
  }
  if (code != IRCUSTOM_MACRO1) colorUpdated(2); //don't update color again if we apply macro, it already does it
  return true;
}


//relatively change brightness, minumum A=5
void relativeChange(byte* property, int8_t amount, byte lowerBoundary =0)
{
  int16_t new_val = (int16_t) *property + amount;
  if (new_val > 0xFF) new_val = 0xFF;
  else if (new_val < lowerBoundary) new_val = lowerBoundary;
  *property = new_val;
}


void decodeIR(uint32_t code)
{
  if (code == 0xFFFFFFFF) //repeated code, continue brightness up/down
  {
    irTimesRepeated++;
    if (lastValidCode == IR24_BRIGHTER)
    { 
      relativeChange(&bri, 10); colorUpdated(2);
    }
    else if (lastValidCode == IR24_DARKER)
    {
      relativeChange(&bri, -10, 5); colorUpdated(2);
    }
    else if (lastValidCode == IR24_ON && irTimesRepeated > 7)
    {
      nightlightActive = true;
      nightlightStartTime = millis();
      colorUpdated(2);
    }
    return;
  }
  lastValidCode = 0; irTimesRepeated = 0;

  if (decodeIRCustom(code)) return;
  if      (code > 0xFFFFFF) return; //invalid code
  else if (code > 0xFF0000) decodeIR44(code); //is in 44-key remote range
  else if (code > 0xF70000 && code < 0xF80000) decodeIR24(code); //is in 24-key remote range
  //code <= 0xF70000 also invalid
}


void decodeIR24(uint32_t code)
{
  switch (code) {
    case IR24_BRIGHTER  : relativeChange(&bri, 10);         break;
    case IR24_DARKER    : relativeChange(&bri, -10, 5);     break;
    case IR24_OFF       : briLast = bri; bri = 0;           break;
    case IR24_ON        : bri = briLast;                    break;
    case IR24_RED       : colorFromUint32(COLOR_RED);       break;
    case IR24_REDDISH   : colorFromUint32(COLOR_REDDISH);   break;
    case IR24_ORANGE    : colorFromUint32(COLOR_ORANGE);    break;
    case IR24_YELLOWISH : colorFromUint32(COLOR_YELLOWISH); break;
    case IR24_YELLOW    : colorFromUint32(COLOR_YELLOW);    break;
    case IR24_GREEN     : colorFromUint32(COLOR_GREEN);     break;
    case IR24_GREENISH  : colorFromUint32(COLOR_GREENISH);  break;
    case IR24_TURQUOISE : colorFromUint32(COLOR_TURQUOISE); break;
    case IR24_CYAN      : colorFromUint32(COLOR_CYAN);      break;
    case IR24_AQUA      : colorFromUint32(COLOR_AQUA);      break;
    case IR24_BLUE      : colorFromUint32(COLOR_BLUE);      break;
    case IR24_DEEPBLUE  : colorFromUint32(COLOR_DEEPBLUE);  break;
    case IR24_PURPLE    : colorFromUint32(COLOR_PURPLE);    break;
    case IR24_MAGENTA   : colorFromUint32(COLOR_MAGENTA);   break;
    case IR24_PINK      : colorFromUint32(COLOR_PINK);      break;
    case IR24_WHITE     : colorFromUint32(COLOR_WHITE);           effectCurrent = 0;  break;
    case IR24_FLASH     : if (!applyPreset(1)) effectCurrent = FX_MODE_COLORTWINKLE;  break;
    case IR24_STROBE    : if (!applyPreset(2)) effectCurrent = FX_MODE_RAINBOW_CYCLE; break;
    case IR24_FADE      : if (!applyPreset(3)) effectCurrent = FX_MODE_BREATH;        break;
    case IR24_SMOOTH    : if (!applyPreset(4)) effectCurrent = FX_MODE_RAINBOW;       break;
    default: return;
  }
  lastValidCode = code;
  colorUpdated(2); //for notifier, IR is considered a button input
}


void decodeIR44(uint32_t code)
{
  //not implemented for now
}


void initIR()
{
  if (irEnabled)
  {
    irrecv = new IRrecv(IR_PIN);
    irrecv->enableIRIn();
  }
}


void handleIR()
{
  if (irEnabled && millis() - irCheckedTime > 120)
  {
    irCheckedTime = millis();
    if (irEnabled)
    {
      if (irrecv == NULL)
      { 
        initIR(); return;
      }
      
      if (irrecv->decode(&results))
      {
        Serial.print("IR recv\r\n0x");
        Serial.println((uint32_t)results.value, HEX);
        Serial.println();
        decodeIR(results.value);
        irrecv->resume();
      }
    } else if (irrecv != NULL)
    {
      irrecv->disableIRIn();
      delete irrecv; irrecv = NULL;
    }
  }
}

#endif


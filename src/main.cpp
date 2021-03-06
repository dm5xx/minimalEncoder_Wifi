// Use from 0 to 4. Higher number, more debugging messages and memory usage.
#define _WIFIMGR_LOGLEVEL_    3
//#define LDEBUG

#include <FS.h>

//Ported to ESP32
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

  //needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>

// From v1.1.0
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;

#include <LittleFS.h>
FS* filesystem = &LittleFS;
#define FileFS    LittleFS
#define FS_Name       "LittleFS"

#define ESP_getChipId()   (ESP.getChipId())

#define LED_ON      LOW
#define LED_OFF     HIGH

// Pin D2 mapped to pin GPIO2/ADC12 of ESP32, or GPIO2/TXD1 of NodeMCU control on-board LED
#define PIN_LED       D4

// Now support ArduinoJson 6.0.0+ ( tested with v6.14.1 )
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
//#include <Arduino.h>
#include "defines.h"
#include <WebSockets2_Generic.h>

char configFileName[] = "/config.json";

// You only need to format the filesystem once
//#define FORMAT_FILESYSTEM       true
#define FORMAT_FILESYSTEM         false

// SSID and PW for your Router
String Router_SSID;
String Router_Pass;

// From v1.1.0
#define MIN_AP_PASSWORD_SIZE    8

#define SSID_MAX_LEN            32
//From v1.0.10, WPA2 passwords can be up to 63 characters long.
#define PASS_MAX_LEN            64

typedef struct
{
  char wifi_ssid[SSID_MAX_LEN];
  char wifi_pw  [PASS_MAX_LEN];
}  WiFi_Credentials;

typedef struct
{
  String wifi_ssid;
  String wifi_pw;
}  WiFi_Credentials_String;

#define NUM_WIFI_CREDENTIALS      2

typedef struct
{
  WiFi_Credentials  WiFi_Creds [NUM_WIFI_CREDENTIALS];
} WM_Config;

WM_Config         WM_config;

#define  CONFIG_FILENAME              F("/wifi_cred.dat")

bool initialConfig = false;

String AP_SSID;
String AP_PASS;

// Use false if you don't like to display Available Pages in Information Page of Config Portal
// Comment out or use true to display Available Pages in Information Page of Config Portal
// Must be placed before #include <ESP_WiFiManager.h>
#define USE_AVAILABLE_PAGES     false

// From v1.0.10 to permit disable/enable StaticIP configuration in Config Portal from sketch. Valid only if DHCP is used.
// You'll loose the feature of dynamically changing from DHCP to static IP, or vice versa
// You have to explicitly specify false to disable the feature.
//#define USE_STATIC_IP_CONFIG_IN_CP          false

// Use false to disable NTP config. Advisable when using Cellphone, Tablet to access Config Portal.
// See Issue 23: On Android phone ConfigPortal is unresponsive (https://github.com/khoih-prog/ESP_WiFiManager/issues/23)
#define USE_ESP_WIFIMANAGER_NTP     false

// Use true to enable CloudFlare NTP service. System can hang if you don't have Internet access while accessing CloudFlare
// See Issue #21: CloudFlare link in the default portal (https://github.com/khoih-prog/ESP_WiFiManager/issues/21)
#define USE_CLOUDFLARE_NTP          false

// New in v1.0.11
#define USING_CORS_FEATURE          true
//////

// Use USE_DHCP_IP == true for dynamic DHCP IP, false to use static IP which you have to change accordingly to your network
#if (defined(USE_STATIC_IP_CONFIG_IN_CP) && !USE_STATIC_IP_CONFIG_IN_CP)
  // Force DHCP to be true
  #if defined(USE_DHCP_IP)
    #undef USE_DHCP_IP
  #endif
  #define USE_DHCP_IP     true
#else
  // You can select DHCP or Static IP here
  #define USE_DHCP_IP     true
  //#define USE_DHCP_IP     false
#endif

#if ( USE_DHCP_IP || ( defined(USE_STATIC_IP_CONFIG_IN_CP) && !USE_STATIC_IP_CONFIG_IN_CP ) )
// Use DHCP
  #warning Using DHCP IP
  IPAddress stationIP   = IPAddress(0, 0, 0, 0);
  IPAddress gatewayIP   = IPAddress(192, 168, 2, 1);
  IPAddress netMask     = IPAddress(255, 255, 255, 0);
#else
  // Use static IP
  #warning Using static IP
  
  #ifdef ESP32
    IPAddress stationIP   = IPAddress(192, 168, 2, 232);
  #else
    IPAddress stationIP   = IPAddress(192, 168, 2, 186);
  #endif
  
  IPAddress gatewayIP   = IPAddress(192, 168, 2, 1);
  IPAddress netMask     = IPAddress(255, 255, 255, 0);
#endif

#define USE_CONFIGURABLE_DNS      true

IPAddress dns1IP      = gatewayIP;
IPAddress dns2IP      = IPAddress(8, 8, 8, 8);

#include <ESP_WiFiManager.h>              //https://github.com/khoih-prog/ESP_WiFiManager

//define your default values here, if there are different values in configFileName (config.json), they are overwritten.
#define WEBSOCKET_SERVER_LEN                64
#define WEBSOCKET_DEVICE_LEN                 3
#define WEBSOCKET_SERVER_PORT_LEN           6
#define WEBSOCKET_MODE_LEN             3
#define WEBSOCKET_ID_LEN            3

char websocket_server [WEBSOCKET_SERVER_LEN]        = "ultra";
char websocket_port   [WEBSOCKET_SERVER_PORT_LEN]   = "2999";
char websocket_device  [WEBSOCKET_DEVICE_LEN]         = "D1";
char websocket_mode  [WEBSOCKET_MODE_LEN] = "C";
char websocket_id    [WEBSOCKET_ID_LEN]    = "0";

char dest[12] = "\0";

// Function Prototypes
uint8_t connectMultiWiFi(void);

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback(void)
{
#ifdef LDEBUG
  Serial.println("Should save config");
#endif
  shouldSaveConfig = true;
}

bool loadFileFSConfigFile(void)
{
  //clean FS, for testing
  //FileFS.format();

  //read configuration from FS json
#ifdef LDEBUG
  Serial.println("Mounting FS...");
#endif

  if (FileFS.begin())
  {
#ifdef LDEBUG
    Serial.println("Mounted file system");
#endif

    if (FileFS.exists(configFileName))
    {
      //file exists, reading and loading
#ifdef LDEBUG
      Serial.println("Reading config file");
#endif
      File configFile = FileFS.open(configFileName, "r");

      if (configFile)
      {
#ifdef LDEBUG
        Serial.print("Opened config file, size = ");
#endif
        size_t configFileSize = configFile.size();
#ifdef LDEBUG
        Serial.println(configFileSize);
#endif

        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[configFileSize + 1]);

        configFile.readBytes(buf.get(), configFileSize);

#ifdef LDEBUG
        Serial.print("\nJSON parseObject() result : ");
#endif

        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get(), configFileSize);

        if ( deserializeError )
        {
#ifdef LDEBUG
          Serial.println("failed");
#endif
          return false;
        }
        else
        {
#ifdef LDEBUG
          Serial.println("OK");
#endif

          if (json["websocket_server"])
            strncpy(websocket_server, json["websocket_server"], sizeof(websocket_server));
         
          if (json["websocket_port"])
            strncpy(websocket_port, json["websocket_port"], sizeof(websocket_port));
 
          if (json["websocket_device"])
            strncpy(websocket_device,  json["websocket_device"], sizeof(websocket_device));

          if (json["websocket_mode"])
            strncpy(websocket_mode, json["websocket_mode"], sizeof(websocket_mode));

          if (json["websocket_id"])  
            strncpy(websocket_id,   json["websocket_id"], sizeof(websocket_id));
        }

        //serializeJson(json, Serial);
        serializeJsonPretty(json, Serial);

        configFile.close();
      }
    }
  }
  else
  {
#ifdef LDEBUG
    Serial.println("failed to mount FS");
#endif
    return false;
  }
  return true;
}

bool saveFileFSConfigFile(void)
{
#ifdef LDEBUG
  Serial.println("Saving config");
#endif
  DynamicJsonDocument json(1024);

  json["websocket_server"] = websocket_server;
  json["websocket_port"]   = websocket_port;
  json["websocket_device"]  = websocket_device;

  json["websocket_mode"] = websocket_mode;
  json["websocket_id"]   = websocket_id;

  File configFile = FileFS.open(configFileName, "w");

#ifdef LDEBUG
  if (!configFile)
  {
   Serial.println("Failed to open config file for writing");
  }
#endif

  //serializeJson(json, Serial);
#ifdef LDEBUG
  serializeJsonPretty(json, Serial);
#endif
  // Write data to file and close it
  serializeJson(json, configFile);
  configFile.close();
  //end save
}

void heartBeatPrint(void)
{
  static int num = 1;


#ifdef LDEBUG
  if (WiFi.status() == WL_CONNECTED)
    Serial.print("H");        // H means connected to WiFi
  else
    Serial.print("F");        // F means not connected to WiFi
#endif

  if (num == 80)
  {
#ifdef LDEBUG
    Serial.println();
#endif
    num = 1;
  }
  else if (num++ % 10 == 0)
  {
#ifdef LDEBUG
    Serial.print(" ");
#endif
  }
}

void toggleLED()
{
  //toggle state
#ifdef LDEBUG
  Serial.print("Toggle");        // H means connected to WiFi
#endif
  digitalWrite(PIN_LED, !digitalRead(PIN_LED));
}

void check_WiFi(void)
{
  if ( (WiFi.status() != WL_CONNECTED) )
  {
#ifdef LDEBUG
    Serial.println("\nWiFi lost. Call connectMultiWiFi in loop");
#endif
    connectMultiWiFi();
  }
}  

void check_status()
{
  static ulong checkstatus_timeout  = 0;
  static ulong LEDstatus_timeout    = 0;
  static ulong checkwifi_timeout    = 0;
  
  static ulong currentMillis;

#define HEARTBEAT_INTERVAL    10000L
#define LED_INTERVAL          2000L
#define WIFICHECK_INTERVAL    1000L

  currentMillis = millis();

  // Check WiFi every WIFICHECK_INTERVAL (1) seconds.
  if ((currentMillis > checkwifi_timeout) || (checkwifi_timeout == 0))
  {
    check_WiFi();
    checkwifi_timeout = currentMillis + WIFICHECK_INTERVAL;
  }

  if ((currentMillis > LEDstatus_timeout) || (LEDstatus_timeout == 0))
  {
    // Toggle LED at LED_INTERVAL = 2s
    toggleLED();
    LEDstatus_timeout = currentMillis + LED_INTERVAL;
  }

  // Print hearbeat every HEARTBEAT_INTERVAL (10) seconds.
  if ((currentMillis > checkstatus_timeout) || (checkstatus_timeout == 0))
  {
    heartBeatPrint();
    checkstatus_timeout = currentMillis + HEARTBEAT_INTERVAL;
  }
}

void loadConfigData(void)
{
  File file = FileFS.open(CONFIG_FILENAME, "r");
  LOGERROR(F("LoadWiFiCfgFile "));

  if (file)
  {
    file.readBytes((char *) &WM_config, sizeof(WM_config));
    file.close();
    LOGERROR(F("OK"));
  }
  else
  {
    LOGERROR(F("failed"));
  }
}
    
void saveConfigData(void)
{
  File file = FileFS.open(CONFIG_FILENAME, "w");
  LOGERROR(F("SaveWiFiCfgFile "));

  if (file)
  {
    file.write((uint8_t*) &WM_config, sizeof(WM_config));
    file.close();
    LOGERROR(F("OK"));
  }
  else
  {
    LOGERROR(F("failed"));
  }
}

uint8_t connectMultiWiFi(void)
{
  #define WIFI_MULTI_1ST_CONNECT_WAITING_MS       2200L

#define WIFI_MULTI_CONNECT_WAITING_MS           100L
  
  uint8_t status;

  LOGERROR(F("ConnectMultiWiFi with :"));
  
  if ( (Router_SSID != "") && (Router_Pass != "") )
  {
    LOGERROR3(F("* Flash-stored Router_SSID = "), Router_SSID, F(", Router_Pass = "), Router_Pass );
  }

  for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
  {
    // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
    if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
    {
      LOGERROR3(F("* Additional SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
    }
  }
  
  LOGERROR(F("Connecting MultiWifi..."));

  WiFi.mode(WIFI_STA);

#if !USE_DHCP_IP    
  #if USE_CONFIGURABLE_DNS  
    // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5
    WiFi.config(stationIP, gatewayIP, netMask, dns1IP, dns2IP);  
  #else
    // Set static IP, Gateway, Subnetmask, Use auto DNS1 and DNS2.
    WiFi.config(stationIP, gatewayIP, netMask);
  #endif 
#endif

  int i = 0;
  status = wifiMulti.run();
  delay(WIFI_MULTI_1ST_CONNECT_WAITING_MS);

  while ( ( i++ < 10 ) && ( status != WL_CONNECTED ) )
  {
    status = wifiMulti.run();

    if ( status == WL_CONNECTED )
      break;
    else
      delay(WIFI_MULTI_CONNECT_WAITING_MS);
  }

  if ( status == WL_CONNECTED )
  {
    LOGERROR1(F("WiFi connected after time: "), i);
    LOGERROR3(F("SSID:"), WiFi.SSID(), F(",RSSI="), WiFi.RSSI());
    LOGERROR3(F("Channel:"), WiFi.channel(), F(",IP address:"), WiFi.localIP() );
  }
  else
    LOGERROR(F("WiFi not connected"));

  return status;
}

using namespace websockets2_generic;

#define PIN_CLOCK D1
#define PIN_DATA D2
#define PIN_BUTTON D3 // interrupt 0


#define latchPin D8 
#define clockPin D5 
#define dataPin D7
 
WebsocketsClient client;
bool connected = false;

static uint8_t next = 0;
static uint16_t persist=0;

String device = "D0";
String mode = "C";
String profile = "0";

int8_t BankNr = -1;

int8_t requestValues() {
  static int8_t matrix[] = {0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0};
  next <<= 2;
  
  if(digitalRead(PIN_DATA)) 
    next |= 0x02;
  if(digitalRead(PIN_CLOCK)) 
    next |= 0x01;
  
  next &= 0x0f;

   if(matrix[next]) {
      persist <<= 4;
      persist |= next;
      if((persist&0xff)==0x2b) 
        return -1;
      if((persist&0xff)==0x17) 
        return 1;
   }
   return 0;
}

void myShiftOut(uint number)
{
    digitalWrite(latchPin, LOW);

    for(int x = 0; x < 16; x++)
    {
        digitalWrite(dataPin, (number >> x) & 1);
        digitalWrite(clockPin, 1);
        digitalWrite(clockPin, 0);
    }

    digitalWrite(latchPin, HIGH);        
}

void onEventsCallback(WebsocketsEvent event, String data) 
{
  if (event == WebsocketsEvent::ConnectionOpened) 
  {
#ifdef LDEBUG
    Serial.println(F("Connnection Opened"));
#endif
  } 
  else if (event == WebsocketsEvent::ConnectionClosed) 
  {
#ifdef LDEBUG
    Serial.println(F("Connnection Closed"));
#endif
  } 
  else if (event == WebsocketsEvent::GotPing) 
  {
#ifdef LDEBUG
    Serial.println(F("Got a Ping!"));
#endif
  } 
  else if (event == WebsocketsEvent::GotPong) 
  {
#ifdef LDEBUG
    Serial.println(F("Got a Pong!"));
#endif
  }
}

void warningLed()
{
    for(byte a = 0; a < 10; a++)
    {
        digitalWrite(PIN_LED, HIGH);
        delay(150);
        digitalWrite(PIN_LED, LOW);
        delay(150);
    }
}

// void ICACHE_RAM_ATTR Encode() { // ICACHE... must be placed befor every interrupt function in esp8266!

void setup() {
#ifdef LDEBUG
  delay(5000);
  Serial.begin(9600);
  Serial.println(F("Basic Encoder Test:"));
#endif
 //  pinMode(D3, INPUT_PULLUP);
 //  attachInterrupt(D3, handleKey, RISING);

  pinMode(PIN_CLOCK, INPUT);
  pinMode(PIN_CLOCK, INPUT_PULLUP);
  pinMode(PIN_DATA, INPUT);
  pinMode(PIN_DATA, INPUT_PULLUP);

  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  pinMode(D0, INPUT_PULLUP);
  pinMode(D4, OUTPUT);

#ifdef LDEBUG
  Serial.println("\nStarting ESP8266-Client on " + String(ARDUINO_BOARD));
  Serial.println(F(WEBSOCKETS2_GENERIC_VERSION));


  Serial.print("\nStarting AutoConnectWithFSParams using " + String(FS_Name));
  Serial.println(" on " + String(ARDUINO_BOARD));
  Serial.println("ESP_WiFiManager Version " + String(ESP_WIFIMANAGER_VERSION));
#endif

  if (FORMAT_FILESYSTEM) 
    FileFS.format();

  // Format FileFS if not yet
#ifdef ESP32
  if (!FileFS.begin(true))
#else
  if (!FileFS.begin())
#endif  
  {
#ifdef LDEBUG
   Serial.print(FS_Name);
   Serial.println(F(" failed! AutoFormatting."));
#endif
    
#ifdef ESP8266
    FileFS.format();
#endif
  }

  loadFileFSConfigFile();

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  ESP_WMParameter custom_websocket_server("websocket_server", "websocket_server", websocket_server, WEBSOCKET_SERVER_LEN + 1);
  ESP_WMParameter custom_websocket_port  ("websocket_port",   "websocket_port",   websocket_port,   WEBSOCKET_SERVER_PORT_LEN + 1);
  ESP_WMParameter custom_websocket_device ("websocket_device",  "websocket_device",  websocket_device,  WEBSOCKET_DEVICE_LEN + 1 );

  ESP_WMParameter custom_websocket_mode("websocket_mode", "websocket_mode", websocket_mode, WEBSOCKET_MODE_LEN + 1);
  ESP_WMParameter custom_websocket_id  ("websocket_id",   "websocket_id",   websocket_id,   WEBSOCKET_ID_LEN + 1);

  unsigned long startedAt = millis();
  
  // Use this to default DHCP hostname to ESP8266-XXXXXX or ESP32-XXXXXX
  //ESP_WiFiManager ESP_wifiManager;
  // Use this to personalize DHCP hostname (RFC952 conformed)
  ESP_WiFiManager ESP_wifiManager("AutoConnect-FSParams");

  //set config save notify callback
  ESP_wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  ESP_wifiManager.addParameter(&custom_websocket_server);
  ESP_wifiManager.addParameter(&custom_websocket_port);
  ESP_wifiManager.addParameter(&custom_websocket_device);

  ESP_wifiManager.addParameter(&custom_websocket_mode);
  ESP_wifiManager.addParameter(&custom_websocket_id);

  //reset settings - for testing
  //ESP_wifiManager.resetSettings();

  ESP_wifiManager.setDebugOutput(true);

  //set custom ip for portal
  ESP_wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 100, 1), IPAddress(192, 168, 100, 1), IPAddress(255, 255, 255, 0));

  //set minimum quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //ESP_wifiManager.setMinimumSignalQuality();
  ESP_wifiManager.setMinimumSignalQuality(-1);

  // From v1.0.10 only
  // Set config portal channel, default = 1. Use 0 => random channel from 1-13
  ESP_wifiManager.setConfigPortalChannel(0);
  //////
  
#if !USE_DHCP_IP    
  #if USE_CONFIGURABLE_DNS  
    // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5
    ESP_wifiManager.setSTAStaticIPConfig(stationIP, gatewayIP, netMask, dns1IP, dns2IP);  
  #else
    // Set static IP, Gateway, Subnetmask, Use auto DNS1 and DNS2.
    ESP_wifiManager.setSTAStaticIPConfig(stationIP, gatewayIP, netMask);
  #endif 
#endif  

  // New from v1.1.1
#if USING_CORS_FEATURE
  ESP_wifiManager.setCORSHeader("Your Access-Control-Allow-Origin");
#endif

  // We can't use WiFi.SSID() in ESP32 as it's only valid after connected.
  // SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
  // Have to create a new function to store in EEPROM/SPIFFS/LittleFS for this purpose
  Router_SSID = ESP_wifiManager.WiFi_SSID();
  Router_Pass = ESP_wifiManager.WiFi_Pass();

  //Remove this line if you do not want to see WiFi password printed
#ifdef LDEBUG
  Serial.println("\nStored: SSID = " + Router_SSID + ", Pass = " + Router_Pass);
#endif

  if (Router_SSID != "")
  {
    ESP_wifiManager.setConfigPortalTimeout(120); //If no access point name has been previously entered disable timeout.
#ifdef LDEBUG
    Serial.println("Got stored Credentials. Timeout 120s");
#endif
  }
  else
  {
#ifdef LDEBUG
    Serial.println("No stored Credentials. No timeout");
#endif
  }

  String chipID = String(ESP_getChipId(), HEX);
  chipID.toUpperCase();

  // SSID and PW for Config Portal
  AP_SSID = "ESP_" + chipID + "_AutoConnectAP";
  AP_PASS = "MyESP_" + chipID;

  // From v1.1.0, Don't permit NULL password
  if ( (Router_SSID == "") || (Router_Pass == "") )
  {
#ifdef LDEBUG
    Serial.println("We haven't got any access point credentials, so get them now");
#endif

    initialConfig = true;

    // Starts an access point
    //if (!ESP_wifiManager.startConfigPortal((const char *) ssid.c_str(), password))
    if ( !ESP_wifiManager.startConfigPortal(AP_SSID.c_str(), AP_PASS.c_str()) )
    {
#ifdef LDEBUG
      Serial.println("Not connected to WiFi but continuing anyway.");
#endif
    } 
    else 
    {
#ifdef LDEBUG
      Serial.println("WiFi connected...yeey :)");
#endif
    }
    // Stored  for later usage, from v1.1.0, but clear first
    memset(&WM_config, 0, sizeof(WM_config));
    
    for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
    {
      String tempSSID = ESP_wifiManager.getSSID(i);
      String tempPW   = ESP_wifiManager.getPW(i);
  
      if (strlen(tempSSID.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1)
        strcpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str());
      else
        strncpy(WM_config.WiFi_Creds[i].wifi_ssid, tempSSID.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_ssid) - 1);

      if (strlen(tempPW.c_str()) < sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1)
        strcpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str());
      else
        strncpy(WM_config.WiFi_Creds[i].wifi_pw, tempPW.c_str(), sizeof(WM_config.WiFi_Creds[i].wifi_pw) - 1);  

      // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
      if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
      {
        LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
        wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
      }
    }

    saveConfigData();
  }
  else
  {
    wifiMulti.addAP(Router_SSID.c_str(), Router_Pass.c_str());
  }

  startedAt = millis();

  if (!initialConfig)
  {
    // Load stored data, the addAP ready for MultiWiFi reconnection
    loadConfigData();

    for (uint8_t i = 0; i < NUM_WIFI_CREDENTIALS; i++)
    {
      // Don't permit NULL SSID and password len < MIN_AP_PASSWORD_SIZE (8)
      if ( (String(WM_config.WiFi_Creds[i].wifi_ssid) != "") && (strlen(WM_config.WiFi_Creds[i].wifi_pw) >= MIN_AP_PASSWORD_SIZE) )
      {
        LOGERROR3(F("* Add SSID = "), WM_config.WiFi_Creds[i].wifi_ssid, F(", PW = "), WM_config.WiFi_Creds[i].wifi_pw );
        wifiMulti.addAP(WM_config.WiFi_Creds[i].wifi_ssid, WM_config.WiFi_Creds[i].wifi_pw);
      }
    }

    if ( WiFi.status() != WL_CONNECTED ) 
    {
#ifdef LDEBUG
      Serial.println("ConnectMultiWiFi in setup");
#endif
     
      connectMultiWiFi();
    }
  }

#ifdef LDEBUG
  Serial.print("After waiting ");
  Serial.print((float) (millis() - startedAt) / 1000L);
  Serial.print(" secs more in setup(), connection result is ");
#endif


#ifdef LDEBUG
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("connected. Local IP: ");
    Serial.println(WiFi.localIP());
  }
  else
    Serial.println(ESP_wifiManager.getStatus(WiFi.status()));
#endif

  //read updated parameters
  strncpy(websocket_server, custom_websocket_server.getValue(), sizeof(websocket_server));
  strncpy(websocket_port,   custom_websocket_port.getValue(),   sizeof(websocket_port));
  strncpy(websocket_device,  custom_websocket_device.getValue(),  sizeof(websocket_device));

  strncpy(websocket_mode, custom_websocket_mode.getValue(), sizeof(websocket_mode));
  strncpy(websocket_id, custom_websocket_id.getValue(),     sizeof(websocket_id));

  //save the custom parameters to FS
  if (shouldSaveConfig)
  {
    saveFileFSConfigFile();
  }

  // Check if connected to wifi
  if (WiFi.status() != WL_CONNECTED) 
  {
#ifdef LDEBUG
    Serial.println(F("No Wifi!"));
#endif
    return;
  }

#ifdef LDEBUG
  Serial.print(F("Connected to Wifi, Connecting to WebSockets Server @"));
  Serial.println(websockets_server_host);
#endif
 
    strcat(dest, "/");
    strcat(dest, websocket_device);
    strcat(dest, "/");
    strcat(dest, websocket_mode);
    strcat(dest, "/");
    strcat(dest, websocket_id);

   while (!connected)
   {
#ifdef LDEBUG
     Serial.print(F("."));  
     Serial.println(F("Trying to connect..."));
#endif
     connected = client.connect(websocket_server, atoi(websocket_port), dest);

     if(!connected)
         warningLed();
   }


  if (connected) 
  {
#ifdef LDEBUG
    Serial.println(F("Connected!"));
    client.send("Ping");
#endif
  } 
  else 
  {
#ifdef LDEBUG
    Serial.println(F("Not Connected!"));
#endif
  }

#ifdef LDEBUG
  Serial.println(F("WiFi connected"));  
  Serial.println(F("IP address: "));
  Serial.println(WiFi.localIP());
#endif

byte counter = 0;

while(counter < 5)
{
    int sensorVal = digitalRead(D0);
    
    if (sensorVal == LOW) {
          
          Serial.println(F("ESR rest!"));
          ESP_wifiManager.resetSettings();
          WiFi.disconnect();
          delay(1000);
          Serial.println(F("ESP restarting..."));
          ESP.restart();
    }
    else
    {
      Serial.println(F("NotPushed!"));
    }
    
    delay(500);
    counter++;
}


  // run callback when messages are received
  client.onMessage([&](WebsocketsMessage message) 
  {
    static DynamicJsonDocument doc(200);
    String msg = message.data();    
    DeserializationError error = deserializeJson(doc,  msg);

    if (error) {
#ifdef LDEBUG
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
#endif
        return;
    }

    String bn = "B";
    bn.concat(BankNr);
        
    if(doc.containsKey("Bank"))
        BankNr = doc["Bank"].as<int>();
    else if(doc.containsKey(bn))
    {
        auto number=doc[bn].as<uint>();
        myShiftOut(number);
    }
  });

  // run callback when events are occuring
  client.onEvent(onEventsCallback);
  client.send("GetConfig");
}


void loop() {
    static int8_t value;

    check_status();

    if (client.available()) 
    {
        client.poll();
        value=requestValues();

        if( value!=0 )
        {
            if ( next==0x0b) 
                client.send("0");
            
            if ( next==0x07)
                client.send("1");
        }
    }
    else
    {
        connected = false;

        while (!connected)
        {
#ifdef LDEBUG
            Serial.print(F("."));        
            Serial.println(F("Trying to reconnect!!"));
#endif
          connected = client.connect(websocket_server, atoi(websocket_port), dest);

         if(!connected)
             warningLed();
       }
#ifdef LDEBUG
        Serial.println(F("RECONNECTED!!"));
#endif
    }
}
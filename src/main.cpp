#include <Arduino.h>
#include <ArduinoJson.h>
#include "defines.h"
#include <WebSockets2_Generic.h>

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
    Serial.println("Connnection Opened");
  } 
  else if (event == WebsocketsEvent::ConnectionClosed) 
  {
    Serial.println("Connnection Closed");
  } 
  else if (event == WebsocketsEvent::GotPing) 
  {
    Serial.println("Got a Ping!");
  } 
  else if (event == WebsocketsEvent::GotPong) 
  {
    Serial.println("Got a Pong!");
  }
}


// void ICACHE_RAM_ATTR Encode() { // ICACHE... must be placed befor every interrupt function in esp8266!

void setup() {
  Serial.begin(9600);
  Serial.println("Basic Encoder Test:");
 //  pinMode(D3, INPUT_PULLUP);
 //  attachInterrupt(D3, handleKey, RISING);

  pinMode(PIN_CLOCK, INPUT);
  pinMode(PIN_CLOCK, INPUT_PULLUP);
  pinMode(PIN_DATA, INPUT);
  pinMode(PIN_DATA, INPUT_PULLUP);

  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);


  Serial.println("\nStarting ESP8266-Client on " + String(ARDUINO_BOARD));
  Serial.println(WEBSOCKETS2_GENERIC_VERSION);
  
  // Connect to wifi
  WiFi.begin(ssid, password);

  // Wait some time to connect to wifi
  for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) 
  {
    Serial.print(".");
    delay(1000);
  }

  // Check if connected to wifi
  if (WiFi.status() != WL_CONNECTED) 
  {
    Serial.println("No Wifi!");
    return;
  }

  Serial.print("Connected to Wifi, Connecting to WebSockets Server @");
  Serial.println(websockets_server_host);
 
   while (!connected)
   {
     delay(1000);
     Serial.print(".");
  
     Serial.println("Trying to connect...");
     connected = client.connect("192.168.97.213", 2999, "/"+device+"/"+mode+"/"+profile);
   }


  if (connected) 
  {
    Serial.println("Connected!");

    String WS_msg = String("Hello to Server from ") + BOARD_NAME;
    client.send("Ping");
  } 
  else 
  {
    Serial.println("Not Connected!");
  }


  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // run callback when messages are received
  client.onMessage([&](WebsocketsMessage message) 
  {
    static DynamicJsonDocument doc(200);

    Serial.print("Got Message: ");
    Serial.println(message.data());

    String msg = message.data();    
    DeserializationError error = deserializeJson(doc,  msg);

    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    auto number=doc["B4"].as<uint>();
    
    Serial.print("Value: ");
    Serial.println(number);
    myShiftOut(number);
  });

  // run callback when events are occuring
  client.onEvent(onEventsCallback);
}


void loop() {
    static int8_t value;

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
            delay(5000);
            Serial.print(".");
        
            Serial.println("Trying to reconnect!!");
            connected = client.connect("192.168.97.213", 2999, "/"+device+"/"+mode+"/"+profile);
        }
        Serial.println("RECONNECTED!!");
    }
}

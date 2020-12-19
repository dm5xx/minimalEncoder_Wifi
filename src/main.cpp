#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketClient.h>

#define PIN_CLOCK D1
#define PIN_DATA D2
#define PIN_BUTTON D3 // interrupt 0

WebSocketClient ws(false);
WiFiClient client;

const char* ssid = "mmmedia";
const char* password = "tp4004mmatrixx6";

static uint8_t next = 0;
static uint16_t persist=0;

String device = "D0";
String mode = "C";
String profile = "0";

int8_t requestValues() {
  static int8_t matrix[] = {0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0};

  next <<= 2;
  if (digitalRead(PIN_DATA)) next |= 0x02;
  if (digitalRead(PIN_CLOCK)) next |= 0x01;
  next &= 0x0f;

   if  (matrix[next] ) {
      persist <<= 4;
      persist |= next;
      if ((persist&0xff)==0x2b) return -1;
      if ((persist&0xff)==0x17) return 1;
   }
   return 0;
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

  Serial.printf("Connecting to %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  while (!ws.isConnected())
  {
    delay(1000);
    Serial.print(".");
    
    Serial.println("Trying to connect...");
    ws.connect("192.168.97.213", "/"+device+"/"+mode+"/"+profile, 2999);
  }

    Serial.print("Connected: ");
    Serial.println(ws.isConnected());

}

void loop() {
    static int8_t value;

    if (!ws.isConnected()) {
        Serial.println("Trying to connect...");
        ws.connect("192.168.97.213", "/"+device+"/"+mode+"/"+profile, 2999);
    }
    else 
    {
        value=requestValues();

        if( value!=0 )
        {
            if ( next==0x0b) 
                ws.send("0");
            
            if ( next==0x07)
                ws.send("1");
        }
    }
}

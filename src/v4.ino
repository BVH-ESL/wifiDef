//#include <Arduino.h>

#if defined(ESP8266)
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif
#include <Wire.h>
#include <MCP7940.h>
#include <SPI.h>
#include <SD.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsClient.h>
#include <Hash.h>
extern "C" {
#include<user_interface.h>
}

//MCP3202 init pin and variable
const uint8_t MCP_SS = 5;             //chip select MCP3202 at D1
const uint8_t MCP_NUM_CHANNELS = 4;
const uint32_t MCP_VREF = 3300UL;
int readADC( byte chan ) { // read data from MCP3208 at the specified channel
  unsigned int value = 0;
  byte wbyte;
  digitalWrite( MCP_SS, LOW );
  chan &= 0x7;
  wbyte = (1 << 2) | (1 << 1) | (chan >> 2); // start bit, single-ended, D2
  SPI.transfer( wbyte );
  wbyte = (chan << 6); // D1, D0
  value = SPI.transfer( wbyte );
  value = (value << 8) | SPI.transfer( 0x00 );
  digitalWrite( MCP_SS, HIGH );
  return (value & 0x0fff);
}

//SDModule init pin and variable
const uint8_t SD_SS = 4;              //chip select SDModule at D2
char fileNameSD[12];
File dataFileSD;
char bufSD[45];

RTC_MCP7940 rtc;
const int RTC_SQW = 15;               //D8
const int RTC_SCL = 0;                //D3
const int RTC_SDA = 2;                //D4
DateTime timeNow;

//init timer1 function type and variable
typedef void (*timercallback)(void);
int counter_micro_second = 0;

void countCounter()
{
  counter_micro_second++;
}

void resetCounter()
{
  timeNow = rtc.now();
  counter_micro_second = 0;
  // sprintf( bufSD, "%02u/%02u/%04u %02u:%02u:%02u:%04d",
  //      timeNow.month(),
  //      timeNow.day(),
  //      timeNow.year(),
  //      timeNow.hour(),
  //      timeNow.minute(),
  //      timeNow.second(),
  //      counter_micro_second
  //      );
  // Serial.println(bufSD);
}

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
String data_in = "";

#define USE_SERIAL Serial

int isDone = true;

void webSocketEvent(WStype_t type, uint8_t * payload, size_t lenght) {
  switch (type) {
    case WStype_DISCONNECTED:
      {
        // USE_SERIAL.printf("[WSc] Disconnected!\n");
      }
      break;
    case WStype_CONNECTED:
      {
        USE_SERIAL.printf("[WSc] Connected to url: %s\n",  payload);
        // send message to server when Connected
        webSocket.sendTXT("esp");
      }
      break;
    case WStype_TEXT:
      // USE_SERIAL.printf("[WSc] get text: %s\n", payload);
      data_in += (char *)payload;
      // USE_SERIAL.println(data_in);
      if (data_in == "stop") {
        dataFileSD.close();
        isDone = true;
        webSocket.sendTXT("close");
      } else if (data_in == "start") {
        DateTime now = rtc.now();
        sprintf( fileNameSD, "%02d%02d%02d%02d.txt",
                 now.month(),
                 now.day(),
                 now.hour(),
                 now.minute()
               );
        // set file in sd card
        dataFileSD.close();
        dataFileSD = SD.open(fileNameSD, FILE_WRITE);
        dataFileSD.println("MM/DD/YYYY HH:mm:ss:msms,ch01,ch02,ch03,ch04");
        isDone = false;
        webSocket.sendTXT("open");
      }
      data_in = "";
      // send message to server
      break;
    case WStype_BIN:
      // USE_SERIAL.printf("[WSc] get binary lenght: %u\n", lenght);
      // hexdump(payload, lenght);
      // send data to server
      // webSocket.sendBIN(payload, lenght);
      break;
  }
}

void setup() {
  // put your setup code here, to run once:
  USE_SERIAL.begin(115200);
  USE_SERIAL.setDebugOutput(true);
  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();
  //  DateTime now = rtc.now();

  //init mcpcs pin
  pinMode(MCP_SS, OUTPUT);
  digitalWrite(MCP_SS, HIGH);

  int status;
  WiFi.disconnect(true);
  status = WiFi.begin("eodLoggerControll2", "asdf1234");
  // status = WiFi.waitForConnectResult();

  // if (status != WL_CONNECTED) {
  //   Serial.println("Connection Failed");
  //   status = WiFi.waitForConnectResult();
  //    while (true) {}
  // }

  IPAddress ip(192, 168, 4, 3); // where xx is the desired IP Address
  IPAddress gateway(192, 168, 4, 1); // set gateway to match your network

  IPAddress subnet(255, 255, 255, 0); // set subnet mask to match your network
  WiFi.config(ip, gateway, subnet);
  for (uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
  }

  //setup dataString
  for (int i = 0; i < 45; i++)
  {
    bufSD[i] = 'a';
  }

  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_SS, 250000))
  {
    Serial.println("Crd Failed");
    return;
  }
  dataFileSD.close();

  //init rtc
  rtc.begin();
#if defined(ESP8266)
  Wire.begin(RTC_SDA, RTC_SCL);
#endif
  if (!rtc.isset()) {
    rtc.adjust(DateTime(2016, 9, 28, 12, 30, 0));
    rtc.init();
  }
  // rtc.adjust(DateTime(2016, 9, 28, 12, 30, 0));
  // rtc.init();
  //set sqwmode
  rtc.writeSqwPinMode(SquareWave1HZ);

  //set interrupt form RTC SQW pin for counter
  attachInterrupt(RTC_SQW, resetCounter, FALLING);

  // set iterrupt timer1
  timer1_isr_init();
  timer1_attachInterrupt(reinterpret_cast<timercallback>(countCounter));
  timer1_enable(TIM_DIV16, TIM_EDGE, 1);    //TIM_DIV16 -> 5MHz = 5 ticks/us, TIM_DIV1 -> 80MHz = 80 ticks/us
  timer1_write(500);                       //call interrupt after ... tick

  //connect controll ws server
  webSocket.begin("192.168.4.1", 8081);
  webSocket.onEvent(webSocketEvent);
}
// write value into sd card
void loop() {
  if (!isDone) {
    if (counter_micro_second % 250 == 0)
    {
      int ch, value;
      uint16_t milli_volt[MCP_NUM_CHANNELS];
      for (ch = 0; ch < MCP_NUM_CHANNELS; ch++)
      {
        value = readADC(ch);
        milli_volt[ch] = (uint16_t)((value * MCP_VREF) / 4096);
      }
      sprintf( bufSD, "%02u/%02u/%04u %02u:%02u:%02u:%04d,%04d,%04d,%04d,%04d",
               timeNow.month(),
               timeNow.day(),
               timeNow.year(),
               timeNow.hour(),
               timeNow.minute(),
               timeNow.second(),
               counter_micro_second,
               milli_volt[0],
               milli_volt[1],
               milli_volt[2],
               milli_volt[3]
             );
      dataFileSD.println(bufSD);
    }
  }
  webSocket.loop();
}

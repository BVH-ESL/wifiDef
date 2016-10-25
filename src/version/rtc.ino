#include <Arduino.h>

#if defined(ESP8266)
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif
#include <Wire.h>
#include <MCP7940.h>
#include <SPI.h>
#include <SD.h>

bool isDone = false;

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
// char fileNameSD[32];
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
int counter_second = 0;

void countCounter()
{
  counter_micro_second++;
}

void resetCounter()
{
  timeNow = rtc.now();
  counter_micro_second = 0;
  counter_second++;
  if(counter_second == 120){
    counter_second = 0;
    isDone = true;
  }
}

SqwPinMode modes[] = {OFF, ON, SquareWave1HZ, SquareWave4kHz, SquareWave8kHz, SquareWave32kHz};

void setup() {
  // put your setup code here, to run once:
  // set iterrupt timer1
  timer1_isr_init();
  timer1_attachInterrupt(reinterpret_cast<timercallback>(countCounter));
  timer1_enable(TIM_DIV16, TIM_EDGE, 1);    //TIM_DIV16 -> 5MHz = 5 ticks/us, TIM_DIV1 -> 80MHz = 80 ticks/us
  timer1_write(500);                       //call interrupt after ... ticks

  rtc.begin();
#if defined(ESP8266)
  Wire.begin(RTC_SDA, RTC_SCL);
#endif
  Serial.begin(115200);
  Serial.println();
  
  //set sqwmode
  rtc.writeSqwPinMode(SquareWave1HZ);
  //set interrupt form RTC SQW pin for counter
  attachInterrupt(RTC_SQW, resetCounter, FALLING);
  DateTime now = rtc.now();

  //setup dataString
  for (int i = 0; i < 45; i++)
  {
    bufSD[i] = 'a';
  }

  //init card
  // Serial.print("Initializing SD card...");
  // if (!SD.begin(SD_SS, 100000))
  // {
  //   Serial.println("Card Failed");
  //   return;
  // }
  // sprintf( fileNameSD, "%02d%02d%02d%02d.txt",
  //      now.month(),
  //      now.day(),
  //      now.hour(),
  //      now.minute()
  //    );
  //set file in sd card
  // dataFileSD.close();
  // dataFileSD = SD.open(fileNameSD, FILE_WRITE);
  // Serial.println("card initialized.");
  // dataFileSD.println("MM/DD/YYYY HH:mm:ss:msms,ch01,ch02,ch03,ch04");                      //write headder
  // Serial.println(fileNameSD);
  //init mcpcs pin
  // pinMode(MCP_SS, OUTPUT);
  // digitalWrite(MCP_SS, HIGH);
  // delay(5000);
}

void loop(){
  // if(!isDone){
    if (counter_micro_second % 500 == 0)
    {
  //     int ch, value;
  //     uint16_t milli_volt[MCP_NUM_CHANNELS];
  //     for (ch = 0; ch < MCP_NUM_CHANNELS; ch++)
  //     {
  //       value = readADC(ch);
  //       milli_volt[ch] = (uint16_t)((value * MCP_VREF) / 4096);
  //     }
      sprintf( bufSD, "%02u/%02u/%04u %02u:%02u:%02u:%04d",
           timeNow.month(),
           timeNow.day(),
           timeNow.year(),
           timeNow.hour(),
           timeNow.minute(),
           timeNow.second(),
           counter_micro_second
           );
       Serial.println(bufSD);
  //      dataFileSD.println(bufSD);
  //  }
  // }else{
  //   dataFileSD.close();
  }
}

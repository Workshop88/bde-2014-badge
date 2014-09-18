#include <Wire.h>
#include <SPI.h>
#include <RTClib.h>
#include <SD.h>
#include <RH_ASK.h>
#include <Adafruit_SI1145.h>

Sd2Card card;
SdVolume volume;
SdFile root;

const int sd_cs = 4;

RTC_DS1307 rtc;

Adafruit_SI1145 sensor = Adafruit_SI1145();

int load_complete = 1; // Assume success

RH_ASK radio;

void setup()
{
  Wire.begin();
  rtc.begin();
  
  /* Needs to be set to OUTPUT for SD card to work */
  pinMode(SS, OUTPUT);
  
  if(!card.init(SPI_HALF_SPEED, sd_cs))
  {
    load_complete = 0;
  }
  
  if(!volume.init(card))
  {
    load_complete = 0;
  }
  
  root.openRoot(volume);
  
  if(!sensor.begin())
  {
    load_complete = 0;
  }
  
  if(!radio.init())
  {
     load_complete = 0; 
  }

}

void loop()
{
  const char *msg = "hello";
  uint8_t buff[256];
  uint8_t bufflen = 256;
  float vis_val = 0, ir_val = 0, uv_val = 0;
  
  vis_val = sensor.readVisible();
  uv_val  = sensor.readUV() / 100.0f;
  ir_val  = sensor.readIR();
  
  radio.send((uint8_t *)msg, strlen(msg));
  radio.waitPacketSent();
  
  if(radio.recv(buff, &bufflen))
  {
    /* We have data. */
  }
  
  DateTime dt = rtc.now();
  
  
  
  
}


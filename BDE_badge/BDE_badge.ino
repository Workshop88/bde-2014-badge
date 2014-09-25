#include <EEPROM.h>

#include <Wire.h>
#include <SPI.h>
#include <RTClib.h>
#include <SD.h>
#include <RH_ASK.h>
#include <Adafruit_SI1145.h>


Sd2Card card;
SdVolume volume;
SdFile root;

const int sd_cs = 10;

RTC_DS1307 rtc;

Adafruit_SI1145 sensor = Adafruit_SI1145();

int load_complete = 1; // Assume success

RH_ASK radio(4800, 2, 3);

typedef struct radio_msg_t_stct {
  uint32_t time;
  uint8_t badge_id;
  uint8_t ir_min;
  uint8_t ir_max;
  uint8_t uv_min;
  uint8_t uv_max;
  uint8_t vis_min;
  uint8_t vis_max;
  char badges[13];
} radio_msg_t;
radio_msg_t msg;

void setup()
{
  msg.badge_id = EEPROM.read(0);
  Serial.begin(9600);
  Wire.begin();
  rtc.begin();
  
  /* Needs to be set to OUTPUT for SD card to work */
  pinMode(SS, OUTPUT);
  pinMode(sd_cs, OUTPUT);
  
  if(!card.init(SPI_HALF_SPEED, sd_cs))
  {
    load_complete = 0;
    Serial.println("Error reading SD card");
  }
  
  if(!volume.init(card))
  {
    load_complete = 0;
    Serial.println("Error initializing SD volume");
  }
  
  root.openRoot(volume);
  
  if(!sensor.begin())
  {
    load_complete = 0;
    Serial.println("Error loading sensor.");
  }
  
  if(!radio.init())
  {
     load_complete = 0; 
     Serial.println("Error initializing radio.");
  }

  Serial.println("Initialization succeeded!");

}


#define UPDATE_MINMAX(v, mn, mx) do { \
  if(v < mn)                        \
  {                                 \
    mn = v;                         \
  }                                 \
  if(v > mx)                        \
  {                                 \
    mx = v;                         \
  }                                 \
} while(0)

void loop()
{
  DateTime time_now = RTC.now();
  
  uint8_t buff[100];
  uint8_t bufflen = 100;
  float vis_max = 0, ir_max = 0, uv_max = 0;
  float vis_min = 1000000.0f, ir_min = 1000000.0f, uv_min = 1000000.0f;
  float vis_val = 0, ir_val = 0, uv_val = 0;
  
  vis_val = sensor.readVisible();
  uv_val  = sensor.readUV() / 100.0f;
  ir_val  = sensor.readIR();
  UPDATE_MINMAX(vis_val, msg.vis_min, msg.vis_max);
  UPDATE_MINMAX(uv_val, msg.uv_min, msg.uv_max);
  UPDATE_MINMAX(ir_val, msg.ir_min, msg.ir_max);
  
  msg.time = time_now.unixtime();
  
  radio.send((uint8_t *)&msg, sizeof(msg));
  radio.waitPacketSent();
  
  if(radio.recv(buff, &bufflen))
  {
    int byte_off = 0;
    int bit_off = 0;
    /* We have data. */
    radio_msg_t *in_msg = (radio_msg_t *)buff;
    /* Update our bitmask with the badge we detected. */
    byte_off = in_msg->badge_id / 8;
    bit_off = in_msg->badge_id % 8;
    msg.badges[byte_off] |= 0x1 << bit_off; 
  }
 
}


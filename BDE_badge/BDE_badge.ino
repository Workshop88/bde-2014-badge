#include <EEPROM.h>

#include <Wire.h>
#include <SPI.h>
#include <RTClib.h>
#include <SD.h>
#include <RH_ASK.h>
#include <RHDatagram.h>
#include <Adafruit_SI1145.h>

Sd2Card card;
SdVolume volume;
SdFile root;

const int sd_cs = 10;

RTC_DS1307 rtc;

Adafruit_SI1145 sensor = Adafruit_SI1145();

int load_complete = 1; // Assume success

RH_ASK driver(4800, 2, 3);
RHDatagram *radio;

typedef struct radio_msg_t_stct {
  uint16_t ir_min;
  uint16_t ir_max;
  uint16_t uv_min;
  uint16_t uv_max;
  uint16_t vis_min;
  uint16_t vis_max;
  char badges[16];
  uint32_t time;
  uint8_t badge_id;
} radio_msg_t;
radio_msg_t msg;

//char outstr[100];
  
uint8_t buff[40];
uint8_t bufflen = 40;

uint8_t src =0, dst=0, id=0, flags=0;
void setup()
{
  memset(&msg, 0, sizeof(radio_msg_t));
  msg.badge_id = EEPROM.read(0);
  if(msg.badge_id == 0)
    msg.badge_id = 1;
  msg.ir_min=0xFFFF;
  msg.ir_max=0;
  msg.uv_min=0xFFFF;
  msg.uv_max=0;
  msg.vis_min=0xFFFF;
  msg.vis_max=0;
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
  radio = new RHDatagram(driver, 0x1);
  if(!radio->init())
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

float vis_val = 0, ir_val = 0, uv_val = 0;

void loop()
{
  DateTime time_now = rtc.now();

  
  ir_val  = sensor.readIR();
  vis_val = sensor.readVisible();
  uv_val  = sensor.readUV();

  UPDATE_MINMAX(vis_val, msg.vis_min, msg.vis_max);
  UPDATE_MINMAX(uv_val, msg.uv_min, msg.uv_max);
  UPDATE_MINMAX(ir_val, msg.ir_min, msg.ir_max);
  
  msg.time = time_now.unixtime();
  //snprintf(outstr, 100, "Src: %hhx Time: %u IR %hd/%hd UV %hd/%hd VIS %hd/%hd Badges %016xll%016xll\n", 
  //                msg.badge_id, 
  //                msg.time,
  //                msg.ir_min, msg.ir_max, 
  //                msg.uv_min, msg.uv_max, 
  //                msg.vis_min, msg.vis_max,
  //                *((uint64_t*)(&msg.badges[0])),
  //                *((uint64_t*)(&msg.badges[8])));
  //Serial.println(outstr);
  radio->sendto((uint8_t *)&msg, sizeof(msg), RH_BROADCAST_ADDRESS);
  radio->waitPacketSent();
  
  if(radio->recvfrom(buff, &bufflen, &src, &dst, &id, &flags))
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
  
  delay(1000);
 
}


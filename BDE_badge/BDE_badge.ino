#include <EEPROM.h>

#include <Wire.h>
#include <SPI.h>
#include <RTClib.h>
#include <SD.h>
#include <RH_ASK.h>
#include <RHDatagram.h>
#include <Adafruit_SI1145.h>

const int sd_cs = 10;

RTC_DS1307 rtc;

Adafruit_SI1145 sensor = Adafruit_SI1145();

int load_complete = 1; // Assume success
int log_available = 0;

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

typedef struct file_rec_t_stct {
   uint32_t time;
   uint16_t ir_val;
   uint16_t uv_val;
   uint16_t vis_val;
   uint8_t badge_id;
   uint8_t padding; 
} file_rec_t;
file_rec_t record;

uint8_t buff[34];
uint8_t bufflen = 34;

uint8_t src =0, dst=0, id=0, flags=0;
File logfile;

prog_char filenm_format[] PROGMEM = { "data%02d.csv" };

void setup()
{
  memset(&msg, 0, sizeof(radio_msg_t));
  msg.badge_id = EEPROM.read(0);
  if(msg.badge_id == 0xFF)
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
  
  if(!SD.begin(sd_cs))
  {
    load_complete = 0;
    Serial.println(F("Error reading SD card"));
  } else {
    Serial.println(F("Opening log file."));
    char fname[12];
    int i=0;
    do{
       snprintf_P(fname, 12, filenm_format, i);
       Serial.print(F("Trying file "));
       Serial.println(fname);
       if(!SD.exists(fname)) {
         Serial.print(F("I see no ")); Serial.print(fname); Serial.println(F(" here. Creating."));
         logfile = SD.open(fname, FILE_WRITE);
         Serial.print(F("Created new log file '"));
         Serial.print(fname); Serial.println("'");
         break;
       }
       i++;
    } while(i<100);
    if(i==100) {
      log_available = 0;
    } else { 
      log_available=1;
    }
  }
  
  if(!sensor.begin())
  {
    load_complete = 0;
    Serial.println(F("Error loading sensor."));
  }
  radio = new RHDatagram(driver, 0x1);
  if(!radio->init())
  {
     load_complete = 0; 
     Serial.println(F("Error initializing radio."));
  }

  Serial.println(F("Initialization succeeded!"));

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
  DateTime time_now = rtc.now();

  
  record.ir_val  = sensor.readIR();
  record.vis_val = sensor.readVisible();
  record.uv_val  = sensor.readUV();

  UPDATE_MINMAX(record.vis_val, msg.vis_min, msg.vis_max);
  UPDATE_MINMAX(record.uv_val, msg.uv_min, msg.uv_max);
  UPDATE_MINMAX(record.ir_val, msg.ir_min, msg.ir_max);
  
  msg.time = time_now.unixtime();
  Serial.print("Time is ");
  Serial.println(msg.time);
  record.time = msg.time;
  record.badge_id = 0;
  record.padding = 0;
  
  radio->sendto((uint8_t *)&msg, sizeof(msg), RH_BROADCAST_ADDRESS);
  radio->waitPacketSent();
  
  if(radio->recvfrom(buff, &bufflen, &src, &dst, &id, &flags))
  {
    Serial.println(F("Got data on radio."));
    /* We have data. */
    radio_msg_t *in_msg = (radio_msg_t *)buff;
    /* Update our bitmask with the badge we detected. */
    msg.badges[(in_msg->badge_id / 8)] |= 0x1 << (in_msg->badge_id % 8);
    record.badge_id = in_msg->badge_id;
  }
  
  /* TODO: Write out record to file. */
  if(log_available) {
    logfile.print(record.time);
    logfile.print(",");
    logfile.print(record.ir_val);
    logfile.print(",");
    logfile.print(record.uv_val);
    logfile.print(",");
    logfile.print(record.vis_val);
    logfile.print(",");
    if(record.badge_id == 0)
    {
      logfile.print(F("NONE"));
    } else {
      logfile.print(record.badge_id);
    }
    logfile.println();
    Serial.println(F("Flushing."));
    logfile.flush();
    Serial.println(F("Flushed."));
  }
  delay(1000);
 
}


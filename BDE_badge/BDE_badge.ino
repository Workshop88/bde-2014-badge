#include <EEPROM.h>

#include <Wire.h>
#include <SPI.h>
#include <RTClib.h>
#include <SD.h>
#include <RH_ASK.h>
#include <RHDatagram.h>
#include <Adafruit_SI1145.h>

#define DEBUG 1

const int sd_cs = 10;

RTC_DS1307 rtc;

Adafruit_SI1145 sensor = Adafruit_SI1145();

int load_complete = 1; // Assume success
int log_available = 0;

RH_ASK driver(2400, 2, 3);
RHDatagram *radio;

typedef struct radio_msg_t_stct {
  uint16_t ir_min;
  uint16_t ir_max;
  uint16_t uv_min;
  uint16_t uv_max;
  uint16_t vis_min;
  uint16_t vis_max;
  //char badges[32];
  uint32_t time;
  uint8_t badge_id;
  uint8_t infector;
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

uint8_t buff[20];
uint8_t bufflen = 20;

//uint8_t src =0, dst=0, id=0, flags=0;
File logfile;

uint32_t next_send = 0, next_sense = 0;
const prog_char filenm_format[] PROGMEM = { "data%02d.csv" };
char * id_file = "id.txt";
char fname[12];

#define get_next_send(out) do { \
  long mil = millis(); \
  long val = random(10000) + 20000; \
  Serial.print(F("Random number in get_next_send is ")); \
  Serial.println(val); \
  Serial.println(mil); \
  out = mil + val; \
} while(0) 

#define get_next_sense(out) do { \
        out = millis() + random(10000,15000); \
} while(0)

void setup()
{
  get_next_send(next_send);
  get_next_sense(next_sense);
  memset(&msg, 0, sizeof(radio_msg_t));
  memset(buff, 0, bufflen);
  msg.badge_id = 0;
  msg.ir_min=0xFFFF;
  msg.ir_max=0;
  msg.uv_min=0xFFFF;
  msg.uv_max=0;
  msg.vis_min=0xFFFF;
  msg.vis_max=0;
  msg.infector = 0;
  Serial.begin(9600);
  Wire.begin();
  rtc.begin();
  
  /* Needs to be set to OUTPUT for SD card to work */
  pinMode(SS, OUTPUT);
  pinMode(sd_cs, OUTPUT);
 

  
  if(!SD.begin(sd_cs))
  {
    load_complete = 0;
#if DEBUG
    Serial.println(F("Error reading SD card"));
#endif
  } else {
#if DEBUG
    Serial.println(F("Reading id.txt."));
#endif
    if(SD.exists(id_file)) {
      int i=0;
      logfile = SD.open(id_file, FILE_READ);
      while((i<bufflen) && ((buff[i] = logfile.read()) != -1))
      {
        i++;
      }
      msg.badge_id = atoi((char *)buff);
      EEPROM.write(0, msg.badge_id);
      logfile.close();
    }

#if DEBUG
    Serial.println(F("Opening log file."));
#endif

    int i=0;
    do{
       snprintf_P(fname, 12, filenm_format, i);
#if DEBUG
       Serial.print(F("Trying file "));
       Serial.println(fname);
#endif
       if(!SD.exists(fname)) {
#if DEBUG
         Serial.print(F("I see no ")); Serial.print(fname); Serial.println(F(" here. Creating."));
#endif
         logfile = SD.open(fname, FILE_WRITE);
#if DEBUG
         Serial.print(F("Created new log file '"));
         Serial.print(fname); Serial.println(F("'"));
#endif
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

  if(msg.badge_id == 0)
  {
    /* Still no badge id, so no id file. */
    msg.badge_id = EEPROM.read(0);
    if(msg.badge_id == 0xFF)
    {
      /* Nothing in EEPROM, so we're a new badge with no file on the SD card... */
      msg.badge_id = random(101, 127);
    }
  }

#if DEBUG
  Serial.print(F("I am badge "));
  Serial.println(msg.badge_id);
#endif

  
  if(!sensor.begin())
  {
    load_complete = 0;
#if DEBUG
    Serial.println(F("Error loading sensor."));
#endif
  }
  radio = new RHDatagram(driver, msg.badge_id);
  if(!radio->init())
  {
     load_complete = 0; 
#if DEBUG
     Serial.println(F("Error initializing radio."));
#endif
  }
#if DEBUG
  Serial.println(F("Initialization succeeded!"));
#endif

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
  int writeFile = 0;

  bufflen = sizeof(buff);
  if(radio->recvfrom(buff, &bufflen, NULL, NULL, NULL, NULL))
  {
    writeFile = 1;
#if DEBUG
    Serial.println(F("Got data on radio."));
#endif
    /* We have data. */
    radio_msg_t *in_msg = (radio_msg_t *)buff;
    /* Update our bitmask with the badge we detected. */
    if(in_msg->badge_id == 254) {
      /* time adjust */
      Serial.print("Adjust to ");
      Serial.println(in_msg->time);
      rtc.adjust(DateTime(in_msg->time));
    } else if(in_msg->badge_id & 0x80)
    {
      /* I'm now infected. */
      msg.badge_id |= 0x80;
      msg.infector = in_msg->badge_id;
    }
    record.badge_id = in_msg->badge_id;
  }

  DateTime time_now = rtc.now();

  msg.time = time_now.unixtime();

  if(writeFile || (millis() > next_sense)) {
    writeFile = 1;
    record.ir_val  = sensor.readIR();
    record.vis_val = sensor.readVisible();
    record.uv_val  = sensor.readUV();
    UPDATE_MINMAX(record.vis_val, msg.vis_min, msg.vis_max);
    UPDATE_MINMAX(record.uv_val, msg.uv_min, msg.uv_max);
    UPDATE_MINMAX(record.ir_val, msg.ir_min, msg.ir_max);
    get_next_sense(next_sense);
  }


  record.time = msg.time;
  record.badge_id = 0;
  record.padding = 0;
  
  if(millis() > next_send) {
    Serial.print(F("About to send. "));
    Serial.print(millis());
    Serial.print(F(" "));
    Serial.println(next_send);
    for(int i =0;i<3;i++) {
      radio->sendto((uint8_t *)&msg, sizeof(msg), RH_BROADCAST_ADDRESS);
      radio->waitPacketSent();
      delay(random(500, 800));
    }
    get_next_send(next_send);
    Serial.println(F("Done sending."));
    Serial.print(millis());
    Serial.print(F(" "));
    Serial.println(next_send);
  }
  
  
  /* TODO: Write out record to file. */
  if(log_available && writeFile) {
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
#if DEBUG
    Serial.println(F("Flushing."));
   Serial.print(F("Time is "));
   Serial.println(msg.time);
#endif
    logfile.flush();
#if DEBUG
    Serial.println(F("Flushed."));
#endif
  }
 
}


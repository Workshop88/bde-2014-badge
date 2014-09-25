// ask_receiver.pde
// -*- mode: C++ -*-
// Simple example of how to use RadioHead to receive messages
// with a simple ASK transmitter in a very simple way.
// Implements a simplex (one-way) receiver with an Rx-B1 module

#include <RH_ASK.h>
#include <RHDatagram.h>
#include <SPI.h> // Not actualy used but needed to compile
typedef struct radio_msg_t_stct {
  uint32_t time;
  uint16_t ir_min;
  uint16_t ir_max;
  uint16_t uv_min;
  uint16_t uv_max;
  uint16_t vis_min;
  uint16_t vis_max;
  char badges[16];
  uint8_t badge_id;
} radio_msg_t;
RH_ASK driver(4800, 2, 3);
RHDatagram *radio;

void setup()
{
    Serial.begin(9600);	// Debugging only
    radio = new RHDatagram(driver, 200);
    radio->init();
}

void loop()
{
    uint8_t buf[40];
    uint8_t buflen = sizeof(buf);
    uint8_t src_addr = 0;
    uint8_t dst_addr = 0;
    uint8_t id = 0;
    uint8_t flags = 0;
    memset(buf, 0, 40);
    

    if (radio->recvfrom(buf, &buflen, &src_addr, &dst_addr, &id, &flags)) // Non-blocking
    {
	int i;
        char outstr[100];
	// Message with a good checksum received, dump it.
	//driver.printBuffer("Got:", buf, buflen);
        if(buflen != sizeof(radio_msg_t))
        {
          Serial.print("Buffer mismatch.  got ");
          Serial.print(buflen);
          Serial.print(" expected ");
          Serial.println(sizeof(radio_msg_t));
        }
        radio_msg_t *msg = (radio_msg_t *)buf;
        i = msg->badge_id;
        snprintf(outstr, 100, "Src: %hhx Time: %u IR %hd/%hd UV %hd/%hd VIS %hd/%hd Badges %016xll%016xll\n", 
                  msg->badge_id, 
                  msg->time,
                  msg->ir_min, msg->ir_max, 
                  msg->uv_min, msg->uv_max, 
                  msg->vis_min, msg->vis_max,
                  *((uint64_t*)(msg->badges)),
                  *((uint64_t*)(&msg->badges[8])));
        
        Serial.println(outstr);
        driver.printBuffer("Got:", buf, buflen);
    }
}

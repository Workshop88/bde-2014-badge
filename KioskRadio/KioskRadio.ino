// ask_receiver.pde
// -*- mode: C++ -*-
// Simple example of how to use RadioHead to receive messages
// with a simple ASK transmitter in a very simple way.
// Implements a simplex (one-way) receiver with an Rx-B1 module

#include <RH_ASK.h>
#include <RHDatagram.h>
#include <SPI.h> // Not actualy used but needed to compile
typedef struct radio_msg_t_stct {
  uint16_t ir_min;
  uint16_t ir_max;
  uint16_t uv_min;
  uint16_t uv_max;
  uint16_t vis_min;
  uint16_t vis_max;
//  char badges[32];
  uint32_t time;
  uint8_t badge_id;
  uint8_t infector;
} radio_msg_t;
RH_ASK driver(2400, 2, 3);
RHDatagram *radio;

void setup()
{
    Serial.begin(9600);	// Debugging only
    radio = new RHDatagram(driver, 200);
    radio->init();
}

void loop()
{
    uint8_t buf[50];
    uint8_t buflen = sizeof(buf);
    uint8_t src_addr = 0;
    uint8_t dst_addr = 0;
    uint8_t id = 0;
    uint8_t flags = 0;
    memset(buf, 0, 50);
    

    if (radio->recvfrom(buf, &buflen, &src_addr, &dst_addr, &id, &flags)) // Non-blocking
    {
	int i;
        //char outstr[100];
	// Message with a good checksum received, dump it.
	//driver.printBuffer("Got:", buf, buflen);
        if(buflen != sizeof(radio_msg_t))
        {
          Serial.print("Buffer mismatch.  got ");
          Serial.print(buflen);
          Serial.print(" expected ");
          Serial.println(sizeof(radio_msg_t));
        }
        radio_msg_t *msg = (radio_msg_t *)&buf[0];
        i = msg->badge_id;
        Serial.print(msg->badge_id);
        Serial.print(",");
        Serial.print(msg->time);
        Serial.print(",");
        Serial.print(msg->ir_min);
        Serial.print(",");
        Serial.print(msg->ir_max);
        Serial.print(",");
        Serial.print(msg->uv_min);
        Serial.print(",");
        Serial.print(msg->uv_max);
        Serial.print(",");
        Serial.print(msg->vis_min);
        Serial.print(",");
        Serial.print(msg->vis_max);
        Serial.print(",");
        Serial.print(msg->infector);
        /*Serial.print(",");
        for(i=0;i<32;i++)
        {
          Serial.print(msg->badges[i]&0xF0>>4, HEX);
          Serial.print(msg->badges[i]&0xF, HEX);
        }*/
        Serial.println("");
    }
}

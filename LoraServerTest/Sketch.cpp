
#include "LoRaService.h"

void setup() {

Serial.begin(9600);
while(!Serial);

pinMode(13, OUTPUT);

if(LoRaService.initRadio()==-1)
{
	LoRaService.sendSerial('3','1');
}
}

void loop() {


if (Serial.available()) {
   LoRaService.serialEventHandler(); 
    delayMicroseconds(10);  
  } 


analogWrite(13, 255);
delay(200);
analogWrite(13,0);
delay(200);

}

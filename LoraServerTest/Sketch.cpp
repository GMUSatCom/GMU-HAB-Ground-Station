/*Begining of Auto generated code by Atmel studio */
#include <Arduino.h>

/*End of auto generated code by Atmel studio */

/*
 * RadioServer.cpp
 *
 * Created: 10/10/2019 6:24:09 PM
 * Author : Vaughn Nugent 
 */ 

#include "LoRaService.h"

char mess[8] = {"Message"};

void setup(){
	
	pinMode(13, OUTPUT);
	
	Serial.begin(BAUDRATE);
	while(!Serial);
	
	
	if(initRadio()==-1)
    {
		sendSerial('3','1');
	}
	
}


void loop(){
	
	if (Serial.available()) {
		serialEventHandler();	
		delayMicroseconds(10);	
	}		
	
	
	delay(200);
	analogWrite(13,0);
	delay(200);
	analogWrite(13,255);	 

}


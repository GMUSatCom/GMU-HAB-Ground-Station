
/*
 * RadioServer.cpp
 *
 * Created: 10/10/2019 6:24:09 PM
 * Author : Vaughn Nugent 
 * Base sketch to operate the service lib 
 */ 

#include "LoRaService.h"

void setup(){
	
	pinMode(13, OUTPUT);
	
	Serial.begin(9600);
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


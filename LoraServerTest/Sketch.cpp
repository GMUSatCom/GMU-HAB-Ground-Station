/*
 * RadioServer.cpp
 *
 * Created: 10/10/2019 6:24:09 PM
 * Author : Vaughn Nugent 
 * Base sketch to operate the service lib 
 */ 

#include <Arduino.h>
//#include "LoRaService.h"


void setup(){
	
	pinMode(13, OUTPUT);
	
	//Serial.begin(9600);
	//while(!Serial);
	
	
	//if(LoRaService.initRadio()==-1)
    //{
		//LoRaService.sendSerial('3','1');
	//}
	//
}


void loop(){
	
	//if (Serial.available()) {
		////LoRaService.serialEventHandler();	
		//delayMicroseconds(10);	
	//}		
	
	
	delay(200);
	analogWrite(13,0);
	delay(200);
	analogWrite(13,255);	 

}


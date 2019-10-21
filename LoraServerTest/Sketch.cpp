
#include "LoRaService.h"

void onReceive(int packetSize)
{
	LoRaService.callReceive(packetSize);
}

void setup() {

	Serial.begin(115200);
	while(!Serial);

	pinMode(13, OUTPUT);

	if(!LoRaService.initRadio())
	{
		LoRaService.sendSerial('3','1');
	}
	LoRaService.sendSerial('2','0');
	LoRa.onReceive(onReceive);

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

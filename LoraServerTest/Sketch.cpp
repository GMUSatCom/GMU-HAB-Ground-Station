#include "LoRaService.h"

/*
 * Sketch.cpp
 *
 * Created: 10/20/2019 2:59:19 PM
 * Author: Vaughn Nugent
 * 
 * Example sketch for using the LoRaServiceClass to manage the RFM95 radio
 * and serial communications. 
 */

const long BAUDRATE = 115200;
const int SERIALDELAY = 20; // MS to delay the serial event check

void setup() 
{

	//Begin Serial
	Serial.begin(BAUDRATE);
	while(!Serial);

	// Start the radio
	if(!LoRaService.initRadio())
	{
		pinMode(13,OUTPUT);
		LoRaService.sendSerial(LoRaService.ERR, LoRaService.lora_init_err);
		while(1) // Failed, do nothing
		{
			analogWrite(13,255);
			delay(200);
			analogWrite(13,255);
			delay(200);
			
		}
	}
	LoRaService.sendSerial(LoRaService.OKAY, LoRaService.no_err);
	
	//Submit the callback function for radio handler
	LoRa.onReceive(onReceive);
}

void loop() {
	
	delay(SERIALDELAY); // How long to delay checking of the serial rx
}


/*
  SerialEvent occurs whenever a new data comes in the hardware serial RX. This
  routine is run between each time loop() runs, so using delay inside loop can
  delay response. Multiple bytes of data may be available.
*/
void serialEvent()
{
	LoRaService.serialEventHandler();	
}

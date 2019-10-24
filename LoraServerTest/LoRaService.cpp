
/*
 * LoRaService.cpp
 *
 * Created: 10/11/2019 10:54:14 PM
 * Author : Vaughn Nugent
 * Description: Program to configure and communicate with a LoRa radio, using the Sandeepmistry library for Arduino.
 * this package relies on, and is compiled with the standard Arduino 1.8 Libraries for the ATMEGA2560 (Ardunio Mega)
 * this package expects you to modify your hardware serial TX and RX buffers to match the size of the Serial packet 
 * size for best stability, performance and efficiency.
 */ 

#include "LoRaService.h"

void LoRaService::setHwPins(int enable_pin, int reset_pin, int interrupt_pin )
{
  _enable = enable_pin;
  _resetPin =  reset_pin;
  _interruptPin = interrupt_pin;
}

void LoRaService::setRadioConfigArr(long freq, int power, int sf, long bw, int crate, long preamble, int sword, bool async )
{
	// Should do checks here 
	_frequency = freq;
	_tx_power = power;
	_spreading_factor = sf;
	_sig_bandwidth = bw;
	_coding_rate = crate;
	_lora_preamble = preamble;
	_sync_word = sword;
	_async_mode = async;
	
}
int LoRaService::setRadioConfigArr(char* data, int size)
{
	if( size< 21)
		return -1;
	
	int i =0; 
	
	_frequency= data[i++] | (data[i++] << 8) | (data[i++] << 16) | (data[i++] << 24)	;
	
	_tx_power = data[i++] | (data[i++] << 8) ;
	
	_spreading_factor =  data[i++] | (data[i++] << 8);
	
	_sig_bandwidth = data[i++] | (data[i++] << 8) | (data[i++] << 16) | (data[i++] << 24);
	
	_coding_rate =  data[i++] | (data[i++] << 8) ;
	
	_lora_preamble = data[i++] | (data[i++] << 8) | (data[i++] << 16) | (data[i++] << 24);
	
	_sync_word =  data[i++] | (data[i++] << 8) ;
	
	_async_mode =  data[i++];
	
	return 1;	
}

//Change the destination id 
void LoRaService::setDestId(char id)
{
	DESTID = id;	
}

void LoRaService::setHwId(char id)
{
	HWID = id;	
}


/*************************************************************/
/*                    RADIO SECTION                          */
/*************************************************************/


int LoRaService::initRadio()
{
  LoRa.setPins(_enable, _resetPin, _interruptPin); // configure pinout 
 
 // Set LoRa values based on the globals configured above
  LoRa.setTxPower(_tx_power);
  LoRa.setFrequency(_frequency);
  LoRa.setSpreadingFactor(_spreading_factor);
  LoRa.setSignalBandwidth(_sig_bandwidth);
  LoRa.setCodingRate4(_coding_rate);
  LoRa.setPreambleLength(_lora_preamble);
  LoRa.setSyncWord(_sync_word);
  //LoRa.onReceive(onReceive);
  

  if (!LoRa.begin(_frequency))
  {           
    return -1;
  }
  else
  {   //Enter continuous receive mode
	  LoRa.receive();
	  return 1;
  }
  
}

/********* TRANSMIT **************/
//Creates and sends a lora packet, errors are 'handled' and status is updated and send to the serial uplink 
int LoRaService::sendLoraPacket(char* data, int data_size)
{
  char lora_packet[RADIOPACKETSIZE] =  {0};  
  int start = 0;
  
  lora_packet[start++] = DESTID; // Start with dest id 
  
  // Make sure the data buffer isn't too large 
  if(data_size> RADIOPACKETSIZE -1 )
  {
	  data_size = RADIOPACKETSIZE - 1; //Set the data size to the packet size so memcpy will truncate the rest of the data buffer
  }

  // Copy contents from data buffer to the lora packet buffer shifted 1 because of the dest id header
  for(int i = 0; i< data_size; i++)
  {
	  lora_packet[start++] = data[i];
  }  
  
  if(LoRa.beginPacket() == 0)
  {
   return -1;    
  }
    
  // write lora_packet buffer to the radio buffer
  int sent_bytes = LoRa.write((byte *)lora_packet, RADIOPACKETSIZE); 

  if(LoRa.endPacket(_async_mode) == 0)
  {    
    return -1; 
  } 
  
  //Clear the buffers by idle and re-init 
  LoRa.idle();
  initRadio();

  return sent_bytes; 
  
}

/***************** Recieve ***************/
// Forward a message to the serial output from the radio buffer immediately 
void LoRaService::onReceive(int packetSize) {
	if(packetSize<4)          // if there's no packet, return
	{
		sendSerial(WARNING, lora_receive_err);
		LoRa.flush();
	} 
	
	char serial_message[RADIOPACKETSIZE] ={0};  //create the message portion of the serial packet
	  
	// Get stats from packet
	_last_packet_rssi = LoRa.packetRssi(); //rssi
	_last_packet_snr = LoRa.packetSnr();   //snr
	_last_packet_freq_err = LoRa.packetFrequencyError();
	  
	//First 2 bytes are header info
	char recipient = LoRa.read();          // recipient address
	char sender = LoRa.read();            // sender address	   
     
	//make sure the device ids are correct 
	if(sender != DESTID || recipient !=HWID)
	{
		char ids[2] = {sender,recipient};
		//Message is not for this device exit
		sendSerialData(WARNING, lora_receive_err, ids, 2);
		return;
	}
	
	/* This packet includes the last 2 bytes of the header, 
	 * the first 2 bytes of the header will include the message ID and 
	 * message length, the rest is data. 
	 * the buffer is NOT null terminated.
	 */
	//Capture payload
	while(LoRa.available())
	{	int sm=0;
		serial_message[sm++] = LoRa.read();
	}

	//Write our message to serial immediately 
	sendSerialData(OKAY, 0, serial_message, RADIOPACKETSIZE);

	//Clear the buffers by idle and re-init 
	LoRa.idle();
	initRadio(); 
  
}


/*************************************************************/
/*                    SERIAL SECTION                         */
/*************************************************************/

int LoRaService::sendSerialData(char header1, char header2, char* data, int data_size)
{
  
   if(data_size>= (SERIALPACKETSIZE - 2) || data_size < 1)
   {
	   //message is too large to send in a serial packet
	   return -1;
   }
  
  char serial_packet[SERIALPACKETSIZE]={0}; 
  int msg_start = 0;

  //Insert headers
  serial_packet[msg_start++] = header1;
  serial_packet[msg_start++] = header2; 
  
 for(int i = 0; i<data_size; i++)
 {
	serial_packet[msg_start++] = data[i];	 
 }
 
  int sent = Serial.write(serial_packet, SERIALPACKETSIZE);//send serial packet

  return sent;
}

int LoRaService::sendSerial(char header1, char header2)
{
  char serial_packet[2] = {0};  
  int msg_start = 0;

  //Insert headers
  serial_packet[msg_start++] = header1;
  serial_packet[msg_start++] = header2;

  // Write the data to the TX Buff
  int sent = Serial.write(serial_packet, sizeof(serial_packet));
 
  // Returns the number of bytes sent
  return sent;
  
}

/*************** Recived Serial **************************/
void LoRaService::serialEventHandler()
{       
	if(Serial.available()<2)
	{
		Serial.flush();  // Cant use it, flush serial buffer
		return;
	}
 
  // Make sure the buffer wont be too large to stuff it into a radio packet if needed 
  int buff_size;
  if(Serial.available()> RADIOPACKETSIZE+2)
  {
	  buff_size = RADIOPACKETSIZE + 2;
  }
  else
  {
	 buff_size = Serial.available();
  }
  char buff[buff_size]={0};
  
  // Consume all available bytes to a buffer 
  Serial.readBytes(buff, buff_size);
  
  //Take the first 2 bytes and use the first as an id check or other, and the instruction  
  char device_id = buff[0];
  char instruction = buff[1];
  char data[buff_size-2]={0};
	for (int i =0; i< buff_size-2; i++)
	{
		data[i] = buff[i+2];				
	}

  commandHandler(instruction, data, (buff_size-2));
  
  // Flush the serial buffer when we are done because were not sure if we can use it later 
  Serial.flush();
}



/*************************************************************/
/*                   COMMAND SECTION                         */
/*************************************************************/

int LoRaService::commandHandler(char instruction, char* data, int data_size)
{
    switch(instruction)
	{		
		case IDLERD :               // Instruction to idle radio
		{
			LoRa.idle();
			return 1;		
		}
    
		case SLEEPRD :              // Instruction to sleep
		{
			LoRa.sleep();
			return 1;		
		}
	
	   case SETRD :                // Instruction to modify radio
	   { 
			// Initialize the radio with new settings			
			if(setRadioConfigArr(data,data_size) == -1 && initRadio()==-1)            
			{
			  sendSerial(ERR, lora_init_err); // There was an init error, return error code  
			  return -1; 
			}	
			return 1;	                    
		}
		
		case SEND :                   // Instructions to send message
		{
			if(sendLoraPacket(data, data_size)==-1);
			{
			  sendSerial(ERR, lora_send_err);  // There was an error, return error code   
			  return -1; 
			}		
			return 1;	     
		}
    
		// Send the data back since we couldn't do anything with it 
		default :
		{		
			 sendSerialData(OKAY, no_err, data, data_size);	
			 return 1;	     
		}  
	
	}
	return 1;   
}

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

//Hardware const start
int enable = 7;             // LoRa radio chip select
int resetPin = 6;           // LoRa radio reset
int interruptPin = 1;       // change for your board; must be a hardware interrupt pin

//Radio Vars
byte DESTID = 0xFF;           // destination to send to

long frequency = 915E6;       // Set Operating frequency
int tx_power = 23;            // Transmit Power level
int spreading_factor = 12;    // Spreading Factor
long sig_bandwidth = 15.6E3;  // Signal Bandwidth
int coding_rate = 8;          // Coding Rate denominator
long lora_preamble = 10;      // Set lora radio preamble length
int sync_word = 0;            // Lora Sync Word
bool async_mode = false;      // Packet send in async mode

//Captured Packet info
int last_packet_rssi;         // Save the last packets rssi
float last_packet_snr;        // Save last packets s/n ratio
long last_packet_freq_err;    // Records the last packets frequency error in hz (look at documentation)


void setHwPins(int enable_pin, int reset_pin, int interrupt_pin )
{
  enable = enable_pin;
  resetPin =  reset_pin;
  interruptPin = interrupt_pin;
}

void setRadioConfig(long freq, int power, int sf, long bw, int crate, long preamble, int sword, bool async )
{
	// Should do checks here 
	frequency = freq;
	tx_power = power;
	spreading_factor = sf;
	sig_bandwidth = bw;
	coding_rate = crate;
	lora_preamble = preamble;
	sync_word = sword;
	async_mode = async;
	
}
int setRadioConfig(char* data, int size)
{
	if( size< 21)
		return -1;
	
	int i =0; 
	
	frequency= data[i++] | (data[i++] << 8) | (data[i++] << 16) | (data[i++] << 24)	;
	
	tx_power = data[i++] | (data[i++] << 8) ;
	
	spreading_factor =  data[i++] | (data[i++] << 8);
	
	sig_bandwidth = data[i++] | (data[i++] << 8) | (data[i++] << 16) | (data[i++] << 24);
	
	coding_rate =  data[i++] | (data[i++] << 8) ;
	
	lora_preamble = data[i++] | (data[i++] << 8) | (data[i++] << 16) | (data[i++] << 24);
	
	sync_word =  data[i++] | (data[i++] << 8) ;
	
	async_mode =  data[i++];
	
	return 1;	
}

//Change the destination id 
void setDestId(byte id)
{
	DESTID = id;	
}

/*************************************************************/
/*                    RADIO SECTION                          */
/*************************************************************/


int initRadio()
{
  LoRa.setPins(enable, resetPin, interruptPin); // configure pinout 
 
 // Set LoRa values based on the globals configured above
  LoRa.setTxPower(tx_power);
  LoRa.setFrequency(frequency);
  LoRa.setSpreadingFactor(spreading_factor);
  LoRa.setSignalBandwidth(sig_bandwidth);
  LoRa.setCodingRate4(coding_rate);
  LoRa.setPreambleLength(lora_preamble);
  LoRa.setSyncWord(sync_word);
  LoRa.onReceive(onReceive);

  if (!LoRa.begin(frequency))
  {           
    return -1;
  }
  else
  return 1;
}

/********* TRANSMIT **************/
//Creates and sends a lora packet, errors are 'handled' and status is updated and send to the serial uplink 
int sendLoraPacket(char* data, int data_size)
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

  if(LoRa.endPacket(async_mode) == 0)
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
void onReceive(int packetSize) {
  if(LoRa.available()<2)          // if there's no packet, return
	{
		LoRa.flush();
	} 
	
  char serial_message[RADIOPACKETSIZE] ={0};  //create the message portion of the serial packet
  
  // Get stats from packet
  last_packet_rssi = LoRa.packetRssi(); //rssi
  last_packet_snr = LoRa.packetSnr();   //snr
  last_packet_freq_err = LoRa.packetFrequencyError();
  
  //make sure the device ids are correct 
  if(LoRa.read() != DESTID)
  {
    //Message is not for this device exit
    return;
  }
 
	LoRa.readBytes(serial_message, RADIOPACKETSIZE);

  //Write our message to serial immediately 
  sendSerial(OKAY, 0, serial_message, RADIOPACKETSIZE);

 //Clear the buffers by idle and re-init 
 LoRa.idle();
 initRadio(); 
  
}


/*************************************************************/
/*                    SERIAL SECTION                         */
/*************************************************************/

int sendSerial(char header1, char header2, char* data, int data_size)
{
  
   if(data_size>= (SERIALPACKETSIZE - msg_start) || data_size < 1)
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

int sendSerial(char header1, char header2)
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
void serialEventHandler()
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

int commandHandler(char instruction, char* data, int data_size)
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
			if(setRadioConfig(data,data_size) == -1 && initRadio()==-1)            
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
			 sendSerial(OKAY, no_err, data, data_size);	
			 return 1;	     
		}  
	
	}
	return 1;   
}